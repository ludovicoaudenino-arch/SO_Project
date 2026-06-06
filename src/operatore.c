/**
 * @file operatore.c
 * @brief Main entry point for the worker (operatore) process.
 *
 * Each operator is assigned to a station (primi, secondi, coffee, or cassa).
 * They compete for a seat at their station, process customer orders from
 * the message queues, simulate service times, handle breaks (pauses),
 * and record simulation statistics.
 */

#include "common.h"
#include "ipc_utils.h"
#include "shared_data.h"
#include "time_utils.h"
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/sem.h>
#include <time.h>
#include <unistd.h>

static int shm_id;             /**< @brief Shared memory identifier for System V segment */
static int target_station;     /**< @brief Index of the station assigned to this operator */
static unsigned int seed;      /**< @brief Thread-safe local random seed for rand_r */
static int nof_pause;          /**< @brief Remaining breaks (pauses) the operator can take today */
int service_time;              /**< @brief Active service time for the current customer (simulated seconds) */

/**
 * @brief Signal handler for SIGUSR1.
 *
 * Interrupts blocking message queue operations (msgrcv) to allow graceful check
 * of simulation_running state.
 *
 * @param sig Signal number (unused).
 */
static void handle_signal(int sig) { (void)sig; }

/**
 * @brief Configures signal action for SIGUSR1.
 */
static void set_sigaction() {
  struct sigaction sa = {.sa_handler = handle_signal, .sa_flags = 0};
  sigemptyset(&sa.sa_mask);
  sigaction(SIGUSR1, &sa, NULL);
}

/**
 * @brief Parses command-line arguments to retrieve shared memory ID and target station index.
 *
 * Exits with EXIT_FAILURE if arguments are insufficient.
 *
 * @param argc Count of command line arguments.
 * @param argv Vector of command line arguments.
 */
static void parse_arguments(int argc, char *argv[]) {
  if (argc < 3) {
    fprintf(stderr, "NOT ENOUGH ARGUMENTS\n");
    exit(EXIT_FAILURE);
  }
  shm_id = atoi(argv[1]);
  target_station = atoi(argv[2]);
  if (target_station < 0 || target_station >= NUM_STATIONS) {
    perror("ERROR ARGUMENT 2: TOO MANY STATIONS\n");
    exit(EXIT_FAILURE);
  }
}

/**
 * @brief Serves a first or second course dish to a user.
 *
 * Decrements the portions left for the requested dish if available, simulates service
 * time via sim_sleep, updates stats, and sends a ServedMsg reply to the user.
 * Bypasses sleep and returns served = 0 immediately if the dish is out of portions.
 *
 * @param shm Pointer to the SharedData structure.
 * @param order Pointer to the ServingMsg request.
 */
static void serve_primi_secondi(struct SharedData *shm, ServingMsg *order) {
  int sem_id = shm->sem_id;
  int random_srvc_time =
      random_service_time(shm->stations[target_station].avg_srvc,
                           shm->stations[target_station].srvc_delta);
  int dish_type = order->dish_type;
  int served = 0;
  int portion_mutex =
      (target_station == 0) ? SEM_MUTEX_PORZIONI_P : SEM_MUTEX_PORZIONI_S;

  sem_op(sem_id, portion_mutex, -1, SEM_UNDO);
  if (shm->stations[target_station].portion_left[dish_type] > 0) {
    shm->stations[target_station].portion_left[dish_type]--;
    sem_op(sem_id, portion_mutex, +1, SEM_UNDO);
    sim_sleep(random_srvc_time, shm->config.n_nano_secs);
    served = 1;
    stats_record_dish_served(&shm->sim_stats, sem_id, target_station);
  } else {
    sem_op(sem_id, portion_mutex, +1, SEM_UNDO);
  }

  ServedMsg reply;
  reply.mytype = order->pid;
  reply.served = served;
  reply.pid = getpid();
  int send_result = send_message(shm->msg_queue_list[NUM_QUEUES - 1], &reply,
                                 MSG_CONTENT_SIZE(ServedMsg), 0);
  if (send_result == -1) {
    return;
  }
}

/**
 * @brief Serves a coffee/dessert to a user.
 *
 * Simulates service time, replies with success (served = 1), and records stats.
 * Coffee portions are assumed to be unlimited.
 *
 * @param shm Pointer to the SharedData structure.
 * @param order Pointer to the ServingMsg request.
 */
static void serve_caffe(struct SharedData *shm, ServingMsg *order) {
  int sem_id = shm->sem_id;
  int random_srvc_time =
      random_service_time(shm->stations[target_station].avg_srvc,
                           shm->stations[target_station].srvc_delta);

  sim_sleep(random_srvc_time, shm->config.n_nano_secs);
  ServedMsg reply;
  reply.mytype = order->pid;
  reply.served = 1;
  reply.pid = getpid();
  int send_result = send_message(shm->msg_queue_list[NUM_QUEUES - 1], &reply,
                                 MSG_CONTENT_SIZE(ServedMsg), 0);
  if (send_result == -1) {
    return;
  }
  stats_record_dish_served(&shm->sim_stats, sem_id, STATION_COFFEE);
}

/**
 * @brief Processes payment for a user at the checkout cassa.
 *
 * Calculates the total cost based on consumed dishes and configured prices,
 * records the revenue in stats, simulates cashier processing time, and replies.
 *
 * @param shm Pointer to the SharedData structure.
 * @param order Pointer to the OrderMsg detailing purchased dishes.
 */
static void serve_cassa(struct SharedData *shm, OrderMsg *order) {
  int sem_id = shm->sem_id;
  int random_srvc_time =
      random_service_time(shm->stations[target_station].avg_srvc,
                           shm->stations[target_station].srvc_delta);

  float total_primi_price = order->primi_ordered * shm->config.price_primi;
  float total_secondi_price =
      order->secondi_ordered * shm->config.price_secondi;
  float total_coffee_price = order->coffee_ordered * shm->config.price_coffee;

  float total = total_primi_price + total_secondi_price + total_coffee_price;

  stats_record_revenue(&shm->sim_stats, sem_id, total);

  sim_sleep(random_srvc_time, shm->config.n_nano_secs);
  ServedMsg reply;
  reply.mytype = order->pid;
  reply.served = 1;
  reply.pid = getpid();
  int send_result = send_message(shm->msg_queue_list[NUM_QUEUES - 1], &reply,
                                 MSG_CONTENT_SIZE(ServedMsg), 0);
  if (send_result == -1) {
    return;
  }
}

/**
 * @brief Blocks waiting for an incoming order on the station's message queue.
 *
 * @param shm Pointer to the SharedData structure.
 * @param order Output pointer to the message structure.
 * @param target_station Index of the message queue/station.
 * @param msg_size Size of the message payload.
 * @return 1 on successful message receipt, 0 on failure (interrupted or queue removed).
 */
static int wait_order(struct SharedData *shm, void *order, int target_station,
                      size_t msg_size) {
  int result =
      receive_message(shm->msg_queue_list[target_station], order, msg_size,
                      shm->stations[target_station].msg_type, 0);
  if (result == -1) {
    return 0;
  }

  return 1;
}

/**
 * @brief Simulates taking a worker break (pause).
 *
 * Operators only take a break if there is at least one other active operator at the station.
 * Relinquishes the station seat, updates stats, sleeps for the break duration,
 * and re-acquires the seat before resuming.
 *
 * @param shm Pointer to the SharedData structure.
 * @param target_station Index of the food or cassa station.
 */
static void go_pause(struct SharedData *shm, int target_station) {
  sem_op(shm->sem_id, SEM_MUTEX_SHM, -1, SEM_UNDO);
  if (shm->stations[target_station].active_operators > 1 && nof_pause > 0 &&
      shm->day_running) {
    nof_pause--;
    shm->stations[target_station].active_operators--;

    stats_record_pause(&shm->sim_stats, shm->sem_id);

    sem_op(shm->sem_id, SEM_MUTEX_SHM, +1, SEM_UNDO);
    sem_op(shm->sem_id, shm->stations[target_station].sem_seats_index, +1,
           SEM_UNDO);
    sim_sleep((WORKER_PAUSE * 60), shm->config.n_nano_secs);
    sem_op(shm->sem_id, shm->stations[target_station].sem_seats_index, -1,
           SEM_UNDO);
    sem_op(shm->sem_id, SEM_MUTEX_SHM, -1, SEM_UNDO);
    shm->stations[target_station].active_operators++;
    sem_op(shm->sem_id, SEM_MUTEX_SHM, +1, SEM_UNDO);
    stats_record_active_operator(&shm->sim_stats, shm->sem_id);
    return;
  }
  sem_op(shm->sem_id, SEM_MUTEX_SHM, +1, SEM_UNDO);
}

/**
 * @brief Runs the main workday processing loop.
 *
 * Continuously polls the station's message queue for orders, processes them,
 * and handles operator breaks based on random probability.
 *
 * @param shm Pointer to the SharedData structure.
 */
static void run_workday(struct SharedData *shm) {
  while (shm->day_running) {

    if (target_station == STATION_CASSA) {
      OrderMsg order;
      if (!wait_order(shm, &order, target_station,
                      MSG_CONTENT_SIZE(OrderMsg))) {
        continue;
      }
      serve_cassa(shm, &order);
    } else {
      ServingMsg order;
      if (!wait_order(shm, &order, target_station,
                      MSG_CONTENT_SIZE(ServingMsg))) {
        continue;
      }
      if (target_station == STATION_PRIMI ||
          target_station == STATION_SECONDI) {
        serve_primi_secondi(shm, &order);
      } else if (target_station == STATION_COFFEE) {
        serve_caffe(shm, &order);
      }
    }
    int prob_pausa = (rand_r(&seed) % 100);
    if (prob_pausa < 15) {
      go_pause(shm, target_station);
    }
  }
}

/**
 * @brief Acquires a workstation seat at the assigned station.
 *
 * Blocks on the station's seat semaphore, increments the active operators counter
 * in shared memory, and records active operator stats.
 *
 * @param shm Pointer to the SharedData structure.
 */
static void joint_stazione(struct SharedData *shm) {
  int target_sem = shm->stations[target_station].sem_seats_index;
  sem_op(shm->sem_id, target_sem, -1, SEM_UNDO);
  sem_op(shm->sem_id, SEM_MUTEX_SHM, -1, SEM_UNDO);
  shm->stations[target_station].active_operators++;
  sem_op(shm->sem_id, SEM_MUTEX_SHM, +1, SEM_UNDO);
  stats_record_active_operator(&shm->sim_stats, shm->sem_id);
}

/**
 * @brief Relinquishes the workstation seat at the assigned station.
 *
 * Decrements the active operators counter and increments the seat semaphore.
 *
 * @param shm Pointer to the SharedData structure.
 */
static void leave_stazione(struct SharedData *shm) {
  int target_sem = shm->stations[target_station].sem_seats_index;
  sem_op(shm->sem_id, SEM_MUTEX_SHM, -1, SEM_UNDO);
  shm->stations[target_station].active_operators--;
  sem_op(shm->sem_id, SEM_MUTEX_SHM, +1, SEM_UNDO);
  sem_op(shm->sem_id, target_sem, +1, SEM_UNDO);
}

/**
 * @brief Main entry point for the operator process.
 *
 * Connects to shared memory, sets up signal actions, and loops through simulation days
 * performing workday routines.
 *
 * @param argc Count of command line arguments.
 * @param argv Vector of command line arguments.
 * @return 0 on successful termination.
 */
int main(int argc, char *argv[]) {
  (void)argc;
  (void)argv;

  parse_arguments(argc, argv);

  struct SharedData *shm = attach_shared_memory(shm_id);

  set_sigaction();

  seed = time(NULL) ^ getpid();
  nof_pause = shm->config.nof_pause;

  while (1) {
    sem_op(shm->sem_id, SEM_READY, +1, 0);
    sem_op(shm->sem_id, SEM_DAY_START, -1, 0);

    if (!shm->simulation_running) {
      break;
    }

    joint_stazione(shm);

    run_workday(shm);

    leave_stazione(shm);
  }
  return 0;
}
