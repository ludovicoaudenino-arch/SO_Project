/**
 * @file operatore.c
 * @brief Main entry point for the worker (operatore) process.
 */
#include "common.h"
#include "ipc_utils.h"
#include "shared_data.h"
#include "time_utils.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/sem.h>

static int shm_id;
static int target_station;
volatile sig_atomic_t should_exit = 0;

static void handle_signal(int sig) {
  (void)sig;
  should_exit = 1;
}

static void set_sigaction() {
  struct sigaction sa = {.sa_handler = handle_signal, .sa_flags = 0};
  sigemptyset(&sa.sa_mask);
  sigaction(SIGUSR1, &sa, NULL);
}

static void parse_arguments(int argc, char *argv[]) {
  if (argc < 3) {
    perror("NOT ENOUGH ARG");
    exit(EXIT_FAILURE);
  }
  shm_id = atoi(argv[1]);
  target_station = atoi(argv[2]);
}

static void serve_primi_secondi(struct SharedData *shm, ServingMsg *order) {
  int sem_id = shm->sem_id;
  int dish_type = order->dish_type;
  int served = 0;
  int portion_mutex =
      (target_station == 0) ? SEM_MUTEX_PORZIONI_P : SEM_MUTEX_PORZIONI_S;

  sem_op(sem_id, portion_mutex, -1, SEM_UNDO);
  if (target_station == 0) {
    if (shm->portion_left_primi[dish_type] > 0) {
      shm->portion_left_primi[dish_type]--;
      served = 1;
      sem_op(sem_id, SEM_MUTEX_STATS, -1, SEM_UNDO);
      shm->sim_stats.dishes_primi_today++;
      sem_op(sem_id, SEM_MUTEX_STATS, +1, SEM_UNDO);
    }
  } else {
    if (shm->portion_left_secondi[dish_type] > 0) {
      shm->portion_left_secondi[dish_type]--;
      served = 1;
      sem_op(sem_id, SEM_MUTEX_STATS, -1, SEM_UNDO);
      shm->sim_stats.dishes_secondi_today++;
      sem_op(sem_id, SEM_MUTEX_STATS, +1, SEM_UNDO);
    }
  }
  sem_op(sem_id, portion_mutex, +1, SEM_UNDO);

  ServedMsg reply;
  reply.mytype = order->pid;
  reply.served = served;
  send_message(shm->msg_queue_served, &reply, MSG_CONTENT_SIZE(ServingMsg), 0);
}

static void serve_caffe(struct SharedData *shm, ServingMsg *order) {
  int sem_id = shm->sem_id;
  ServedMsg reply;
  reply.mytype = order->pid;
  reply.served = 1;

  send_message(shm->msg_queue_served, &reply, MSG_CONTENT_SIZE(ServingMsg), 0);
  sem_op(sem_id, SEM_MUTEX_STATS, -1, SEM_UNDO);
  shm->sim_stats.dishes_coffee_today++;
  sem_op(sem_id, SEM_MUTEX_STATS, +1, SEM_UNDO);
}

static void serve_cassa(struct SharedData *shm, ServingMsg *order) {
  (void)shm;
  (void)order;
}

static void handle_jolly_assignment(struct SharedData *shm) {
  if (target_station == -1) {
    StationMsg jolly_msg;
    receive_message(shm->msg_queue_served, &jolly_msg,
                    MSG_CONTENT_SIZE(StationMsg), JOLLY_MSG_TYPE, 0);
    target_station = jolly_msg.station_id;
  }
}

static void run_workday(struct SharedData *shm) {
  while (shm->day_running) {
    ServingMsg order;
    receive_message(shm->msg_queue_requests[target_station], &order,
                    MSG_CONTENT_SIZE(ServingMsg),
                    shm->stations[target_station].msg_type, 0);
    int service_time =
        random_service_time(shm->stations[target_station].avg_srvc,
                            shm->stations[target_station].srvc_delta);
    sim_sleep(service_time, shm->config.n_nano_secs);

    if (target_station == 0 || target_station == 1) {
      serve_primi_secondi(shm, &order);
    } else if (target_station == 2) {
      serve_caffe(shm, &order);
    } else if (target_station == 3) {
      serve_cassa(shm, &order);
    }
  }
}

int main(int argc, char *argv[]) {
  (void)argc;
  (void)argv;

  parse_arguments(argc, argv);

  struct SharedData *shm = attach_shared_memory(shm_id);
  int sem_id = shm->sem_id;

  set_sigaction();

  while (shm->simulation_running) {
    sem_op(sem_id, SEM_READY, +1, 0);
    sem_op(sem_id, SEM_DAY_START, -1, 0);

    handle_jolly_assignment(shm);

    if (target_station >= 0 && target_station <= 3) {
      int target_sem = shm->stations[target_station].sem_seats_index;
      sem_op(sem_id, target_sem, -1, SEM_UNDO);
    }

    run_workday(shm);

    if (target_station >= 0 && target_station <= 3) {
      int target_sem = shm->stations[target_station].sem_seats_index;
      sem_op(sem_id, target_sem, +1, SEM_UNDO);
    }
  }
  return 0;
}
