/**
 * @file utente.c
 * @brief Main entry point for the user (utente) process.
 */
#include "common.h"
#include "ipc_utils.h"
#include "shared_data.h"
#include <stdio.h>
#include <stdlib.h>

static int shm_id;
static int sem_id;
static int msg_id[NUM_QUEUES];

static void parse_arguments(int argc, char *argv[]) {
  if (argc < 3) {
    fprintf(stderr, "NOT ENOUGH ARGUMENTS");
    exit(EXIT_FAILURE);
  }
  shm_id = atoi(argv[1]);
}

int main(int argc, char *argv[]) {
  parse_arguments(argc, argv);

  return 0;
}
