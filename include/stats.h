#ifndef STATS_H
#define STATS_H

/**
 * @file stats.h
 * @brief Simulation statistics data structure and reporting functions.
 *
 * Defines the SimStats structure that resides in shared memory
 * and the functions for printing daily and final reports.
 */

/**
 * @struct SimStats
 * @brief Holds all daily and cumulative simulation statistics.
 *
 * This structure is placed in shared memory and accessed by all
 * processes. All writes must be protected by SEM_MUTEX_STATS.
 */
typedef struct {
  /** @name Daily counters (reset at the start of each day) */
  /** @{ */
  int users_served_today;            /**< @brief Users who completed the full service today */
  int users_not_served_today;        /**< @brief Users who gave up or were not served today */
  int dishes_primi_today;            /**< @brief First Course dishes served today */
  int dishes_secondi_today;          /**< @brief Main Course dishes served today */
  int dishes_coffee_today;           /**< @brief Coffee/Dessert items served today */
  int dishes_leftover_primi_today;   /**< @brief Unsold First Course portions at end of day */
  int dishes_leftover_secondi_today; /**< @brief Unsold Main Course portions at end of day */
  long wait_time_total_today;        /**< @brief Cumulative wait time in simulated seconds today */
  long wait_time_primi_today;        /**< @brief Cumulative wait time at First Course today (simulated seconds) */
  long wait_time_secondi_today;      /**< @brief Cumulative wait time at Main Course today (simulated seconds) */
  long wait_time_coffee_today;       /**< @brief Cumulative wait time at Coffee station today (simulated seconds) */
  long wait_time_cassa_today;        /**< @brief Cumulative wait time at Cashier today (simulated seconds) */
  int wait_count_today;              /**< @brief Number of wait events today (for averaging) */
  int wait_count_primi_today;        /**< @brief Wait events at First Course today */
  int wait_count_secondi_today;      /**< @brief Wait events at Main Course today */
  int wait_count_coffee_today;       /**< @brief Wait events at Coffee station today */
  int wait_count_cassa_today;        /**< @brief Wait events at Cashier today */
  int active_operators_today;        /**< @brief Number of active operators today */
  int pauses_today;                  /**< @brief Total worker breaks taken today */
  float revenue_today;               /**< @brief Revenue collected today in EUR */
  /** @} */

  /** @name Cumulative counters (across the entire simulation) */
  /** @{ */
  int users_served_total;            /**< @brief Total users served across all days */
  int users_not_served_total;        /**< @brief Total users not served across all days */
  int dishes_primi_total;            /**< @brief Total First Course dishes served */
  int dishes_secondi_total;          /**< @brief Total Main Course dishes served */
  int dishes_coffee_total;           /**< @brief Total Coffee/Dessert items served */
  int dishes_leftover_primi_total;   /**< @brief Total unsold First Course portions */
  int dishes_leftover_secondi_total; /**< @brief Total unsold Main Course portions */
  long wait_time_total_sim;          /**< @brief Cumulative wait time across all days (simulated seconds) */
  long wait_time_primi_sim;          /**< @brief Cumulative First Course wait time (simulated seconds) */
  long wait_time_secondi_sim;        /**< @brief Cumulative Main Course wait time (simulated seconds) */
  long wait_time_coffee_sim;         /**< @brief Cumulative Coffee station wait time (simulated seconds) */
  long wait_time_cassa_sim;          /**< @brief Cumulative Cashier wait time (simulated seconds) */
  int wait_count_total_sim;          /**< @brief Total wait events across all days */
  int wait_count_primi_sim;          /**< @brief Total First Course wait events */
  int wait_count_secondi_sim;        /**< @brief Total Main Course wait events */
  int wait_count_coffee_sim;         /**< @brief Total Coffee station wait events */
  int wait_count_cassa_sim;          /**< @brief Total Cashier wait events */
  int active_operators_total;        /**< @brief Total operator activations */
  int pauses_total;                  /**< @brief Total worker breaks across all days */
  float revenue_total;               /**< @brief Total revenue across all days in EUR */
  int days_completed;                /**< @brief Number of simulation days completed */
  /** @} */
} SimStats;

/**
 * @brief Prints the statistics summary for a single day.
 *
 * Output includes total served/not served users, count of served dishes
 * per course, leftovers, average simulated wait times per station, active
 * operators, breaks, and daily revenue.
 *
 * @param s Pointer to the SimStats structure in shared memory.
 * @param day The day number (1-indexed).
 */
void print_daily_stats(const SimStats *s, int day);

/**
 * @brief Prints the final simulation summary.
 *
 * Prints cumulative values across all days, average wait times,
 * and the cause of simulation termination (timeout, overload, or signal).
 *
 * @param s Pointer to the SimStats structure in shared memory.
 * @param termination_cause Reason code for simulation end (0 = timeout, 1 = overload, 2 = SIGINT).
 */
void print_final_stats(const SimStats *s, int termination_cause);

/**
 * @brief Accumulates daily statistics into the simulation totals and resets daily counters.
 *
 * Prepares the shared SimStats structure for the next simulation day by adding
 * current daily counters to final cumulative statistics and resetting daily counters.
 *
 * @param s Pointer to the SimStats structure in shared memory.
 */
void accumulate_and_reset_daily_stats(SimStats *s);

/**
 * @brief Increments the daily served users counter.
 *
 * This function is process-safe and internally acquires/releases SEM_MUTEX_STATS.
 *
 * @param s Pointer to the SimStats structure.
 * @param sem_id System V semaphore set ID.
 */
void stats_record_user_served(SimStats *s, int sem_id);

/**
 * @brief Increments the daily not-served users counter.
 *
 * This function is process-safe and internally acquires/releases SEM_MUTEX_STATS.
 *
 * @param s Pointer to the SimStats structure.
 * @param sem_id System V semaphore set ID.
 */
void stats_record_user_not_served(SimStats *s, int sem_id);

/**
 * @brief Records a dish serving event at a given station.
 *
 * This function is process-safe and internally acquires/releases SEM_MUTEX_STATS.
 * Increments the appropriate daily dish counter depending on the station type.
 *
 * @param s Pointer to the SimStats structure.
 * @param sem_id System V semaphore set ID.
 * @param station_type Station index where the dish was served (STATION_PRIMI, STATION_SECONDI, STATION_COFFEE).
 */
void stats_record_dish_served(SimStats *s, int sem_id, int station_type);

/**
 * @brief Records revenue collected from checkout transactions.
 *
 * This function is process-safe and internally acquires/releases SEM_MUTEX_STATS.
 * Adds the transaction amount to the daily collected revenue.
 *
 * @param s Pointer to the SimStats structure.
 * @param sem_id System V semaphore set ID.
 * @param amount Amount collected in EUR.
 */
void stats_record_revenue(SimStats *s, int sem_id, float amount);

/**
 * @brief Increments the daily operator pauses/breaks counter.
 *
 * This function is process-safe and internally acquires/releases SEM_MUTEX_STATS.
 *
 * @param s Pointer to the SimStats structure.
 * @param sem_id System V semaphore set ID.
 */
void stats_record_pause(SimStats *s, int sem_id);

/**
 * @brief Records an operator activation event.
 *
 * This function is process-safe and internally acquires/releases SEM_MUTEX_STATS.
 * Increments both daily and total active operator counters.
 *
 * @param s Pointer to the SimStats structure.
 * @param sem_id System V semaphore set ID.
 */
void stats_record_active_operator(SimStats *s, int sem_id);

/**
 * @brief Records the queue wait time experienced by a user at a station.
 *
 * This function is process-safe and internally acquires/releases SEM_MUTEX_STATS.
 * Increments the general daily wait time/count, and logs wait details for the
 * specific station type.
 *
 * @param s Pointer to the SimStats structure.
 * @param sem_id System V semaphore set ID.
 * @param station_type Station type where the wait occurred (STATION_PRIMI, STATION_SECONDI, STATION_COFFEE, STATION_CASSA).
 * @param wait_time_sim_sec Wait duration in simulated seconds.
 */
void stats_record_wait_time(SimStats *s, int sem_id, int station_type, long wait_time_sim_sec);

#endif
