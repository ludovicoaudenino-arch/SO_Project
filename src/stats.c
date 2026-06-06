/**
 * @file stats.c
 * @brief Implementation of simulation statistics functions.
 */

#include "stats.h"
#include "common.h"
#include "ipc_utils.h"
#include <stdio.h>
#include <sys/sem.h>

void print_daily_stats(const SimStats *s, int day) {
  printf("\n=== RESOCONTO GIORNATA %d ===\n", day);
  printf("Utenti serviti: %d\n", s->users_served_today);
  printf("Utenti NON serviti: %d\n", s->users_not_served_today);
  printf("Piatti Primi serviti: %d\n", s->dishes_primi_today);
  printf("Piatti Secondi serviti: %d\n", s->dishes_secondi_today);
  printf("Caffe serviti: %d\n", s->dishes_coffee_today);
  printf("Avanzi Primi: %d\n", s->dishes_leftover_primi_today);
  printf("Avanzi Secondi: %d\n", s->dishes_leftover_secondi_today);
  
  if (s->wait_count_today > 0) {
    printf("Tempo d'attesa medio totale: %ld sec (simulati)\n", s->wait_time_total_today / s->wait_count_today);
  } else {
    printf("Tempo d'attesa medio totale: N/D\n");
  }

  if (s->wait_count_primi_today > 0) {
    printf("Tempo d'attesa medio Primi: %ld sec (simulati)\n", s->wait_time_primi_today / s->wait_count_primi_today);
  } else {
    printf("Tempo d'attesa medio Primi: N/D\n");
  }

  if (s->wait_count_secondi_today > 0) {
    printf("Tempo d'attesa medio Secondi: %ld sec (simulati)\n", s->wait_time_secondi_today / s->wait_count_secondi_today);
  } else {
    printf("Tempo d'attesa medio Secondi: N/D\n");
  }

  if (s->wait_count_coffee_today > 0) {
    printf("Tempo d'attesa medio Caffe: %ld sec (simulati)\n", s->wait_time_coffee_today / s->wait_count_coffee_today);
  } else {
    printf("Tempo d'attesa medio Caffe: N/D\n");
  }

  if (s->wait_count_cassa_today > 0) {
    printf("Tempo d'attesa medio Cassa: %ld sec (simulati)\n", s->wait_time_cassa_today / s->wait_count_cassa_today);
  } else {
    printf("Tempo d'attesa medio Cassa: N/D\n");
  }

  printf("Pause effettuate dagli operatori: %d\n", s->pauses_today);
  printf("Operatori attivi oggi: %d\n", s->active_operators_today);
  printf("Incasso giornaliero: %.2f EUR\n", s->revenue_today);
  printf("==============================\n\n");
}

void print_final_stats(const SimStats *s, int termination_cause) {
  printf("\n\n===== STATISTICHE FINALI DELLA SIMULAZIONE =====\n");
  printf("Giorni di simulazione completati: %d\n", s->days_completed);
  printf("Motivo terminazione: ");
  if (termination_cause == 0) {
    printf("TIMEOUT\n");
  } else if (termination_cause == 1) {
    printf("OVERLOAD\n");
  } else if (termination_cause == 2) {
    printf("SIGINT\n");
  } else {
    printf("SCONOSCIUTO (%d)\n", termination_cause);
  }
  printf("------------------------------------------------\n");
  printf("Totale Utenti Serviti: %d\n", s->users_served_total);
  printf("Totale Utenti NON Serviti: %d\n", s->users_not_served_total);
  printf("Totale Primi: %d\n", s->dishes_primi_total);
  printf("Totale Secondi: %d\n", s->dishes_secondi_total);
  printf("Totale Caffe: %d\n", s->dishes_coffee_total);
  printf("Totale Avanzi Primi: %d\n", s->dishes_leftover_primi_total);
  printf("Totale Avanzi Secondi: %d\n", s->dishes_leftover_secondi_total);
  
  if (s->wait_count_total_sim > 0) {
    printf("Tempo medio di attesa totale: %ld sec (simulati)\n", s->wait_time_total_sim / s->wait_count_total_sim);
  } else {
    printf("Tempo medio di attesa totale: N/D\n");
  }

  if (s->wait_count_primi_sim > 0) {
    printf("Tempo medio di attesa Primi: %ld sec (simulati)\n", s->wait_time_primi_sim / s->wait_count_primi_sim);
  } else {
    printf("Tempo medio di attesa Primi: N/D\n");
  }

  if (s->wait_count_secondi_sim > 0) {
    printf("Tempo medio di attesa Secondi: %ld sec (simulati)\n", s->wait_time_secondi_sim / s->wait_count_secondi_sim);
  } else {
    printf("Tempo medio di attesa Secondi: N/D\n");
  }

  if (s->wait_count_coffee_sim > 0) {
    printf("Tempo medio di attesa Caffe: %ld sec (simulati)\n", s->wait_time_coffee_sim / s->wait_count_coffee_sim);
  } else {
    printf("Tempo medio di attesa Caffe: N/D\n");
  }

  if (s->wait_count_cassa_sim > 0) {
    printf("Tempo medio di attesa Cassa: %ld sec (simulati)\n", s->wait_time_cassa_sim / s->wait_count_cassa_sim);
  } else {
    printf("Tempo medio di attesa Cassa: N/D\n");
  }

  printf("Totale Pause Operatori: %d\n", s->pauses_total);
  printf("Totale Attivazioni Operatori: %d\n", s->active_operators_total);
  printf("Incasso Totale Globale: %.2f EUR\n", s->revenue_total);
  printf("================================================\n\n");
}

void accumulate_and_reset_daily_stats(SimStats *s) {
  s->users_served_total += s->users_served_today;
  s->users_not_served_total += s->users_not_served_today;
  s->dishes_primi_total += s->dishes_primi_today;
  s->dishes_secondi_total += s->dishes_secondi_today;
  s->dishes_coffee_total += s->dishes_coffee_today;
  s->dishes_leftover_primi_total += s->dishes_leftover_primi_today;
  s->dishes_leftover_secondi_total += s->dishes_leftover_secondi_today;
  s->wait_time_total_sim += s->wait_time_total_today;
  s->wait_count_total_sim += s->wait_count_today;
  s->pauses_total += s->pauses_today;
  s->revenue_total += s->revenue_today;

  s->wait_time_primi_sim += s->wait_time_primi_today;
  s->wait_count_primi_sim += s->wait_count_primi_today;
  s->wait_time_secondi_sim += s->wait_time_secondi_today;
  s->wait_count_secondi_sim += s->wait_count_secondi_today;
  s->wait_time_coffee_sim += s->wait_time_coffee_today;
  s->wait_count_coffee_sim += s->wait_count_coffee_today;
  s->wait_time_cassa_sim += s->wait_time_cassa_today;
  s->wait_count_cassa_sim += s->wait_count_cassa_today;

  s->days_completed++;

  // Reset counters for the next day
  s->users_served_today = 0;
  s->users_not_served_today = 0;
  s->dishes_primi_today = 0;
  s->dishes_secondi_today = 0;
  s->dishes_coffee_today = 0;
  s->dishes_leftover_primi_today = 0;
  s->dishes_leftover_secondi_today = 0;
  s->wait_time_total_today = 0;
  s->wait_count_today = 0;
  s->pauses_today = 0;
  s->revenue_today = 0;

  s->wait_time_primi_today = 0;
  s->wait_count_primi_today = 0;
  s->wait_time_secondi_today = 0;
  s->wait_count_secondi_today = 0;
  s->wait_time_coffee_today = 0;
  s->wait_count_coffee_today = 0;
  s->wait_time_cassa_today = 0;
  s->wait_count_cassa_today = 0;

  s->active_operators_today = 0;
}

void stats_record_user_served(SimStats *s, int sem_id) {
  sem_op(sem_id, SEM_MUTEX_STATS, -1, SEM_UNDO);
  s->users_served_today++;
  sem_op(sem_id, SEM_MUTEX_STATS, +1, SEM_UNDO);
}

void stats_record_user_not_served(SimStats *s, int sem_id) {
  sem_op(sem_id, SEM_MUTEX_STATS, -1, SEM_UNDO);
  s->users_not_served_today++;
  sem_op(sem_id, SEM_MUTEX_STATS, +1, SEM_UNDO);
}

void stats_record_dish_served(SimStats *s, int sem_id, int station_type) {
  sem_op(sem_id, SEM_MUTEX_STATS, -1, SEM_UNDO);
  if (station_type == STATION_PRIMI) {
    s->dishes_primi_today++;
  } else if (station_type == STATION_SECONDI) {
    s->dishes_secondi_today++;
  } else if (station_type == STATION_COFFEE) {
    s->dishes_coffee_today++;
  }
  sem_op(sem_id, SEM_MUTEX_STATS, +1, SEM_UNDO);
}

void stats_record_revenue(SimStats *s, int sem_id, float amount) {
  sem_op(sem_id, SEM_MUTEX_STATS, -1, SEM_UNDO);
  s->revenue_today += amount;
  sem_op(sem_id, SEM_MUTEX_STATS, +1, SEM_UNDO);
}

void stats_record_pause(SimStats *s, int sem_id) {
  sem_op(sem_id, SEM_MUTEX_STATS, -1, SEM_UNDO);
  s->pauses_today++;
  sem_op(sem_id, SEM_MUTEX_STATS, +1, SEM_UNDO);
}

void stats_record_active_operator(SimStats *s, int sem_id) {
  sem_op(sem_id, SEM_MUTEX_STATS, -1, SEM_UNDO);
  s->active_operators_today++;
  s->active_operators_total++;
  sem_op(sem_id, SEM_MUTEX_STATS, +1, SEM_UNDO);
}

void stats_record_wait_time(SimStats *s, int sem_id, int station_type, long wait_time_sim_sec) {
  sem_op(sem_id, SEM_MUTEX_STATS, -1, SEM_UNDO);
  s->wait_time_total_today += wait_time_sim_sec;
  s->wait_count_today++;
  
  if (station_type == STATION_PRIMI) {
    s->wait_time_primi_today += wait_time_sim_sec;
    s->wait_count_primi_today++;
  } else if (station_type == STATION_SECONDI) {
    s->wait_time_secondi_today += wait_time_sim_sec;
    s->wait_count_secondi_today++;
  } else if (station_type == STATION_COFFEE) {
    s->wait_time_coffee_today += wait_time_sim_sec;
    s->wait_count_coffee_today++;
  } else if (station_type == STATION_CASSA) {
    s->wait_time_cassa_today += wait_time_sim_sec;
    s->wait_count_cassa_today++;
  }
  sem_op(sem_id, SEM_MUTEX_STATS, +1, SEM_UNDO);
}
