/**
 * @file operatore.c
 * @brief Main entry point for the worker (operatore) process.
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

static int shm_id;
static int target_station;
static unsigned int seed;
static int nof_pause;
int service_time;

static void handle_signal(int sig) { (void)sig; }

static void set_sigaction() {
  struct sigaction sa = {.sa_handler = handle_signal, .sa_flags = 0};
  sigemptyset(&sa.sa_mask);
  sigaction(SIGUSR1, &sa, NULL);
}

static void parse_arguments(int argc, char *argv[]) {
  if (argc < 3) {
    fprintf(stderr, "NOT ENOUGH ARGUMENTS");
    exit(EXIT_FAILURE);
  }
  shm_id = atoi(argv[1]);
  target_station = atoi(argv[2]);
  if (target_station < 0 || target_station >= NUM_STATIONS) {
    perror("ERROR ARGUMENT 2: TOO MANY STATION");
    exit(EXIT_FAILURE);
  }
}

static void serve_primi_secondi(struct SharedData *shm, ServingMsg *order) {
  int sem_id = shm->sem_id;
  int service_time =
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
    sim_sleep(service_time, shm->config.n_nano_secs);
    served = 1;
    stats_record_dish_served(&shm->sim_stats, sem_id, target_station);
  } else {
    sem_op(sem_id, portion_mutex, +1, SEM_UNDO);
  }

  ServedMsg reply;
  reply.mytype = order->pid;
  reply.served = served;
  reply.pid = getpid();
  send_message(shm->msg_queue_list[NUM_QUEUES - 1], &reply,
               MSG_CONTENT_SIZE(ServedMsg), 0);
}

static void serve_caffe(struct SharedData *shm, ServingMsg *order) {
  int sem_id = shm->sem_id;
  int service_time =
      random_service_time(shm->stations[target_station].avg_srvc,
                          shm->stations[target_station].srvc_delta);

  sim_sleep(service_time, shm->config.n_nano_secs);
  ServedMsg reply;
  reply.mytype = order->pid;
  reply.served = 1;
  reply.pid = getpid();

  send_message(shm->msg_queue_list[NUM_QUEUES - 1], &reply,
               MSG_CONTENT_SIZE(ServedMsg), 0);
  stats_record_dish_served(&shm->sim_stats, sem_id, STATION_COFFEE);
}

static void serve_cassa(struct SharedData *shm, OrderMsg *order) {
  int sem_id = shm->sem_id;
  int service_time =
      random_service_time(shm->stations[target_station].avg_srvc,
                          shm->stations[target_station].srvc_delta);

  float total_primi_price = order->primi_ordered * shm->config.price_primi;
  float total_secondi_price =
      order->secondi_ordered * shm->config.price_secondi;
  float total_coffee_price = order->coffee_ordered * shm->config.price_coffee;

  float total = total_primi_price + total_secondi_price + total_coffee_price;

  stats_record_revenue(&shm->sim_stats, sem_id, total);

  sim_sleep(service_time, shm->config.n_nano_secs);
  ServedMsg reply;
  reply.mytype = order->pid;
  reply.served = 1;
  reply.pid = getpid();
  send_message(shm->msg_queue_list[NUM_QUEUES - 1], &reply,
               MSG_CONTENT_SIZE(ServedMsg), 0);
}

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

static void joint_stazione(struct SharedData *shm) {
  int target_sem = shm->stations[target_station].sem_seats_index;
  sem_op(shm->sem_id, target_sem, -1, SEM_UNDO);
  sem_op(shm->sem_id, SEM_MUTEX_SHM, -1, SEM_UNDO);
  shm->stations[target_station].active_operators++;
  sem_op(shm->sem_id, SEM_MUTEX_SHM, +1, SEM_UNDO);
  stats_record_active_operator(&shm->sim_stats, shm->sem_id);
}

static void leave_stazione(struct SharedData *shm) {
  int target_sem = shm->stations[target_station].sem_seats_index;
  sem_op(shm->sem_id, SEM_MUTEX_SHM, -1, SEM_UNDO);
  shm->stations[target_station].active_operators--;
  sem_op(shm->sem_id, SEM_MUTEX_SHM, +1, SEM_UNDO);
  sem_op(shm->sem_id, target_sem, +1, SEM_UNDO);
}

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
