#ifndef SHARED_DATA_H
#define SHARED_DATA_H

/**
 * @file shared_data.h
 * @brief Shared memory data structures for inter-process communication.
 *
 * Defines the global SharedData structure that resides in a single
 * System V shared memory segment. All child processes (operators,
 * cashiers, users) attach to this segment for real-time coordination.
 */

#include "common.h"
#include "config.h"
#include "stats.h"

/**
 * @struct StationData
 * @brief Real-time status of an individual cafeteria station.
 */
typedef struct StationData {
  int queue_length;     /**< @brief Current number of users waiting in queue */
  int active_operators; /**< @brief Number of operators currently working here */
} StationData;

/**
 * @struct SharedData
 * @brief The global shared memory block for the entire simulation.
 *
 * This structure is allocated once by the parent process (responsabile_mensa)
 * and attached by all children. All write accesses must be protected by the
 * appropriate semaphore (e.g., SEM_MUTEX_SHM, SEM_MUTEX_STATS).
 */
struct SharedData {
  /** @name Control Flags */
  /** @{ */
  int simulation_running; /**< @brief 1 if simulation is active, 0 if terminated */
  int day_running;        /**< @brief 1 if a daily cycle is in progress, 0 otherwise */
  int current_day;        /**< @brief Current simulated day number (1 to sim_duration) */
  int termination_cause;  /**< @brief Reason for termination (0 = timeout, 1 = overload) */
  int all_ready;          /**< @brief Synchronization counter for the startup barrier */
  /** @} */

  /** @name Station States */
  /** @{ */
  StationData station_primi;   /**< @brief Status of the First Course station */
  StationData station_secondi; /**< @brief Status of the Main Course station */
  StationData station_coffee;  /**< @brief Status of the Coffee/Dessert station */
  StationData station_cassa;   /**< @brief Status of the Cashier station */
  /** @} */

  /** @name Food Portions */
  /** @{ */
  int portion_left_primi[MAX_DISH_TYPES];   /**< @brief Remaining portions per First Course dish type */
  int portion_left_secondi[MAX_DISH_TYPES]; /**< @brief Remaining portions per Main Course dish type */
  int nof_type_primi;                       /**< @brief Number of distinct First Course dishes loaded from menu */
  int nof_type_secondi;                     /**< @brief Number of distinct Main Course dishes loaded from menu */
  /** @} */

  /** @name Tables */
  /** @{ */
  int table_seats_free; /**< @brief Number of free seats at the dining tables */
  /** @} */

  /** @name Message Queues */
  /** @{ */
  int msg_queue_requests[4]; /**< @brief Request message queues for Primi, Secondi, Coffee, Cassa */
  int msg_queue_served;      /**< @brief Shared response message queue (served) */
  /** @} */

  /** @name Semaphores */
  /** @{ */
  int sem_id;                /**< @brief System V semaphore set ID */
  /** @} */



  /** @name Statistics and Configuration */
  /** @{ */
  SimStats sim_stats; /**< @brief Aggregate simulation statistics (daily and cumulative) */
  Config config;      /**< @brief Read-only copy of the parsed configuration for child processes */
  /** @} */
};

#endif
