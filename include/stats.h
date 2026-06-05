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
  long wait_time_total_today;        /**< @brief Cumulative wait time in nanoseconds today */
  long wait_time_primi_today;        /**< @brief Cumulative wait time at First Course today */
  long wait_time_secondi_today;      /**< @brief Cumulative wait time at Main Course today */
  long wait_time_coffee_today;       /**< @brief Cumulative wait time at Coffee station today */
  long wait_time_cassa_today;        /**< @brief Cumulative wait time at Cashier today */
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
  long wait_time_total_sim;          /**< @brief Cumulative wait time across all days */
  long wait_time_primi_sim;          /**< @brief Cumulative First Course wait time */
  long wait_time_secondi_sim;        /**< @brief Cumulative Main Course wait time */
  long wait_time_coffee_sim;         /**< @brief Cumulative Coffee station wait time */
  long wait_time_cassa_sim;          /**< @brief Cumulative Cashier wait time */
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
 * @param s Pointer to the SimStats structure in shared memory.
 * @param day The day number (1-indexed).
 */
void print_daily_stats(const SimStats *s, int day);

/**
 * @brief Prints the final simulation summary.
 * @param s Pointer to the SimStats structure in shared memory.
 * @param termination_cause Reason code for simulation end (e.g., timeout or overload).
 */
void print_final_stats(const SimStats *s, int termination_cause);

/**
 * @brief Accumulates daily statistics into the simulation totals and resets daily counters.
 * @param s Pointer to the SimStats structure in shared memory.
 */
void accumulate_and_reset_daily_stats(SimStats *s);

#endif
