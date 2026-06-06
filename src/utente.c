/**
 * @file utente.c
 * @brief Main entry point for the user (utente) process.
 *
 * This file contains the implementation of the user lifecycle. Each user
 * reads the menu, chooses what to eat, joins food station queues by dynamically
 * balancing queue loads, pays at the cashier (cassa), eats at a table,
 * and updates global simulation statistics.
 */

#include "common.h"
#include "ipc_utils.h"
#include "shared_data.h"
#include "time_utils.h"
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

volatile sig_atomic_t should_exit = 0; /**< @brief Flag to request clean termination upon receiving SIGUSR1 */

static int shm_id;                     /**< @brief Shared memory identifier for System V segment */
static unsigned int seed;              /**< @brief Thread-safe local random seed for rand_r */
static int preferenze_primi[MAX_DISH_TYPES];   /**< @brief Randomized preferences for first course dish types */
static int preferenze_secondi[MAX_DISH_TYPES];  /**< @brief Randomized preferences for second course dish types */
static int want_coffee;                /**< @brief Flag indicating if the user wants coffee/dessert (25% probability) */
static pid_t pid;                      /**< @brief Process ID of the current user process */

/**
 * @brief Signal handler for SIGUSR1.
 *
 * Sets the should_exit flag to 1 to request a graceful exit from the loop.
 *
 * @param sig Signal number (unused).
 */
static void handle_signal(int sig) {
  (void)sig;
  should_exit = 1;
}

/**
 * @brief Configures the signal action for SIGUSR1.
 */
static void set_sigaction() {
  struct sigaction sa = {.sa_handler = handle_signal, .sa_flags = 0};
  sigemptyset(&sa.sa_mask);
  sigaction(SIGUSR1, &sa, NULL);
}

/**
 * @brief Shuffles an integer array using the Fisher-Yates algorithm.
 *
 * Used to randomize the food preferences of the user.
 *
 * @param array Pointer to the array to shuffle.
 * @param n Size of the array.
 */
void fisher_yates_shuffle(int *array, int n) {
  for (int i = n - 1; i > 0; i--) {
    int j = rand_r(&seed) % (i + 1);
    int temp = array[i];
    array[i] = array[j];
    array[j] = temp;
  }
}

/**
 * @brief Parses command line arguments to retrieve the shared memory ID.
 *
 * Exits with EXIT_FAILURE if arguments are insufficient.
 *
 * @param argc Count of command line arguments.
 * @param argv Vector of command line arguments.
 */
static void parse_arguments(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(stderr, "NOT ENOUGH ARGUMENTS\n");
    exit(EXIT_FAILURE);
  }
  shm_id = atoi(argv[1]);
}

/**
 * @brief Increments the queue length of a target station in shared memory.
 *
 * Protected using the SEM_MUTEX_SHM semaphore.
 *
 * @param shm Pointer to the SharedData structure.
 * @param station_type Index of the station (STATION_PRIMI, etc.).
 */
static void enter_queue(struct SharedData *shm, int station_type) {
  sem_op(shm->sem_id, SEM_MUTEX_SHM, -1, SEM_UNDO);
  shm->stations[station_type].queue_length++;
  sem_op(shm->sem_id, SEM_MUTEX_SHM, +1, SEM_UNDO);
}

/**
 * @brief Decrements the queue length of a target station in shared memory.
 *
 * Protected using the SEM_MUTEX_SHM semaphore.
 *
 * @param shm Pointer to the SharedData structure.
 * @param station_type Index of the station (STATION_PRIMI, etc.).
 */
static void leave_queue(struct SharedData *shm, int station_type) {
  sem_op(shm->sem_id, SEM_MUTEX_SHM, -1, SEM_UNDO);
  shm->stations[station_type].queue_length--;
  sem_op(shm->sem_id, SEM_MUTEX_SHM, +1, SEM_UNDO);
}

/**
 * @brief Blocks waiting for a response message from an operator.
 *
 * Receives the message from the response queue matching this user's PID.
 *
 * @param shm Pointer to the SharedData structure.
 * @param served Pointer to ServedMsg struct where response will be saved.
 * @return 1 on successful message receipt, 0 on failure (e.g. queue removed).
 */
static int wait_reply(struct SharedData *shm, ServedMsg *served) {
  if (receive_message(shm->msg_queue_list[NUM_QUEUES - 1], served,
                      MSG_CONTENT_SIZE(ServedMsg), pid, 0) == -1) {
    return 0;
  } else {
    return 1;
  }
}

/**
 * @brief Randomizes dish preferences for first or second courses.
 *
 * @param preferenze_type Pointer to the preference array to initialize.
 * @param nof_type Number of dish types available in the menu for this station.
 */
static void choose_preferenze(int *preferenze_type, int nof_type) {
  for (int i = 0; i < nof_type; i++) {
    preferenze_type[i] = i;
  }
  fisher_yates_shuffle(preferenze_type, nof_type);
}

/**
 * @brief Decides which menu dishes to order and whether coffee is desired.
 *
 * @param shm Pointer to the SharedData structure.
 */
static void choose_from_menu(struct SharedData *shm) {
  choose_preferenze(preferenze_primi, shm->stations[STATION_PRIMI].nof_type);
  choose_preferenze(preferenze_secondi,
                    shm->stations[STATION_SECONDI].nof_type);

  int probability_coffee_sweet = (rand_r(&seed) % 100);
  want_coffee = (probability_coffee_sweet < 25) ? 1 : 0;
}

/**
 * @brief Attempts to order a dish at a specific food station.
 *
 * Loops through preferred dish types until one is successfully served
 * or all choices are exhausted. Records simulated wait time on success.
 *
 * @param shm Pointer to the SharedData structure.
 * @param preferenze_type Pointer to the randomized preference array.
 * @param station_type Food station index (STATION_PRIMI, etc.).
 * @return 1 if successfully served, 0 on failure (out of food or interrupted).
 */
static int try_order(struct SharedData *shm, int *preferenze_type,
                     int station_type) {
  struct timespec start_ts, end_ts;
  clock_gettime(CLOCK_MONOTONIC, &start_ts);

  ServingMsg order = {.pid = pid, .mytype = ORDER_TYPE};
  ServedMsg check_served = {.served = 0};
  int success = 0;

  for (int attempt = 0; attempt < shm->stations[station_type].nof_type;
       attempt++) {
    if (should_exit || !shm->day_running) {
      break;
    }
    order.dish_type = (preferenze_type == NULL) ? 0 : preferenze_type[attempt];
    int send_result = send_message(shm->msg_queue_list[station_type], &order,
                                   MSG_CONTENT_SIZE(ServingMsg), 0);
    if (send_result == -1) {
      break;
    }

    if (!wait_reply(shm, &check_served)) {
      break;
    }

    if (check_served.served) {
      success = 1;
      break;
    }
  }
  leave_queue(shm, station_type);

  if (success) {
    clock_gettime(CLOCK_MONOTONIC, &end_ts);
    long diff_ns = (end_ts.tv_sec - start_ts.tv_sec) * 1000000000L +
                   (end_ts.tv_nsec - start_ts.tv_nsec);
    long sim_secs = (diff_ns * 60) / shm->config.n_nano_secs;
    stats_record_wait_time(&shm->sim_stats, shm->sem_id, station_type,
                           sim_secs);
  }
  return success;
}

/**
 * @brief Handles payment at the checkout cassa station.
 *
 * Enters the cassa queue, sends an OrderMsg detailing consumed dishes,
 * and blocks waiting for cashier approval.
 *
 * @param shm Pointer to the SharedData structure.
 * @param piatti_ordered Array indicating which dishes were successfully obtained.
 * @return ServedMsg structure containing the checkout outcome.
 */
static ServedMsg perform_cassa_payment(struct SharedData *shm,
                                       int piatti_ordered[]) {
  if (should_exit || !shm->day_running) {
    ServedMsg served = {.served = 0};
    return served;
  }

  struct timespec start_ts, end_ts;
  clock_gettime(CLOCK_MONOTONIC, &start_ts);

  OrderMsg order;
  order.primi_ordered = piatti_ordered[STATION_PRIMI];
  order.secondi_ordered = piatti_ordered[STATION_SECONDI];
  order.coffee_ordered = piatti_ordered[STATION_COFFEE];
  order.pid = pid;
  order.mytype = ORDER_TYPE;
  ServedMsg served = {.served = 0};

  enter_queue(shm, STATION_CASSA);
  int send_result = send_message(shm->msg_queue_list[STATION_CASSA], &order,
                                 MSG_CONTENT_SIZE(OrderMsg), 0);
  if (send_result != -1) {
    int check_wait = wait_reply(shm, &served);

    if (check_wait) {
      clock_gettime(CLOCK_MONOTONIC, &end_ts);
      long diff_ns = (end_ts.tv_sec - start_ts.tv_sec) * 1000000000L +
                     (end_ts.tv_nsec - start_ts.tv_nsec);
      long sim_secs = (diff_ns * 60) / shm->config.n_nano_secs;
      stats_record_wait_time(&shm->sim_stats, shm->sem_id, STATION_CASSA,
                             sim_secs);
    }
  }
  leave_queue(shm, STATION_CASSA);
  return served;
}

/**
 * @brief Simulates dining at a table seat.
 *
 * Acquires a seat via SEM_TABLE_SEATS, sleeps for a duration proportional
 * to the number of plates, releases the seat, and records the user as served.
 *
 * @param shm Pointer to the SharedData structure.
 * @param n_piatti Total number of dishes consumed.
 */
static void take_seat(struct SharedData *shm, int n_piatti) {
  if (!should_exit && shm->simulation_running && shm->day_running) {
    sem_op(shm->sem_id, SEM_TABLE_SEATS, -1, 0);
    sim_sleep(n_piatti * 2, shm->config.n_nano_secs);
    sem_op(shm->sem_id, SEM_TABLE_SEATS, +1, 0);
  }

  stats_record_user_served(&shm->sim_stats, shm->sem_id);
}

/**
 * @brief Executes the daily routine for a user.
 *
 * Atomically selects food queues, attempts to order dishes, pays at the cassa,
 * and consumes food at a table. Records user as not served if food or payment fails.
 *
 * @param shm Pointer to the SharedData structure.
 */
static void run_routine(struct SharedData *shm) {
  if (should_exit || !shm->day_running) {
    return;
  }
  int station_done[NUM_FOOD_STATIONS] = {0, 0, 0};
  int *preferenze_list[3] = {preferenze_primi, preferenze_secondi, NULL};
  int ordered[3] = {0, 0, 0};

  if (!want_coffee) {
    station_done[STATION_COFFEE] = 1;
  }

  while (station_done[STATION_PRIMI] == 0 ||
         station_done[STATION_SECONDI] == 0 ||
         station_done[STATION_COFFEE] == 0) {
    if (should_exit || !shm->day_running) {
      break;
    }
    int min_queue = INT_MAX;
    int target_station = -1;
    sem_op(shm->sem_id, SEM_MUTEX_SHM, -1, SEM_UNDO);
    for (int i = 0; i < 3; i++) {
      if (station_done[i] == 0) {
        int station_queue = shm->stations[i].queue_length;
        if (station_queue < min_queue) {
          min_queue = station_queue;
          target_station = i;
        }
      }
    }
    if (target_station != -1) {
      shm->stations[target_station].queue_length++;
      sem_op(shm->sem_id, SEM_MUTEX_SHM, +1, SEM_UNDO);
      ordered[target_station] =
          try_order(shm, preferenze_list[target_station], target_station);
      station_done[target_station] = 1;
    } else {
      sem_op(shm->sem_id, SEM_MUTEX_SHM, +1, SEM_UNDO);
      break;
    }
  }

  if (ordered[STATION_PRIMI] == 0 && ordered[STATION_SECONDI] == 0) {
    stats_record_user_not_served(&shm->sim_stats, shm->sem_id);
    return;
  }

  if (perform_cassa_payment(shm, ordered).served) {
    int n_piatti = ordered[STATION_PRIMI] + ordered[STATION_SECONDI] +
                   ordered[STATION_COFFEE];
    take_seat(shm, n_piatti);
  } else {
    stats_record_user_not_served(&shm->sim_stats, shm->sem_id);
  }
}

/**
 * @brief Main entry point for the user process.
 *
 * Parses arguments, attaches shared memory, installs signal handler,
 * and enters the daily barrier synchronization loop.
 *
 * @param argc Count of command line arguments.
 * @param argv Vector of command line arguments.
 * @return 0 on successful termination.
 */
int main(int argc, char *argv[]) {
  pid = getpid();

  parse_arguments(argc, argv);

  struct SharedData *shm = attach_shared_memory(shm_id);

  set_sigaction();

  seed = time(NULL) ^ getpid();

  while (1) {
    sem_op(shm->sem_id, SEM_READY, +1, 0);
    sem_op(shm->sem_id, SEM_DAY_START, -1, 0);
    if (shm->simulation_running == 0) {
      break;
    }
    should_exit = 0;

    choose_from_menu(shm);
    run_routine(shm);
  }
  return 0;
}
