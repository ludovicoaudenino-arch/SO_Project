/**
 * @file utente.c
 * @brief Main entry point for the user (utente) process.
 */

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
  if (argc < 2) {
    perror("NOT ENOUGH ARG");
    exit(EXIT_FAILURE);
  }
  int shm_id = atoi(argv[1]);
  (void)shm_id;
  return 0;
}
