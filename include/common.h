#ifndef COMMON_H
#define COMMON_H

/**
 * @file common.h
 * @brief Global constants, identifiers, and macros for the Cafeteria
 * simulation.
 *
 * This file contains definitions shared across all processes,
 * including data structure limits, station indices,
 * and identifiers for the System V IPC components.
 */

#include <sys/types.h>

/* ========================================================================== */
/*                          MESSAGE QUEUE CONFIGURATION                       */
/* ========================================================================== */

/**
 * @name Message Queue Configuration
 * Global constraints on the number of active message queues.
 * @{
 */
#define NUM_QUEUES                                                             \
  5 /**< @brief Total number of message queues (4 station queues + 1 response  \
       queue) */
/** @} */

/**
 * @name Message Type Identifiers
 * Used to route messages to specific stations and retrieve them by PID.
 * Each station has its own dedicated message queue, so a single type
 * value is sufficient for request routing within each queue.
 * @{
 */
#define ORDER_TYPE 1 /**< @brief Message type for all station order requests */
/** @} */

/**
 * @brief Macro to compute message body size for msgsnd/msgrcv.
 *
 * System V message queues expect the payload size excluding the leading
 * @c long @c mtype field. This macro computes that size for any message
 * struct whose first member is @c long @c mytype.
 */
#define MSG_CONTENT_SIZE(msg_struct) (sizeof(msg_struct) - sizeof(long))

/* --- Message Queue Payload Structures --- */

/**
 * @struct ServingMsg
 * @brief Represents a request for a single dish (Primi or Secondi) at a food
 * station.
 *
 * Sent by users (utente) to a specific station's request queue.
 */
typedef struct {
  long mytype;   /**< @brief Message type (used for routing or filter, must be >
                    0) */
  int dish_type; /**< @brief Index of the dish variant requested (0 to
                    MAX_DISH_TYPES - 1) */
  pid_t pid;     /**< @brief Process ID of the requesting user (utente) */
} ServingMsg;

/**
 * @struct ServedMsg
 * @brief Represents the reply indicating whether a user was successfully
 * served.
 *
 * Sent by operators to the common served message queue, addressed to the user's
 * PID.
 */
typedef struct {
  long mytype; /**< @brief Message type (set to the target user's PID for
                  routing) */
  int served; /**< @brief Service outcome (1 if successfully served, 0 if out of
                 portions/failed) */
  pid_t pid;  /**< @brief Process ID of the operator that handled the request */
} ServedMsg;

/**
 * @struct OrderMsg
 * @brief Represents the checkout summary sent to the cashier (Cassa) station.
 *
 * Sent by users (utente) to the cashier station to record consumption and
 * compute payments.
 */
typedef struct {
  long mytype;         /**< @brief Message type (must be > 0) */
  int primi_ordered;   /**< @brief Count/flag of First Courses consumed */
  int secondi_ordered; /**< @brief Count/flag of Main Courses consumed */
  int coffee_ordered;  /**< @brief Count/flag of Coffee/Desserts consumed */
  pid_t pid;           /**< @brief Process ID of the paying user (utente) */
} OrderMsg;

/* ========================================================================== */
/*                             SEMAPHORE CONFIGURATION                        */
/* ========================================================================== */

/**
 * @name System V Semaphore Indices
 * Mapping of indices within the semaphore set created with semget().
 * @{
 */
#define SEM_MUTEX_SHM 0 /**< @brief Mutex for write access to SharedData */
#define SEM_SEATS_PRIMI                                                        \
  1 /**< @brief Counter for free seats in the First Course queue */
#define SEM_SEATS_SECONDI                                                      \
  2 /**< @brief Counter for free seats in the Main Course queue */
#define SEM_SEATS_COFFEE                                                       \
  3 /**< @brief Counter for free seats in the Coffee station queue */
#define SEM_SEATS_CASSA                                                        \
  4 /**< @brief Counter for free seats in the Cashier queue */
#define SEM_TABLE_SEATS                                                        \
  5 /**< @brief Counter for free seats at the cafeteria tables */
#define SEM_READY                                                              \
  6 /**< @brief Initial synchronization barrier for child processes */
#define SEM_DAY_START 7 /**< @brief Signal to start the simulation day */
#define SEM_MUTEX_STATS                                                        \
  8 /**< @brief Mutex to protect concurrent statistics updates */
#define SEM_MUTEX_PORZIONI_P                                                   \
  9 /**< @brief Mutex to retrieve First Course portions */
#define SEM_MUTEX_PORZIONI_S                                                   \
  10                /**< @brief Mutex to retrieve Main Course portions */
#define NUM_SEMS 11 /**< @brief Total number of semaphores in the set */
/** @} */

/* ========================================================================== */
/*                             SIMULATION LOGIC                               */
/* ========================================================================== */

/**
 * @brief Maximum number of dish variants available.
 * Used to size the portion arrays in the SharedData structure.
 */
#define MAX_DISH_TYPES 10

/**
 * @name Station Indices
 * Unique numerical identifiers for each cafeteria station.
 * Food stations (Primi, Secondi, Coffee) must precede the Cashier
 * so that NUM_FOOD_STATIONS = NUM_STATIONS - 1 holds.
 * @{
 */
#define STATION_PRIMI 0   /**< @brief Index for the First Course station */
#define STATION_SECONDI 1 /**< @brief Index for the Main Course station */
#define STATION_COFFEE 2  /**< @brief Index for the Coffee/Dessert station */
#define STATION_CASSA 3   /**< @brief Index for the Cashier station */
#define NUM_STATIONS 4    /**< @brief Total number of operational stations */
#define NUM_FOOD_STATIONS                                                      \
  (NUM_STATIONS - 1) /**< @brief Number of food-serving stations (excludes     \
                        Cashier) */
/** @} */

/**
 * @name Execution Paths and Limits
 * Binary paths and buffer size constraints.
 * @{
 */
#define OPERATOR_PATH                                                          \
  "./bin/operatore" /**< @brief Relative path to the operator process binary   \
                     */
#define USER_PATH                                                              \
  "./bin/utente" /**< @brief Relative path to the user process binary */
#define ARGC_MAX_LENGTH                                                        \
  16 /**< @brief Max buffer length for formatting string arguments */
/** @} */

/**
 * @name Simulation Timing
 * Macros for time-related computations within the simulated workday.
 * All simulated durations are expressed in simulated seconds unless
 * otherwise stated.
 * @{
 */

/**
 * @brief Computes the total number of child processes spawned in the
 * simulation.
 * @param shm Pointer to the SharedData structure.
 */
#define TOTAL_CHILDREN(shm)                                                    \
  (((shm)->config.nof_workers) + ((shm)->config.nof_users))

/**
 * @brief Converts simulated hours to simulated seconds.
 * @param i Number of simulated hours.
 */
#define SIM_DAY_SECOND(i) (((i) * 60 * 60))

/**
 * @brief Converts simulated minutes to simulated seconds.
 * @param i Number of simulated minutes.
 */
#define WORK_MINUTE(i) ((i) * (60))

/**
 * @brief Interval between automatic portion refills (in simulated minutes).
 */
#define REFILL_INTERVAL 10

/**
 * @brief Refill interval converted to simulated seconds.
 *
 * Derived from REFILL_INTERVAL for direct use with sim_sleep().
 */
#define REFILL_INTERVAL_SECS ((REFILL_INTERVAL) * (60))

/**
 * @brief Duration of an operator's break (in simulated minutes).
 *
 * When an operator takes a pause, they are inactive for this many
 * simulated minutes (converted to seconds at the call site via
 * WORKER_PAUSE * 60).
 */
#define WORKER_PAUSE 15
/** @} */

/**
 * @name Utility Macros
 * General-purpose helper macros.
 * @{
 */

/**
 * @brief Returns the minimum of two values.
 *
 * Both arguments are evaluated exactly once per branch.
 * @warning Both @p a and @p b are evaluated more than once if the
 *          preprocessor expands them; avoid side-effecting expressions.
 */
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
/** @} */

#endif
