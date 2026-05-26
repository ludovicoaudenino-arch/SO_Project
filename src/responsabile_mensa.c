#include "common.h"
#include "config.h"
#include "ipc_utils.h"
#include "shared_data.h"
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define OPERATOR_CASSA_PATH "./bin/operatore_cassa"
#define OPERATOR_PATH "./bin/operatore"
#define USER_PATH "./bin/utente"
#define ARGC_MAX_LENGHT 16

int shm_id = -1;
int sem_id = -1;
int msg_id = -1;
struct SharedData *shm = NULL;
pid_t *operators = NULL;
pid_t *users = NULL;
struct StationInfo {
  int station_id;
  int station_avg_srvc;
  int curr_assignement;
  int max_assignement;
};

static void cleanup_ipc() {
  if (shm_id != -1) {
    remove_shared_memory(shm_id);
  }
  if (sem_id != -1) {
    remove_semaphores(sem_id);
  }
  if (msg_id != -1) {
    remove_message_queue(msg_id);
  }
  if (shm != NULL) {
    detach_shared_memory(shm);
  }
  if (operators != NULL) {
    free(operators);
  }
  if (users != NULL) {
    free(users);
  }
}

static void handle_signal(int sig) {
  (void)sig;
  exit(EXIT_FAILURE);
}

static void set_exit() {
  atexit(cleanup_ipc);

  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = handle_signal;
  sigaction(SIGINT, &sa, NULL);
}

static void initialize_shm() {
  shm_id = create_shared_memory(sizeof(struct SharedData));
  shm = attach_shared_memory(shm_id);
  memset(shm, 0, sizeof(struct SharedData));
}

static void initialize_sem() {
  sem_id = create_semaphore_set(NUM_SEMS);
  unsigned short sem_init_vals[NUM_SEMS];
  sem_init_vals[SEM_MUTEX_SHM] = 1;
  sem_init_vals[SEM_MUTEX_PORZIONI_P] = 1;
  sem_init_vals[SEM_MUTEX_STATS] = 1;
  sem_init_vals[SEM_MUTEX_PORZIONI_S] = 1;
  sem_init_vals[SEM_SEATS_PRIMI] = shm->config.nof_wk_seats_primi;
  sem_init_vals[SEM_SEATS_SECONDI] = shm->config.nof_wk_seats_secondi;
  sem_init_vals[SEM_SEATS_COFFEE] = shm->config.nof_wk_seats_coffee;
  sem_init_vals[SEM_SEATS_CASSA] = shm->config.nof_wk_seats_cassa;
  sem_init_vals[SEM_TABLE_SEATS] = shm->config.nof_table_seats;
  sem_init_vals[SEM_DAY_START] = 0;
  sem_init_vals[SEM_READY] = 0;
  set_all_semaphores(sem_id, sem_init_vals);
}

static int read_menu_file(const char *path) {
  FILE *f = fopen(path, "r");
  if (f == NULL) {
    return -1;
  }
  char line[256];
  int p_count = 0, s_count = 0;
  while (fgets(line, sizeof(line), f) != NULL) {
    if (strncmp(line, "PRIMO:", 6) == 0 && p_count < MAX_DISH_TYPES) {
      p_count++;
    } else if (strncmp(line, "SECONDO:", 8) == 0 && s_count < MAX_DISH_TYPES) {
      s_count++;
    }
  }
  fclose(f);
  shm->nof_type_primi = p_count;
  shm->nof_type_secondi = s_count;

  for (int i = 0; i < p_count; i++) {
    shm->portion_left_primi[i] = shm->config.avg_refill_primi;
  }
  for (int i = 0; i < s_count; i++) {
    shm->portion_left_secondi[i] = shm->config.avg_refill_secondi;
  }
  return 0;
}

static void sort_station_info(struct StationInfo station_info[]) {
  for (int i = 0; i < 3; i++) {
    for (int j = i + 1; j < 4; j++) {
      if (station_info[i].station_avg_srvc < station_info[j].station_avg_srvc) {
        struct StationInfo tmp = station_info[j];
        station_info[j] = station_info[i];
        station_info[i] = tmp;
      }
    }
  }
}

static int get_target_station(struct StationInfo station_info[], int index) {
  int target_station = -1;

  if (index < 4) {
    target_station = station_info[index].station_id;
    station_info[index].curr_assignement++;
  } else {
    int best = -1;
    for (int j = 0; j < 4; j++) {
      if (station_info[j].curr_assignement >= station_info[j].max_assignement)
        continue;
      if (best == -1 || station_info[j].station_avg_srvc *
                                station_info[best].curr_assignement >
                            station_info[best].station_avg_srvc *
                                station_info[j].curr_assignement) {
        best = j;
      }
    }
    if (best == -1) {
      fprintf(stderr, "WARNING: worker %d not assigned, all stations full\n",
              index);
      return -1;
    }
    target_station = station_info[best].station_id;
    station_info[best].curr_assignement++;
  }
  return target_station;
}

static pid_t spawn_operator(int target_station) {
  pid_t pid = fork();
  if (pid == -1) {
    perror("FORK FAILED");
    exit(EXIT_FAILURE);
  }

  if (pid == 0) {
    char shm_str[ARGC_MAX_LENGHT], sem_str[ARGC_MAX_LENGHT],
        msg_str[ARGC_MAX_LENGHT], target_station_str[ARGC_MAX_LENGHT];
    snprintf(shm_str, sizeof(shm_str), "%d", shm_id);
    snprintf(sem_str, sizeof(sem_str), "%d", sem_id);
    snprintf(msg_str, sizeof(msg_str), "%d", msg_id);
    snprintf(target_station_str, sizeof(target_station_str), "%d",
             target_station);

    if (target_station == STATION_CASSA) {
      execl(OPERATOR_CASSA_PATH, "operatore_cassa", shm_str, sem_str, msg_str,
            target_station_str, (char *)NULL);
    } else {
      execl(OPERATOR_PATH, "operatore", shm_str, sem_str, msg_str,
            target_station_str, (char *)NULL);
    }
    perror("EXEC FAILED");
    exit(EXIT_FAILURE);
  }

  return pid;
}

static pid_t spawn_user() {

  pid_t pid = fork();
  if (pid == -1) {
    perror("FORK FAILED");
    exit(EXIT_FAILURE);
  }
  if (pid == 0) {
    char shm_str[ARGC_MAX_LENGHT], sem_str[ARGC_MAX_LENGHT],
        msg_str[ARGC_MAX_LENGHT];
    snprintf(shm_str, sizeof(shm_str), "%d", shm_id);
    snprintf(sem_str, sizeof(sem_str), "%d", sem_id);
    snprintf(msg_str, sizeof(msg_str), "%d", msg_id);
    execl(USER_PATH, "utente", shm_str, sem_str, msg_str, NULL);
    perror("EXEC FAILED");
    exit(EXIT_FAILURE);
  }

  return pid;
}

static void initialize_operators() {

  struct StationInfo station_info[4] = {
      {STATION_PRIMI, shm->config.avg_srvc_primi, 0,
       shm->config.nof_wk_seats_primi},
      {STATION_SECONDI, shm->config.avg_srvc_secondi, 0,
       shm->config.nof_wk_seats_secondi},
      {STATION_COFFEE, shm->config.avg_srvc_coffee, 0,
       shm->config.nof_wk_seats_coffee},
      {STATION_CASSA, shm->config.avg_srvc_cassa, 0,
       shm->config.nof_wk_seats_cassa}};

  sort_station_info(station_info);

  for (int i = 0; i < shm->config.nof_workers; i++) {
    int target_station = get_target_station(station_info, i);
    if (target_station != -1) {
      operators[i] = spawn_operator(target_station);
    }
  }
}

static void initialize_users() {
  for (int i = 0; i < shm->config.nof_users; i++) {
    users[i] = spawn_user();
  }
}

int main(int argc, char *argv[]) {
  (void)argc;
  (void)argv;

  set_exit();

  initialize_shm();

  parse_config((argc > 1) ? argv[1] : NULL, &shm->config);

  if (read_menu_file(shm->config.menu_file) == -1) {
    perror("ERROR OPENING MENU FILE");
    exit(EXIT_FAILURE);
  }

  initialize_sem();

  msg_id = create_message_queue();

  operators = malloc(shm->config.nof_workers * sizeof(pid_t));
  if (operators == NULL) {
    perror("ERRROR MALLOC OPERATORS");
    exit(EXIT_FAILURE);
  }
  users = malloc(shm->config.nof_users * sizeof(pid_t));
  if (users == NULL) {
    perror("ERRROR MALLOC USERS");
    exit(EXIT_FAILURE);
  }

  initialize_operators();

  initialize_users();

  return 0;
}
