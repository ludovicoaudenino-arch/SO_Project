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
 * @brief Real-time status and configuration of an individual cafeteria station.
 */
typedef struct StationData {
  /** @name Static Properties (initialized at startup) */
  /** @{ */
  int avg_srvc;   /**< @brief Average service time for this station (in simulated seconds) */
  int srvc_delta; /**< @brief Delta variation range for service time randomness */
  int sem_seats_index; /**< @brief Semaphore index representing operator seats in the queue */
  int msg_type;        /**< @brief Message type used for requests routed to this station */
  /** @} */

  /** @name Dynamic State */
  /** @{ */
  int queue_length;     /**< @brief Current number of users waiting in the queue */
  int active_operators; /**< @brief Number of operators currently working at this station */
  int max_operators;    /**< @brief Maximum number of operators allowed at this station (configured capacity) */
  int portion_left[MAX_DISH_TYPES]; /**< @brief Remaining portions per dish variant (used for food stations) */
  int nof_type;         /**< @brief Number of distinct dish variants loaded from the menu for this station */
  int avg_refill;       /**< @brief Average portion refill size per interval for this station */
  int max_portions;     /**< @brief Maximum portion capacity limit for each dish variant at this station */
  /** @} */
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
  int simulation_running; /**< @brief Flag: 1 if the simulation is active, 0 if terminated */
  int day_running;        /**< @brief Flag: 1 if a daily cycle is in progress, 0 otherwise */
  int current_day;        /**< @brief Current simulated day number (from 1 to sim_duration) */
  int termination_cause;  /**< @brief Reason for termination (0 = timeout/end of days, 1 = queue overload, 2 = external SIGINT) */
  /** @} */

  /** @name Station States */
  /** @{ */
  StationData stations[NUM_STATIONS]; /**< @brief Real-time status and properties of all stations */
  /** @} */

  /** @name Message Queues */
  /** @{ */
  int msg_queue_list[NUM_QUEUES]; /**< @brief Message queue identifiers: [0..NUM_QUEUES-2] for station requests, [NUM_QUEUES-1] for served replies */
  /** @} */

  /** @name Semaphores */
  /** @{ */
  int sem_id; /**< @brief System V semaphore set ID */
  /** @} */

  /** @name Statistics and Configuration */
  /** @{ */
  SimStats sim_stats; /**< @brief Aggregate simulation statistics (daily and cumulative) */
  Config config;      /**< @brief Read-only copy of the parsed configuration for child processes */
  /** @} */
};

#endif
