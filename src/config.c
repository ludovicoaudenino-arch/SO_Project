#include "config.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MATCH_INT(field)                                                       \
  else if (strcmp(key, #field) == 0) {                                         \
    conf->field = atoi(value);                                                 \
  }
#define MATCH_LONG(field)                                                      \
  else if (strcmp(key, #field) == 0) {                                         \
    conf->field = atol(value);                                                 \
  }
#define MATCH_FLOAT(field)                                                     \
  else if (strcmp(key, #field) == 0) {                                         \
    conf->field = atof(value);                                                 \
  }
#define MATCH_STR(field)                                                       \
  else if (strcmp(key, #field) == 0) {                                         \
    strncpy(conf->field, value, sizeof(conf->field) - 1);                      \
    conf->field[sizeof(conf->field) - 1] = '\0';                               \
  }

static char *trim(char *str) {
  if (str == NULL) {
    return NULL;
  }

  while (*str == ' ' || *str == '\t') {
    str++;
  }

  size_t len = strlen(str);
  if (len == 0) {
    return str;
  }

  char *str_end = &str[len - 1];
  while (*str_end == ' ' || *str_end == '\t') {
    str_end--;
  }

  str_end++;
  *str_end = '\0';
  return str;
}

int parse_config(const char *path, Config *conf) {
  const char *actual_path = (path != NULL) ? path : getenv(CONFIG_ENV_VAR);
  if (actual_path == NULL) {
    actual_path = DEFAULT_CONFIG_PATH;
  }
  if (conf == NULL) {
    return -1;
  }
  FILE *f = fopen(actual_path, "r");
  if (f == NULL) {
    return -1;
  }

  char line[256];
  while (fgets(line, sizeof(line), f) != NULL) {
    size_t len = strlen(line);
    if (len > 0 && line[len - 1] == '\n') {
      line[len - 1] = '\0';
    }

    char *tmp = line;
    while (*tmp == ' ' || *tmp == '\t') {
      tmp++;
    }
    if (*tmp == '\0' || *tmp == '#') {
      continue;
    }

    char *equal = strchr(line, '=');
    if (equal == NULL) {
      continue;
    }
    *equal = '\0';

    char *key = trim(tmp);
    char *value = trim(equal + 1);

    if (0) {
    }
    MATCH_INT(sim_duration)
    MATCH_INT(nof_workers)
    MATCH_INT(nof_users)
    MATCH_INT(nof_wk_seats_primi)
    MATCH_INT(nof_wk_seats_secondi)
    MATCH_INT(nof_wk_seats_coffee)
    MATCH_INT(nof_wk_seats_cassa)
    MATCH_INT(nof_table_seats)
    MATCH_INT(avg_srvc_primi)
    MATCH_INT(avg_srvc_main_course)
    MATCH_INT(avg_srvc_coffee)
    MATCH_INT(avg_srvc_cassa)
    MATCH_INT(avg_refill_primi)
    MATCH_INT(avg_refill_secondi)
    MATCH_INT(max_porzioni_primi)
    MATCH_INT(max_porzioni_secondi)
    MATCH_INT(nof_pause)
    MATCH_INT(overload_threshold)
    MATCH_LONG(n_nano_secs)
    MATCH_FLOAT(price_primi)
    MATCH_FLOAT(price_secondi)
    MATCH_FLOAT(price_coffee)
    MATCH_STR(menu_file)
  }
  fclose(f);
  return 0;
}

cioa