#ifndef COMMON_H
#define COMMON_H

/**
 * @file common.h
 * @brief Global constants, identifiers, and macros for the Cafeteria
 * simulation.
 *
 * This file contains definitions shared across all processes,
 * including data structure limits, station indices,
 * and identifiers for the System V semaphore set.
 */

/**
 * @brief Maximum number of dish variants available.
 * Used to size the portion arrays in the SharedData structure.
 */
#define MAX_DISH_TYPES 10

/**
 * @name Station Indices
 * Unique numerical identifiers for each cafeteria station.
 * @{
 */
#define STATION_PRIMI 0   /**< @brief Index for the First Course station */
#define STATION_SECONDI 1 /**< @brief Index for the Main Course station */
#define STATION_COFFEE 2  /**< @brief Index for the Coffee/Dessert station */
#define STATION_CASSA 3   /**< @brief Index for the Cashier station */
#define NUM_STATIONS 4    /**< @brief Total number of operational stations */
/** @} */

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

#endif
