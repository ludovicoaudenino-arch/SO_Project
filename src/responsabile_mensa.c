/**
 * @file responsabile_mensa.c
 * @brief Main coordinator process for the cafeteria simulation.
 *
 * This process creates all IPC resources (shared memory, semaphores,
 * message queue), spawns operator and user child processes, and
 * orchestrates the daily simulation cycle including synchronization
 * barriers, portion refills, and graceful shutdown.
 */

#include "common.h"
#include "config.h"
#include "ipc_utils.h"
#include "shared_data.h"
#include "time_utils.h"
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define OPERATOR_CASSA_PATH "./bin/operatore_cassa"
#define OPERATOR_PATH "./bin/operatore"
#define USER_PATH "./bin/utente"
#define ARGC_MAX_LENGTH 16
#define TOTAL_CHILDREN ((shm->config.nof_workers) + (shm->config.nof_users))
#define SIM_DAY_SECOND (8 * 60 * 60)

static int shm_id = -1;
static int sem_id = -1;
static int msg_id = -1;
static struct SharedData *shm = NULL;
static pid_t *operators = NULL;
static pid_t *users = NULL;
struct StationInfo {
  int station_id;
  int station_avg_srvc;
  int curr_assignement;
  int max_assignement;
};

/**
 * @brief Releases all IPC resources and dynamic memory at process exit.
 *
 * Registered via atexit() to guarantee cleanup even on abnormal
 * termination. Detaches shared memory before removing it to avoid
 * operating on an already-destroyed segment.
 */
static void cleanup_ipc() {
  if (shm != NULL) {
    detach_shared_memory(shm);
  }
  if (shm_id != -1) {
    remove_shared_memory(shm_id);
  }
  if (sem_id != -1) {
    remove_semaphores(sem_id);
  }
  if (msg_id != -1) {
    remove_message_queue(msg_id);
  }
  if (operators != NULL) {
    free(operators);
  }
  if (users != NULL) {
    free(users);
  }
}

/**
 * @brief Blocks until every spawned child process has terminated.
 *
 * Iterates over the operators and users PID arrays, calling waitpid()
 * on each one. Must be called before cleanup_ipc() to ensure children
 * have finished using IPC resources.
 */
static void wait_for_children() {
  if (operators != NULL) {
    for (int i = 0; i < shm->config.nof_workers; i++) {
      waitpid(operators[i], NULL, 0);
    }
  }
  if (users != NULL) {
    for (int i = 0; i < shm->config.nof_users; i++) {
      waitpid(users[i], NULL, 0);
    }
  }
}

/**
 * @brief Signal handler for SIGINT.
 *
 * Triggers a clean exit which in turn invokes the atexit-registered
 * cleanup_ipc() function to release all IPC resources.
 *
 * @param sig Signal number (unused).
 */
static void handle_signal(int sig) {
  (void)sig;
  exit(EXIT_FAILURE);
}

/**
 * @brief Registers cleanup handlers and signal dispositions.
 *
 * Registers cleanup_ipc() with atexit() and installs handle_signal()
 * as the SIGINT handler via sigaction().
 */
static void set_exit() {
  atexit(cleanup_ipc);

  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = handle_signal;
  sigaction(SIGINT, &sa, NULL);
}

/**
 * @brief Creates and attaches the shared memory segment.
 *
 * Allocates a System V shared memory segment sized for SharedData,
 * attaches it to the process address space, and zero-initializes it.
 */
static void initialize_shm() {
  shm_id = create_shared_memory(sizeof(struct SharedData));
  shm = attach_shared_memory(shm_id);
  memset(shm, 0, sizeof(struct SharedData));
}

/**
 * @brief Creates and initializes the System V semaphore set.
 *
 * Allocates NUM_SEMS semaphores and sets their initial values:
 * mutexes to 1, station seats to the configured capacity,
 * and barrier semaphores (SEM_READY, SEM_DAY_START) to 0.
 */
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

/**
 * @brief Parses the menu file and initializes food portions in shared memory.
 *
 * Counts the number of first and second course types by scanning for
 * "PRIMO:" and "SECONDO:" prefixes, then sets the initial portion
 * quantities for each dish type.
 *
 * @param path Path to the menu file.
 * @return 0 on success, -1 if the file cannot be opened.
 */
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

/**
 * @brief Sorts stations by average service time in descending order.
 *
 * Uses selection sort to prioritize stations with longer service times
 * during the operator assignment phase.
 *
 * @param station_info Array of NUM_STATIONS StationInfo structs to sort.
 */
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

/**
 * @brief Determines which station to assign to the operator at the given index.
 *
 * The first NUM_STATIONS operators are assigned one per station (in priority
 * order). Additional operators are distributed using a weighted ratio that
 * balances service time against current assignment count.
 *
 * @param station_info Array of station metadata (modified in place).
 * @param index Zero-based index of the operator being assigned.
 * @return Station ID on success, -1 if all stations are at capacity.
 */
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

/**
 * @brief Forks and execs an operator process for the given station.
 *
 * Passes IPC identifiers (shm_id, sem_id, msg_id) and the target station
 * as command-line arguments. Cashier operators use a separate executable.
 *
 * @param target_station Station ID to assign (STATION_PRIMI, etc.).
 * @return PID of the spawned child process.
 */
static pid_t spawn_operator(int target_station) {
  pid_t pid = fork();
  if (pid == -1) {
    perror("FORK FAILED");
    exit(EXIT_FAILURE);
  }

  if (pid == 0) {
    char shm_str[ARGC_MAX_LENGTH], sem_str[ARGC_MAX_LENGTH],
        msg_str[ARGC_MAX_LENGTH], target_station_str[ARGC_MAX_LENGTH];
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

/**
 * @brief Forks and execs a user (utente) process.
 *
 * Passes IPC identifiers (shm_id, sem_id, msg_id) as command-line
 * arguments to the child process.
 *
 * @return PID of the spawned child process.
 */
static pid_t spawn_user() {
  pid_t pid = fork();
  if (pid == -1) {
    perror("FORK FAILED");
    exit(EXIT_FAILURE);
  }
  if (pid == 0) {
    char shm_str[ARGC_MAX_LENGTH], sem_str[ARGC_MAX_LENGTH],
        msg_str[ARGC_MAX_LENGTH];
    snprintf(shm_str, sizeof(shm_str), "%d", shm_id);
    snprintf(sem_str, sizeof(sem_str), "%d", sem_id);
    snprintf(msg_str, sizeof(msg_str), "%d", msg_id);
    execl(USER_PATH, "utente", shm_str, sem_str, msg_str, NULL);
    perror("EXEC FAILED");
    exit(EXIT_FAILURE);
  }

  return pid;
}

/**
 * @brief Spawns all operator processes using the station assignment policy.
 *
 * Builds a StationInfo array from the current configuration, sorts it by
 * service time priority, and assigns each operator to the most appropriate
 * station via get_target_station().
 */
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

/**
 * @brief Spawns all user (utente) processes.
 *
 * Iterates over the configured number of users and stores each
 * child PID in the users array for later waitpid() collection.
 */
static void initialize_users() {
  for (int i = 0; i < shm->config.nof_users; i++) {
    users[i] = spawn_user();
  }
}

int main(int argc, char *argv[]) {

  /* --- Phase 1: Setup signal handlers and atexit cleanup --- */
  set_exit();

  /* --- Phase 2: Create IPC resources and parse configuration --- */
  initialize_shm();

  parse_config((argc > 1) ? argv[1] : NULL, &shm->config);

  if (read_menu_file(shm->config.menu_file) == -1) {
    perror("ERROR OPENING MENU FILE");
    exit(EXIT_FAILURE);
  }

  initialize_sem();

  msg_id = create_message_queue();

  /* --- Phase 3: Allocate PID arrays and spawn child processes --- */
  operators = malloc(shm->config.nof_workers * sizeof(pid_t));
  if (operators == NULL) {
    perror("ERROR MALLOC OPERATORS");
    exit(EXIT_FAILURE);
  }
  users = malloc(shm->config.nof_users * sizeof(pid_t));
  if (users == NULL) {
    perror("ERROR MALLOC USERS");
    exit(EXIT_FAILURE);
  }

  initialize_operators();

  initialize_users();

  shm->simulation_running = 1;

  /* --- Phase 4: Daily simulation loop --- */
  for (int current_day = 1; current_day <= shm->config.sim_duration;
       current_day++) {
    /* Wait for all children to signal readiness at the barrier */
    sem_op(sem_id, SEM_READY, -TOTAL_CHILDREN, 0);

    /* Prepare the new day state under mutex protection */
    sem_op(sem_id, SEM_MUTEX_SHM, -1, 0);
    shm->current_day = current_day;
    shm->day_running = 1;
    for (int j = 0; j < shm->nof_type_primi; j++) {
      shm->portion_left_primi[j] = shm->config.avg_refill_primi;
    }
    for (int j = 0; j < shm->nof_type_secondi; j++) {
      shm->portion_left_secondi[j] = shm->config.avg_refill_secondi;
    }
    sem_op(sem_id, SEM_MUTEX_SHM, +1, 0);

    /* Broadcast day start: unblock all children simultaneously */
    sem_op(sem_id, SEM_DAY_START, +TOTAL_CHILDREN, 0);

    /* Let the simulated workday elapse (8 hours) */
    sim_sleep(SIM_DAY_SECOND, shm->config.n_nano_secs);
    /* Signal end of service for this day */
    sem_op(sem_id, SEM_MUTEX_SHM, -1, 0);
    shm->day_running = 0;
    sem_op(sem_id, SEM_MUTEX_SHM, +1, 0);
  }

  /* --- Phase 5: Graceful shutdown --- */
  sem_op(sem_id, SEM_MUTEX_SHM, -1, 0);
  shm->simulation_running = 0;
  sem_op(sem_id, SEM_MUTEX_SHM, +1, 0);

  /* Reap all child processes before IPC cleanup runs via atexit */
  wait_for_children();

  exit(0);
}
