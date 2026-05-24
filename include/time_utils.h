#ifndef TIME_UTILS_H
#define TIME_UTILS_H

/**
 * @file time_utils.h
 * @brief Simulation time conversion and sleep utilities.
 *
 * Provides functions for converting simulation time units
 * to real-time values and sleeping for simulated durations.
 */

#include <time.h>

/**
 * @brief Converts simulated seconds to a real-time struct timespec.
 * @param sim_seconds Number of simulated seconds.
 * @param n_nano_secs Nanoseconds per simulated minute.
 * @return A struct timespec representing the real-time equivalent.
 */
struct timespec sim_seconds_to_real(int sim_seconds, long n_nano_secs);

/**
 * @brief Generates a random service time around an average value.
 * @param avg The average service time.
 * @param percent The maximum percentage deviation (e.g., 50 for ±50%).
 * @return A randomized service time in simulated seconds.
 */
int random_service_time(int avg, int percent);

/**
 * @brief Converts simulated minutes to real nanoseconds.
 * @param sim_minutes Number of simulated minutes.
 * @param n_nano_secs Nanoseconds per simulated minute.
 * @return The equivalent duration in real nanoseconds.
 */
long sim_minutes_to_nanos(int sim_minutes, long n_nano_secs);

/**
 * @brief Sleeps for a simulated duration, handling EINTR interruptions.
 * @param sim_seconds Duration in simulated seconds.
 * @param n_nano_secs Nanoseconds per simulated minute.
 */
void sim_sleep(int sim_seconds, long n_nano_secs);

#endif
