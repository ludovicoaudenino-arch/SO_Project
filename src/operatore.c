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

#define JOLLY_MSG_TYPE 63
#define ORDER_PRIMI_TYPE 1
#define ORDER_SECONDI_TYPE 2
#define ORDER_COFFEE_TYPE 3
#define ORDER_CASSA_TYPE 4
#define MSG_CONTENT_SIZE(msg_struct) (sizeof(msg_struct) - sizeof(long))

typedef struct {
  int avg_srvc;
  int srvc_delta;
  int sem_seats_index;
  int msg_type;
} StationProperties;

typedef struct {
  long mytype;
  int dish_type;
  int served;
  pid_t pid;
} ServingMsg;

typedef struct {
  long mytype;
  int station_id;
} JollyMsg;

static int shm_id;
static int sem_id;
static int msg_id;
static int target_station;
volatile sig_atomic_t should_exit = 0;
static StationProperties station_prop;

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
  if (argc < 5) {
    perror("NOT ENOUGH ARG");
    exit(EXIT_FAILURE);
  }
  int *arg[4] = {&shm_id, &sem_id, &msg_id, &target_station};
  for (int i = 1; i < argc; i++) {
    *arg[i - 1] = atoi(argv[i]);
  }
}

static void set_StationProperties(struct SharedData *shm) {
  switch (target_station) {
  case 0:
    station_prop.avg_srvc = shm->config.avg_srvc_primi;
    station_prop.srvc_delta = 50;
    station_prop.sem_seats_index = SEM_SEATS_PRIMI + target_station;
    station_prop.msg_type = ORDER_PRIMI_TYPE;
    break;
  case 1:
    station_prop.avg_srvc = shm->config.avg_srvc_secondi;
    station_prop.srvc_delta = 50;
    station_prop.sem_seats_index = SEM_SEATS_PRIMI + target_station;
    station_prop.msg_type = ORDER_SECONDI_TYPE;
    break;
  case 2:
    station_prop.avg_srvc = shm->config.avg_srvc_coffee;
    station_prop.srvc_delta = 80;
    station_prop.sem_seats_index = SEM_SEATS_PRIMI + target_station;
    station_prop.msg_type = ORDER_COFFEE_TYPE;
    break;
  case 3:
    station_prop.avg_srvc = shm->config.avg_srvc_cassa;
    station_prop.srvc_delta = 20;
    station_prop.sem_seats_index = SEM_SEATS_PRIMI + target_station;
    station_prop.msg_type = ORDER_CASSA_TYPE;
    break;
  }
}

int main(int argc, char *argv[]) {
  (void)argc;
  (void)argv;

  parse_arguments(argc, argv);

  struct SharedData *shm = attach_shared_memory(shm_id);

  set_StationProperties(shm);

  set_sigaction();

  while (shm->simulation_running) {
    sem_op(sem_id, SEM_READY, +1, 0);
    sem_op(sem_id, SEM_DAY_START, -1, 0);

    if (target_station == -1) {
      JollyMsg jolly_msg;
      receive_message(msg_id, &jolly_msg, MSG_CONTENT_SIZE(JollyMsg),
                      JOLLY_MSG_TYPE, 0);
      target_station = jolly_msg.station_id;
      set_StationProperties(shm);
    }
    if (target_station >= 0 && target_station <= 3) {
      int target_sem = station_prop.sem_seats_index;
      sem_op(sem_id, target_sem, -1, SEM_UNDO);
    }

    while (shm->day_running) {
      ServingMsg order;
      receive_message(msg_id, &order, MSG_CONTENT_SIZE(ServingMsg),
                      station_prop.msg_type, 0);
      int service_time =
          random_service_time(station_prop.avg_srvc, station_prop.srvc_delta);
      sim_sleep(service_time, shm->config.n_nano_secs);

      if (target_station == 0 || target_station == 1) {
        int dish_type = order.dish_type;
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

        ServingMsg reply;
        reply.mytype = order.pid;
        reply.served = served;
        send_message(msg_id, &reply, MSG_CONTENT_SIZE(ServingMsg), 0);
      } else if (target_station == 2) {
        ServingMsg reply;
        reply.mytype = order.pid;
        reply.served = 1;

        send_message(msg_id, &reply, MSG_CONTENT_SIZE(ServingMsg), 0);
        sem_op(sem_id, SEM_MUTEX_STATS, -1, SEM_UNDO);
        shm->sim_stats.dishes_coffee_today++;
        sem_op(sem_id, SEM_MUTEX_STATS, +1, SEM_UNDO);
      }
    }
    sem_op(sem_id, station_prop.sem_seats_index, +1, SEM_UNDO);
  }
  return 0;
}
