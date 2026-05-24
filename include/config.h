#ifndef CONFIG_H
#define CONFIG_H

/**
 * @file config.h
 * @brief Configuration structures and parsing functions.
 * 
 * This file defines the main Configuration structure holding all
 * parameters read from the configuration file or environment variables,
 * as well as the prototype for the parser function.
 */

/**
 * @brief Default path to the configuration file.
 * Used if no path is provided via the environment variable.
 */
#define DEFAULT_CONFIG_PATH "conf/config.conf"

/**
 * @brief Environment variable name for the configuration file path.
 * If this variable is set, its value overrides the default path.
 */
#define CONFIG_ENV_VAR "MENSA_CONFIG"

/**
 * @struct Config
 * @brief System configuration parameters.
 * 
 * Contains all the tunable parameters for the cafeteria simulation,
 * including durations, worker allocations, service times, and pricing.
 */
typedef struct {
  /** @name General Simulation Parameters */
  /** @{ */
  int sim_duration;              /**< @brief Total duration of the simulation in days */
  long n_nano_secs;              /**< @brief Real-time nanoseconds equivalent to one simulation minute */
  /** @} */

  /** @name Entities Allocation */
  /** @{ */
  int nof_workers;               /**< @brief Total number of cafeteria workers */
  int nof_users;                 /**< @brief Total number of users (students/staff) */
  /** @} */

  /** @name Capacities and Queues */
  /** @{ */
  int nof_wk_seats_primi;        /**< @brief Max queue capacity for the First Course station */
  int nof_wk_seats_secondi;      /**< @brief Max queue capacity for the Main Course station */
  int nof_wk_seats_coffee;       /**< @brief Max queue capacity for the Coffee station */
  int nof_wk_seats_cassa;        /**< @brief Max queue capacity for the Cashier station */
  int nof_table_seats;           /**< @brief Total number of seats at cafeteria tables */
  /** @} */

  /** @name Service Times (in simulation minutes) */
  /** @{ */
  int avg_srvc_primi;            /**< @brief Average service time at the First Course station */
  int avg_srvc_main_course;      /**< @brief Average service time at the Main Course station */
  int avg_srvc_coffee;           /**< @brief Average service time at the Coffee station */
  int avg_srvc_cassa;            /**< @brief Average service time at the Cashier station */
  /** @} */

  /** @name Refill and Portions */
  /** @{ */
  int avg_refill_primi;          /**< @brief Average time to refill First Course portions */
  int avg_refill_secondi;        /**< @brief Average time to refill Main Course portions */
  int max_porzioni_primi;        /**< @brief Maximum number of First Course portions per batch */
  int max_porzioni_secondi;      /**< @brief Maximum number of Main Course portions per batch */
  /** @} */

  /** @name Miscellaneous */
  /** @{ */
  int nof_pause;                 /**< @brief Number of allowed breaks for workers */
  float price_primi;             /**< @brief Base price for a First Course */
  float price_secondi;           /**< @brief Base price for a Main Course */
  float price_coffee;            /**< @brief Base price for a Coffee/Dessert */
  int overload_threshold;        /**< @brief Queue threshold to trigger worker reallocation */
  char menu_file[256];           /**< @brief Path to the file containing the daily menu */
  /** @} */
} Config;

/**
 * @brief Parses the configuration file and populates the Config structure.
 * 
 * This function attempts to read the file specified by the given path.
 * If the path is NULL, it falls back to the MENSA_CONFIG environment variable,
 * or the DEFAULT_CONFIG_PATH if the variable is not set.
 * 
 * @param path Pointer to the string containing the path, or NULL for default behavior.
 * @param conf Pointer to the Config structure to be populated.
 * @return 0 on success, -1 on failure (e.g., file not found or invalid format).
 */
int parse_config(const char *path, Config *conf);

#endif