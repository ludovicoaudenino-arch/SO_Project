/**
 * @file time_utils.c
 * @brief Implementation of simulation time utilities.
 */

#include "time_utils.h"
#include <errno.h>
#include <stdlib.h>

/**
 * @details Conversion formula:
 *   1 simulated minute  = n_nano_secs real nanoseconds
 *   1 simulated second  = n_nano_secs / 60 real nanoseconds
 *   total_nanos = (sim_seconds * n_nano_secs) / 60
 */
struct timespec sim_seconds_to_real(int sim_seconds, long n_nano_secs) {
  long total_nanos = ((long)sim_seconds * n_nano_secs) / 60;
  struct timespec ts;
  ts.tv_sec = total_nanos / 1000000000L;
  ts.tv_nsec = total_nanos % 1000000000L;
  return ts;
}

int random_service_time(int avg, int percent) {
  if (avg <= 0 || percent < 0) {
    return avg;
  }
  int min_val = avg - (avg * percent) / 100;
  int max_val = avg + (avg * percent) / 100;
  if (min_val < 1) {
    min_val = 1;
  }
  if (max_val <= min_val) {
    return min_val;
  }
  return min_val + (rand() % (max_val - min_val + 1));
}

long sim_minutes_to_nanos(int sim_minutes, long n_nano_secs) {
  return (long)sim_minutes * n_nano_secs;
}

void sim_sleep(int sim_seconds, long n_nano_secs) {
  struct timespec req = sim_seconds_to_real(sim_seconds, n_nano_secs);
  struct timespec rem;
  while (nanosleep(&req, &rem) == -1 && errno == EINTR) {
    req = rem;
  }
}
