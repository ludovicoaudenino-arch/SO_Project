/**
 * @file utente.c
 * @brief Main entry point for the user (utente) process.
 */
#include "common.h"
#include "ipc_utils.h"
#include "shared_data.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

static int shm_id;
static unsigned int seed;
static int preferenze_primi[MAX_DISH_TYPES];
static int preferenze_secondi[MAX_DISH_TYPES];
static int want_coffee;

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

static void choose_from_menu(struct SharedData *shm) {
  int nof_primi = shm->nof_type_primi;

  for (int i = 0; i < nof_primi; i++) {
    preferenze_primi[i] = i;
  }

  fisher_yates_shuffle(preferenze_primi, nof_primi);

  int nof_secondi = shm->nof_type_secondi;

  for (int i = 0; i < nof_secondi; i++) {
    preferenze_secondi[i] = i;
  }

  fisher_yates_shuffle(preferenze_secondi, nof_secondi);

  int probability_coffee_sweet = (rand_r(&seed) % 100);
  want_coffee = (probability_coffee_sweet < 25) ? 1 : 0;
}

static void make_order(struct SharedData *shm) {
  ServingMsg order = {.pid = getpid()};
  ServedMsg check_served = {.served = 0};
  int attempt = 0;
  while (!check_served.served) {
    if (attempt < shm->nof_type_primi) {
      order.dish_type = preferenze_primi[attempt++];
      send_message(shm->msg_queue_list[STATION_PRIMI], &order,
                   MSG_CONTENT_SIZE(ServingMsg), 0);

      receive_message(shm->msg_queue_list[NUM_QUEUES - 1], &check_served,
                      MSG_CONTENT_SIZE(ServedMsg), order.pid, 0);
    } else {
      break;
    }
  }
};

int main(int argc, char *argv[]) {

  parse_arguments(argc, argv);

  struct SharedData *shm = attach_shared_memory(shm_id);

  seed = time(NULL) ^ getpid();

  while (shm->simulation_running == 1) {
    sem_op(shm->sem_id, SEM_READY, +1, 0);
    sem_op(shm->sem_id, SEM_DAY_START, -1, 0);
  }
  return 0;
}
