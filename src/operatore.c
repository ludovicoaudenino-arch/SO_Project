/**
 * @file operatore.c
 * @brief Main entry point for the worker (operatore) process.
 */
#include "common.h"
#include "ipc_utils.h"
#include "shared_data.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#define JOLLY_MSG_TYPE 63
#define MSG_CONTENT_SIZE (sizeof(((Msg *)0)->content))

static int shm_id;
static int sem_id;
static int msg_id;
static int target_station;
volatile sig_atomic_t should_exit = 0;

typedef struct {
  long mytype;

  union {
    int station_id;
  } content;
} Msg;

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

int main(int argc, char *argv[]) {
  (void)argc;
  (void)argv;

  parse_arguments(argc, argv);

  struct SharedData *shm = attach_shared_memory(shm_id);

  set_sigaction();

  sem_op(sem_id, SEM_READY, +1, 0);
  sem_op(sem_id, SEM_DAY_START, -1, 0);

  if (target_station >= 0 && target_station <= 3) {
    int target_sem = SEM_SEATS_PRIMI + target_station;
    sem_op(sem_id, target_sem, -1, 0);
  } else if (target_station == -1) {
    Msg jolly_msg = {.mytype = JOLLY_MSG_TYPE};
    receive_message(msg_id, &jolly_msg, MSG_CONTENT_SIZE, JOLLY_MSG_TYPE, 0);
  }

  return 0;
}
