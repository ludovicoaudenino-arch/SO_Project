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
#include <unistd.h>

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
    fprintf(stderr, "NOT ENOUGH ARGUMENTS");
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
  reply.pid = getpid();
  send_message(shm->msg_queue_list[NUM_QUEUES - 1], &reply,
               MSG_CONTENT_SIZE(ServedMsg), 0);
}

static void serve_caffe(struct SharedData *shm, ServingMsg *order) {
  int sem_id = shm->sem_id;
  ServedMsg reply;
  reply.mytype = order->pid;
  reply.served = 1;
  reply.pid = getpid();

  send_message(shm->msg_queue_list[NUM_QUEUES - 1], &reply,
               MSG_CONTENT_SIZE(ServedMsg), 0);
  sem_op(sem_id, SEM_MUTEX_STATS, -1, SEM_UNDO);
  shm->sim_stats.dishes_coffee_today++;
  sem_op(sem_id, SEM_MUTEX_STATS, +1, SEM_UNDO);
}

static void serve_cassa(struct SharedData *shm, OrderMsg *order) {
  int sem_id = shm->sem_id;

  float total_primi_price = order->primi_ordered * shm->config.price_primi;
  float total_secondi_price =
      order->secondi_ordered * shm->config.price_secondi;
  float total_coffee_price = order->coffee_ordered * shm->config.price_coffee;

  float total = total_primi_price + total_secondi_price + total_coffee_price;

  sem_op(sem_id, SEM_MUTEX_STATS, -1, SEM_UNDO);
  shm->sim_stats.revenue_today += total;
  sem_op(sem_id, SEM_MUTEX_STATS, +1, SEM_UNDO);

  ServedMsg reply;
  reply.mytype = order->pid;
  reply.served = 1;
  reply.pid = getpid();
  send_message(shm->msg_queue_list[NUM_QUEUES - 1], &reply,
               MSG_CONTENT_SIZE(ServedMsg), 0);
}

static void handle_jolly_assignment(struct SharedData *shm) {
  if (target_station == -1) {
    StationMsg jolly_msg;
    receive_message(shm->msg_queue_list[NUM_QUEUES - 1], &jolly_msg,
                    MSG_CONTENT_SIZE(StationMsg), JOLLY_MSG_TYPE, 0);
    target_station = jolly_msg.station_id;
  }
}

static void run_workday(struct SharedData *shm) {
  while (shm->day_running) {
    int service_time =
        random_service_time(shm->stations[target_station].avg_srvc,
                            shm->stations[target_station].srvc_delta);

    if (target_station == STATION_CASSA) {
      OrderMsg order;
      receive_message(shm->msg_queue_list[target_station], &order,
                      MSG_CONTENT_SIZE(OrderMsg),
                      shm->stations[target_station].msg_type, 0);
      sim_sleep(service_time, shm->config.n_nano_secs);
      serve_cassa(shm, &order);
    } else {
      ServingMsg order;
      receive_message(shm->msg_queue_list[target_station], &order,
                      MSG_CONTENT_SIZE(ServingMsg),
                      shm->stations[target_station].msg_type, 0);
      sim_sleep(service_time, shm->config.n_nano_secs);
      if (target_station == STATION_PRIMI ||
          target_station == STATION_SECONDI) {
        serve_primi_secondi(shm, &order);
      } else if (target_station == STATION_COFFEE) {
        serve_caffe(shm, &order);
      }
    }
  }
}

int main(int argc, char *argv[]) {
  (void)argc;
  (void)argv;

  parse_arguments(argc, argv);

  struct SharedData *shm = attach_shared_memory(shm_id);

  set_sigaction();

  while (shm->simulation_running) {
    sem_op(shm->sem_id, SEM_READY, +1, 0);
    sem_op(shm->sem_id, SEM_DAY_START, -1, 0);

    handle_jolly_assignment(shm);

    if (target_station >= 0 && target_station <= 3) {
      int target_sem = shm->stations[target_station].sem_seats_index;
      sem_op(shm->sem_id, target_sem, -1, SEM_UNDO);
    }

    run_workday(shm);

    if (target_station >= 0 && target_station <= 3) {
      int target_sem = shm->stations[target_station].sem_seats_index;
      sem_op(shm->sem_id, target_sem, +1, SEM_UNDO);
    }
  }
  return 0;
}
