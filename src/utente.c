/**
 * @file utente.c
 * @brief Main entry point for the user (utente) process.
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

static int shm_id;
static unsigned int seed;
static int preferenze_primi[MAX_DISH_TYPES];
static int preferenze_secondi[MAX_DISH_TYPES];
static int want_coffee;
static pid_t pid;

void fisher_yates_shuffle(int *array, int n) {
  for (int i = n - 1; i > 0; i--) {

    int j = rand_r(&seed) % (i + 1);

    int temp = array[i];
    array[i] = array[j];
    array[j] = temp;
  }
}

static void parse_arguments(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(stderr, "NOT ENOUGH ARGUMENTS");
    exit(EXIT_FAILURE);
  }
  shm_id = atoi(argv[1]);
}

static void enter_queue(struct SharedData *shm, int station_type) {
  sem_op(shm->sem_id, SEM_MUTEX_SHM, -1, SEM_UNDO);
  shm->stations[station_type].queue_length++;
  sem_op(shm->sem_id, SEM_MUTEX_SHM, +1, SEM_UNDO);
}

static void leave_queue(struct SharedData *shm, int station_type) {
  sem_op(shm->sem_id, SEM_MUTEX_SHM, -1, SEM_UNDO);
  shm->stations[station_type].queue_length--;
  sem_op(shm->sem_id, SEM_MUTEX_SHM, +1, SEM_UNDO);
}

static int wait_reply(struct SharedData *shm, ServedMsg *served,
                      int station_type) {
  int reply_received = 0;

  while (reply_received == 0) {
    if (receive_message(shm->msg_queue_list[NUM_QUEUES - 1], served,
                        MSG_CONTENT_SIZE(ServedMsg), pid, IPC_NOWAIT) == -1) {
      if (should_exit || !shm->day_running) {
        leave_queue(shm, station_type);
        return 0;
      }
      usleep(1000);
    } else {
      reply_received = 1;
    }
  }

  return 1;
}
static void choose_preferenze(int *preferenze_type, int nof_type) {
  for (int i = 0; i < nof_type; i++) {
    preferenze_type[i] = i;
  }
  fisher_yates_shuffle(preferenze_type, nof_type);
}

static void choose_from_menu(struct SharedData *shm) {
  choose_preferenze(preferenze_primi, shm->stations[STATION_PRIMI].nof_type);
  choose_preferenze(preferenze_secondi,
                    shm->stations[STATION_SECONDI].nof_type);

  int probability_coffee_sweet = (rand_r(&seed) % 100);
  want_coffee = (probability_coffee_sweet < 25) ? 1 : 0;
}

static int try_order(struct SharedData *shm, int *preferenze_type,
                     int station_type) {
  if (should_exit || !shm->day_running) {
    return 0;
  }

  enter_queue(shm, station_type);
  ServingMsg order = {.pid = pid, .mytype = ORDER_TYPE};
  ServedMsg check_served = {.served = 0};

  for (int attempt = 0; attempt < shm->stations[station_type].nof_type;
       attempt++) {
    order.dish_type = (preferenze_type == NULL) ? 0 : preferenze_type[attempt];
    send_message(shm->msg_queue_list[station_type], &order,
                 MSG_CONTENT_SIZE(ServingMsg), 0);

    if (wait_reply(shm, &check_served, station_type) == 0) {
      return 0;
    }

    if (check_served.served) {
      break;
    }
  }

  leave_queue(shm, station_type);
  return check_served.served;
}

static ServedMsg perform_cassa_payment(struct SharedData *shm,
                                       int piatti_ordered[]) {
  if (should_exit || !shm->day_running) {
    ServedMsg served = {.served = 0};
    return served;
  }
  enter_queue(shm, STATION_CASSA);
  OrderMsg order;
  order.primi_ordered = piatti_ordered[STATION_PRIMI];
  order.secondi_ordered = piatti_ordered[STATION_SECONDI];
  order.coffee_ordered = piatti_ordered[STATION_COFFEE];
  order.pid = pid;
  order.mytype = ORDER_TYPE;

  send_message(shm->msg_queue_list[STATION_CASSA], &order,
               MSG_CONTENT_SIZE(OrderMsg), 0);

  ServedMsg served = {.served = 0};
  if (wait_reply(shm, &served, STATION_CASSA) == 0) {
    return served;
  }

  leave_queue(shm, STATION_CASSA);
  return served;
}

static void take_seat(struct SharedData *shm, int n_piatti) {
  if (!should_exit && shm->simulation_running && shm->day_running) {
    sem_op(shm->sem_id, SEM_TABLE_SEATS, -1, 0);
    sim_sleep(n_piatti * 2, shm->config.n_nano_secs);
    sem_op(shm->sem_id, SEM_TABLE_SEATS, +1, 0);
  }

  sem_op(shm->sem_id, SEM_MUTEX_STATS, -1, SEM_UNDO);
  shm->sim_stats.users_served_today++;
  sem_op(shm->sem_id, SEM_MUTEX_STATS, +1, SEM_UNDO);
}

static void run_routine(struct SharedData *shm) {
  if (should_exit || !shm->day_running) {
    return;
  }
  int station_done[FOOD_STATION] = {0, 0, 0};
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
    sem_op(shm->sem_id, SEM_MUTEX_SHM, +1, SEM_UNDO);
    if (target_station != -1) {
      ordered[target_station] =
          try_order(shm, preferenze_list[target_station], target_station);
      station_done[target_station] = 1;
    }
  }

  if (ordered[STATION_PRIMI] == 0 && ordered[STATION_SECONDI] == 0) {
    sem_op(shm->sem_id, SEM_MUTEX_STATS, -1, SEM_UNDO);
    shm->sim_stats.users_not_served_today++;
    sem_op(shm->sem_id, SEM_MUTEX_STATS, +1, SEM_UNDO);
    return;
  }

  if (perform_cassa_payment(shm, ordered).served) {
    int n_piatti = ordered[STATION_PRIMI] + ordered[STATION_SECONDI] +
                   ordered[STATION_COFFEE];
    take_seat(shm, n_piatti);
  } else {
    sem_op(shm->sem_id, SEM_MUTEX_STATS, -1, SEM_UNDO);
    shm->sim_stats.users_not_served_today++;
    sem_op(shm->sem_id, SEM_MUTEX_STATS, +1, SEM_UNDO);
  }
}

int main(int argc, char *argv[]) {
  pid = getpid();

  parse_arguments(argc, argv);

  struct SharedData *shm = attach_shared_memory(shm_id);

  set_sigaction();

  seed = time(NULL) ^ getpid();

  while (shm->simulation_running == 1) {
    sem_op(shm->sem_id, SEM_READY, +1, 0);
    sem_op(shm->sem_id, SEM_DAY_START, -1, 0);
    should_exit = 0;

    choose_from_menu(shm);
    run_routine(shm);
  }
  return 0;
}
