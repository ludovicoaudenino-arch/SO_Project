#ifndef CONFIG_H
#define CONFIG_H
#define DEFAULT_CONFIG_PATH "conf/config.conf"
#define CONFIG_ENV_VAR "MENSA_CONFIG"

typedef struct {
  int sim_duration;
  long n_nano_secs;
  int nof_workers;
  int nof_users;
  int nof_wk_seats_primi;
  int nof_wk_seats_secondi;
  int nof_wk_seats_coffee;
  int nof_wk_seats_cassa;
  int nof_table_seats;
  int avg_srvc_primi;
  int avg_srvc_main_course;
  int avg_srvc_coffee;
  int avg_srvc_cassa;
  int avg_refill_primi;
  int avg_refill_secondi;
  int max_porzioni_primi;
  int max_porzioni_secondi;
  int nof_pause;
  float price_primi;
  float price_secondi;
  float price_coffee;
  int overload_threshold;
  char menu_file[256];
} Config;

int parse_config(const char *path, Config *conf);

#endif