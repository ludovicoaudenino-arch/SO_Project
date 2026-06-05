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
#include "stats.h"
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;

static int shm_id = -1;
static int sem_id = -1;
static int msg_queues[NUM_QUEUES] = {-1, -1, -1, -1, -1};
static struct SharedData *shm = NULL;
static pid_t *operators = NULL;
static pid_t *users = NULL;

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
  for (int i = 0; i < NUM_QUEUES; i++) {
    if (msg_queues[i] != -1) {
      remove_message_queue(msg_queues[i]);
    }
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
 * @brief Discards all messages currently remaining in all message queues.
 *
 * This prevents stale requests from a previous day from leaking into and
 * corrupting the next day's simulation. Called when all children are safely
 * blocked at the daily startup barrier.
 */
static void purge_all_queues(struct SharedData *shm) {
  char buf[2048];
  for (int i = 0; i < NUM_QUEUES; i++) {
    while (msgrcv(shm->msg_queue_list[i], buf, sizeof(buf) - sizeof(long), 0,
                  IPC_NOWAIT) != -1) {
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

  struct sigaction sa = {0};
  sa.sa_handler = handle_signal;
  sigemptyset(&sa.sa_mask);
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
    perror("ERROR OPENING MENU' FILE");
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
  shm->stations[STATION_PRIMI].nof_type = p_count;
  shm->stations[STATION_SECONDI].nof_type = s_count;

  for (int i = 0; i < p_count; i++) {
    shm->stations[STATION_PRIMI].portion_left[i] = shm->config.avg_refill_primi;
  }
  for (int i = 0; i < s_count; i++) {
    shm->stations[STATION_SECONDI].portion_left[i] =
        shm->config.avg_refill_secondi;
  }
  return 0;
}

/**
 * @brief Sorts station indices by average service time in descending order.
 *
 * Builds a priority array of station indices sorted so that stations
 * with the longest average service time come first. This indirection
 * avoids modifying the shared memory stations array.
 *
 * @param stations The StationData array from shared memory.
 * @param priority Output array of NUM_STATIONS indices, sorted by avg_srvc
 * descending.
 */
static void sort_station_priority(const StationData stations[],
                                  int priority[]) {
  for (int i = 0; i < NUM_STATIONS; i++) {
    priority[i] = i;
  }
  for (int i = 0; i < NUM_STATIONS - 1; i++) {
    for (int j = i + 1; j < NUM_STATIONS; j++) {
      if (stations[priority[i]].avg_srvc < stations[priority[j]].avg_srvc) {
        int tmp = priority[i];
        priority[i] = priority[j];
        priority[j] = tmp;
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
 * @param stations The StationData array from shared memory.
 * @param priority Array of station indices sorted by priority.
 * @param assigned Local array tracking how many operators have been assigned
 * per station.
 * @param index Zero-based index of the operator being assigned.
 * @return Station ID on success
 */
static int get_target_station(const StationData stations[],
                              const int priority[], int assigned[], int index) {
  if (index < NUM_STATIONS) {
    int sid = priority[index];
    assigned[sid]++;
    return sid;
  }

  int best = 0;
  for (int j = 1; j < NUM_STATIONS; j++) {
    int sid = priority[j];
    if (stations[sid].avg_srvc * assigned[best] >
        stations[best].avg_srvc * assigned[sid]) {
      best = sid;
    }
  }
  assigned[best]++;
  return best;
}

/**
 * @brief Forks and execs an operator process for the given station.
 *
 * Passes IPC identifiers (shm_id, sem_id, msg_id) and the target station
 * as command-line arguments.
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
    char shm_str[ARGC_MAX_LENGTH], target_station_str[ARGC_MAX_LENGTH];
    snprintf(shm_str, sizeof(shm_str), "%d", shm_id);
    snprintf(target_station_str, sizeof(target_station_str), "%d",
             target_station);
    char *const child_argv[] = {"operatore", shm_str, target_station_str, NULL};
    execve(OPERATOR_PATH, child_argv, environ);
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
    char shm_str[ARGC_MAX_LENGTH];
    snprintf(shm_str, sizeof(shm_str), "%d", shm_id);
    char *const child_argv[] = {"utente", shm_str, NULL};
    execve(USER_PATH, child_argv, environ);
    perror("EXEC FAILED");
    exit(EXIT_FAILURE);
  }

  return pid;
}

/**
 * @brief Spawns all operator processes using the station assignment policy.
 *
 * Sorts station indices by service time priority and assigns each operator
 * to the most appropriate station via get_target_station().
 */
static void initialize_operators() {
  int priority[NUM_STATIONS];
  int assigned[NUM_STATIONS] = {0};

  sort_station_priority(shm->stations, priority);

  for (int i = 0; i < shm->config.nof_workers; i++) {
    int target_station =
        get_target_station(shm->stations, priority, assigned, i);
    operators[i] = spawn_operator(target_station);
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

/**
 * @brief Initializes the parameters and capacities of all cafeteria stations.
 *
 * Configures the service times, delta ranges, semaphore indices, message
 * types, queue lengths, active operator counts, and maximum capacities for
 * first course, second course, coffee, and checkout (cassa) stations using
 * configuration values loaded in the shared memory.
 *
 * @param shm Pointer to the SharedData structure in shared memory.
 */
static void initialize_stations(struct SharedData *shm) {
  shm->stations[STATION_PRIMI].avg_srvc = shm->config.avg_srvc_primi;
  shm->stations[STATION_PRIMI].srvc_delta = 50;
  shm->stations[STATION_PRIMI].sem_seats_index = SEM_SEATS_PRIMI;
  shm->stations[STATION_PRIMI].msg_type = ORDER_PRIMI_TYPE;
  shm->stations[STATION_PRIMI].queue_length = 0;
  shm->stations[STATION_PRIMI].active_operators = 0;
  shm->stations[STATION_PRIMI].max_operators = shm->config.nof_wk_seats_primi;
  shm->stations[STATION_PRIMI].avg_refill = shm->config.avg_refill_primi;
  shm->stations[STATION_PRIMI].max_portions = shm->config.max_porzioni_primi;

  shm->stations[STATION_SECONDI].avg_srvc = shm->config.avg_srvc_secondi;
  shm->stations[STATION_SECONDI].srvc_delta = 50;
  shm->stations[STATION_SECONDI].sem_seats_index = SEM_SEATS_SECONDI;
  shm->stations[STATION_SECONDI].msg_type = ORDER_SECONDI_TYPE;
  shm->stations[STATION_SECONDI].queue_length = 0;
  shm->stations[STATION_SECONDI].active_operators = 0;
  shm->stations[STATION_SECONDI].max_operators =
      shm->config.nof_wk_seats_secondi;
  shm->stations[STATION_SECONDI].avg_refill = shm->config.avg_refill_secondi;
  shm->stations[STATION_SECONDI].max_portions =
      shm->config.max_porzioni_secondi;

  shm->stations[STATION_COFFEE].avg_srvc = shm->config.avg_srvc_coffee;
  shm->stations[STATION_COFFEE].srvc_delta = 80;
  shm->stations[STATION_COFFEE].sem_seats_index = SEM_SEATS_COFFEE;
  shm->stations[STATION_COFFEE].msg_type = ORDER_COFFEE_TYPE;
  shm->stations[STATION_COFFEE].queue_length = 0;
  shm->stations[STATION_COFFEE].active_operators = 0;
  shm->stations[STATION_COFFEE].max_operators = shm->config.nof_wk_seats_coffee;
  shm->stations[STATION_COFFEE].nof_type = 1;

  shm->stations[STATION_CASSA].avg_srvc = shm->config.avg_srvc_cassa;
  shm->stations[STATION_CASSA].srvc_delta = 20;
  shm->stations[STATION_CASSA].sem_seats_index = SEM_SEATS_CASSA;
  shm->stations[STATION_CASSA].msg_type = ORDER_CASSA_TYPE;
  shm->stations[STATION_CASSA].queue_length = 0;
  shm->stations[STATION_CASSA].active_operators = 0;
  shm->stations[STATION_CASSA].max_operators = shm->config.nof_wk_seats_cassa;
}

int main(int argc, char *argv[]) {

  /* --- Phase 1: Setup signal handlers and atexit cleanup --- */
  set_exit();

  /* --- Phase 2: Create IPC resources and parse configuration --- */
  initialize_shm();

  parse_config((argc > 1) ? argv[1] : NULL, &shm->config);

  initialize_stations(shm);

  if (read_menu_file(shm->config.menu_file) == -1) {
    exit(EXIT_FAILURE);
  }

  initialize_sem();
  shm->sem_id = sem_id;

  for (int i = 0; i < NUM_QUEUES; i++) {
    msg_queues[i] = create_message_queue();
    shm->msg_queue_list[i] = msg_queues[i];
  }

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

  shm->simulation_running = 1;

  initialize_operators();

  initialize_users();
  /* --- Phase 4: Daily simulation loop --- */
  for (int current_day = 1; current_day <= shm->config.sim_duration;
       current_day++) {
    /* Wait for all children to signal readiness at the barrier */
    sem_op(sem_id, SEM_READY, -TOTAL_CHILDREN(shm), 0);

    /* Purge stale messages from queues before starting a new day */
    purge_all_queues(shm);

    /* Prepare the new day state under mutex protection */
    sem_op(sem_id, SEM_MUTEX_SHM, -1, SEM_UNDO);
    shm->current_day = current_day;
    shm->day_running = 1;
    for (int j = 0; j < shm->stations[STATION_PRIMI].nof_type; j++) {
      shm->stations[STATION_PRIMI].portion_left[j] =
          shm->config.avg_refill_primi;
    }
    for (int j = 0; j < shm->stations[STATION_SECONDI].nof_type; j++) {
      shm->stations[STATION_SECONDI].portion_left[j] =
          shm->config.avg_refill_secondi;
    }
    sem_op(sem_id, SEM_MUTEX_SHM, +1, SEM_UNDO);

    /* Broadcast day start: unblock all children simultaneously */

    sem_op(sem_id, SEM_DAY_START, +TOTAL_CHILDREN(shm), 0);
    for (int i = 0; i < (WORK_MINUTE(8) / REFILL_INTERVAL); i++) {

      sim_sleep(SECOND_TO_REFILL, shm->config.n_nano_secs);
      sem_op(sem_id, SEM_MUTEX_SHM, -1, SEM_UNDO);
      for (int j = 0; j < NUM_STATIONS - 1; j++) {

        int nof_type = shm->stations[j].nof_type;
        int max_portions = shm->stations[j].max_portions;
        int avg_refill = shm->stations[j].avg_refill;

        for (int n = 0; n < nof_type; n++) {

          int *portion_left = &shm->stations[j].portion_left[n];
          *portion_left = MIN(*portion_left + avg_refill, max_portions);
        }
      }
      sem_op(sem_id, SEM_MUTEX_SHM, +1, SEM_UNDO);
    }
    /* Signal end of service for this day */
    sem_op(sem_id, SEM_MUTEX_SHM, -1, SEM_UNDO);
    shm->day_running = 0;
    print_daily_stats(&shm->sim_stats, current_day);
    accumulate_and_reset_daily_stats(&shm->sim_stats);
    sem_op(sem_id, SEM_MUTEX_SHM, +1, SEM_UNDO);

    /* Send SIGUSR1 to interrupt any blocking receive_message */
    for (int i = 0; i < shm->config.nof_workers; i++) {
      kill(operators[i], SIGUSR1);
    }
    for (int i = 0; i < shm->config.nof_users; i++) {
      kill(users[i], SIGUSR1);
    }
  }

  /* --- Phase 5: Graceful shutdown --- */
  sem_op(sem_id, SEM_MUTEX_SHM, -1, SEM_UNDO);
  shm->simulation_running = 0;
  sem_op(sem_id, SEM_MUTEX_SHM, +1, SEM_UNDO);

  /* Send SIGUSR1 to all child processes one last time to make sure they wake up
   */
  for (int i = 0; i < shm->config.nof_workers; i++) {
    kill(operators[i], SIGUSR1);
  }
  for (int i = 0; i < shm->config.nof_users; i++) {
    kill(users[i], SIGUSR1);
  }

  /* Wake up any children waiting on SEM_DAY_START so they check
   * simulation_running and exit */
  sem_op(sem_id, SEM_DAY_START, +TOTAL_CHILDREN(shm), 0);

  /* Reap all child processes before IPC cleanup runs via atexit */
  wait_for_children();

  print_final_stats(&shm->sim_stats, 0);

  exit(0);
}
