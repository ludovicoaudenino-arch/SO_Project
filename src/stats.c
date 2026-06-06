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
  float f_day = (float)day;

  int tot_served = s->users_served_total + s->users_served_today;
  int tot_not_served = s->users_not_served_total + s->users_not_served_today;
  int tot_primi = s->dishes_primi_total + s->dishes_primi_today;
  int tot_secondi = s->dishes_secondi_total + s->dishes_secondi_today;
  int tot_coffee = s->dishes_coffee_total + s->dishes_coffee_today;
  int tot_leftover_primi =
      s->dishes_leftover_primi_total + s->dishes_leftover_primi_today;
  int tot_leftover_secondi =
      s->dishes_leftover_secondi_total + s->dishes_leftover_secondi_today;
  int tot_pauses = s->pauses_total + s->pauses_today;
  float tot_revenue = s->revenue_total + s->revenue_today;

  printf("\n=== RESOCONTO GIORNATA %d ===\n", day);
  printf("--- Statistiche Giornaliere ---\n");
  printf("Utenti serviti oggi: %d\n", s->users_served_today);
  printf("Utenti NON serviti oggi: %d\n", s->users_not_served_today);
  printf("Piatti Primi serviti oggi: %d\n", s->dishes_primi_today);
  printf("Piatti Secondi serviti oggi: %d\n", s->dishes_secondi_today);
  printf("Caffe/Dolci serviti oggi: %d\n", s->dishes_coffee_today);
  printf("Avanzi Primi oggi: %d\n", s->dishes_leftover_primi_today);
  printf("Avanzi Secondi oggi: %d\n", s->dishes_leftover_secondi_today);

  if (s->wait_count_today > 0) {
    printf("Tempo d'attesa medio totale oggi: %ld sec (simulati)\n",
           s->wait_time_total_today / s->wait_count_today);
  } else {
    printf("Tempo d'attesa medio totale oggi: N/D\n");
  }

  if (s->wait_count_primi_today > 0) {
    printf("Tempo d'attesa medio Primi oggi: %ld sec (simulati)\n",
           s->wait_time_primi_today / s->wait_count_primi_today);
  } else {
    printf("Tempo d'attesa medio Primi oggi: N/D\n");
  }

  if (s->wait_count_secondi_today > 0) {
    printf("Tempo d'attesa medio Secondi oggi: %ld sec (simulati)\n",
           s->wait_time_secondi_today / s->wait_count_secondi_today);
  } else {
    printf("Tempo d'attesa medio Secondi oggi: N/D\n");
  }

  if (s->wait_count_coffee_today > 0) {
    printf("Tempo d'attesa medio Caffe oggi: %ld sec (simulati)\n",
           s->wait_time_coffee_today / s->wait_count_coffee_today);
  } else {
    printf("Tempo d'attesa medio Caffe oggi: N/D\n");
  }

  if (s->wait_count_cassa_today > 0) {
    printf("Tempo d'attesa medio Cassa oggi: %ld sec (simulati)\n",
           s->wait_time_cassa_today / s->wait_count_cassa_today);
  } else {
    printf("Tempo d'attesa medio Cassa oggi: N/D\n");
  }

  printf("Operatori attivi oggi: %d\n", s->active_operators_today);
  printf("Pause effettuate oggi: %d\n", s->pauses_today);
  printf("Incasso oggi: %.2f EUR\n", s->revenue_today);

  printf("\n--- Statistiche Cumulate (fino a Giorno %d) ---\n", day);
  printf("Utenti serviti totali: %d (media/giorno: %.2f)\n", tot_served,
         tot_served / f_day);
  printf("Utenti NON serviti totali: %d (media/giorno: %.2f)\n", tot_not_served,
         tot_not_served / f_day);

  int tot_dishes = tot_primi + tot_secondi + tot_coffee;
  printf("Piatti distribuiti totali: %d (media/giorno: %.2f)\n", tot_dishes,
         tot_dishes / f_day);
  printf("  - Primi totali: %d (media/giorno: %.2f)\n", tot_primi,
         tot_primi / f_day);
  printf("  - Secondi totali: %d (media/giorno: %.2f)\n", tot_secondi,
         tot_secondi / f_day);
  printf("  - Caffe/Dolci totali: %d (media/giorno: %.2f)\n", tot_coffee,
         tot_coffee / f_day);

  int tot_leftover = tot_leftover_primi + tot_leftover_secondi;
  printf("Piatti avanzati totali: %d (media/giorno: %.2f)\n", tot_leftover,
         tot_leftover / f_day);
  printf("  - Avanzi Primi totali: %d (media/giorno: %.2f)\n",
         tot_leftover_primi, tot_leftover_primi / f_day);
  printf("  - Avanzi Secondi totali: %d (media/giorno: %.2f)\n",
         tot_leftover_secondi, tot_leftover_secondi / f_day);

  // Calcolo dei tempi medi simulati di attesa totali (cumulati fino ad oggi)
  long cum_wait_count_total = s->wait_count_total_sim + s->wait_count_today;
  long cum_wait_time_total = s->wait_time_total_sim + s->wait_time_total_today;
  if (cum_wait_count_total > 0) {
    printf("Tempo d'attesa medio complessivo: %ld sec (simulati)\n",
           cum_wait_time_total / cum_wait_count_total);
  } else {
    printf("Tempo d'attesa medio complessivo: N/D\n");
  }

  long cum_wait_count_primi =
      s->wait_count_primi_sim + s->wait_count_primi_today;
  long cum_wait_time_primi = s->wait_time_primi_sim + s->wait_time_primi_today;
  if (cum_wait_count_primi > 0) {
    printf("  - Medio Primi: %ld sec (simulati)\n",
           cum_wait_time_primi / cum_wait_count_primi);
  }

  long cum_wait_count_secondi =
      s->wait_count_secondi_sim + s->wait_count_secondi_today;
  long cum_wait_time_secondi =
      s->wait_time_secondi_sim + s->wait_time_secondi_today;
  if (cum_wait_count_secondi > 0) {
    printf("  - Medio Secondi: %ld sec (simulati)\n",
           cum_wait_time_secondi / cum_wait_count_secondi);
  }

  long cum_wait_count_coffee =
      s->wait_count_coffee_sim + s->wait_count_coffee_today;
  long cum_wait_time_coffee =
      s->wait_time_coffee_sim + s->wait_time_coffee_today;
  if (cum_wait_count_coffee > 0) {
    printf("  - Medio Caffe: %ld sec (simulati)\n",
           cum_wait_time_coffee / cum_wait_count_coffee);
  }

  long cum_wait_count_cassa =
      s->wait_count_cassa_sim + s->wait_count_cassa_today;
  long cum_wait_time_cassa = s->wait_time_cassa_sim + s->wait_time_cassa_today;
  if (cum_wait_count_cassa > 0) {
    printf("  - Medio Cassa: %ld sec (simulati)\n",
           cum_wait_time_cassa / cum_wait_count_cassa);
  }

  printf("Pause totali: %d (media/giorno: %.2f)\n", tot_pauses,
         tot_pauses / f_day);
  printf("Ricavo totale: %.2f EUR (media/giorno: %.2f)\n", tot_revenue,
         tot_revenue / f_day);
  printf("==============================\n\n");
}

void print_final_stats(const SimStats *s, int termination_cause) {
  float days = s->days_completed > 0 ? (float)s->days_completed : 1.0f;

  printf("\n\n===== STATISTICHE FINALI DELLA SIMULAZIONE =====\n");
  printf("Giorni di simulazione completati: %d\n", s->days_completed);
  printf("Motivo terminazione: ");
  if (termination_cause == 0) {
    printf("TIMEOUT (raggiunta durata massima)\n");
  } else if (termination_cause == 1) {
    printf("OVERLOAD (troppi utenti in attesa a fine giornata)\n");
  } else if (termination_cause == 2) {
    printf("SIGINT (interruzione esterna)\n");
  } else {
    printf("SCONOSCIUTO (%d)\n", termination_cause);
  }
  printf("------------------------------------------------\n");
  printf("Totale Utenti Serviti: %d (media/giorno: %.2f)\n",
         s->users_served_total, s->users_served_total / days);
  printf("Totale Utenti NON Serviti: %d (media/giorno: %.2f)\n",
         s->users_not_served_total, s->users_not_served_total / days);

  int tot_dishes =
      s->dishes_primi_total + s->dishes_secondi_total + s->dishes_coffee_total;
  printf("Totale Piatti Distribuiti: %d (media/giorno: %.2f)\n", tot_dishes,
         tot_dishes / days);
  printf("  - Primi: %d (media/giorno: %.2f)\n", s->dishes_primi_total,
         s->dishes_primi_total / days);
  printf("  - Secondi: %d (media/giorno: %.2f)\n", s->dishes_secondi_total,
         s->dishes_secondi_total / days);
  printf("  - Caffe/Dolci: %d (media/giorno: %.2f)\n", s->dishes_coffee_total,
         s->dishes_coffee_total / days);

  int tot_leftovers =
      s->dishes_leftover_primi_total + s->dishes_leftover_secondi_total;
  printf("Totale Piatti Avanzati: %d (media/giorno: %.2f)\n", tot_leftovers,
         tot_leftovers / days);
  printf("  - Avanzi Primi: %d (media/giorno: %.2f)\n",
         s->dishes_leftover_primi_total, s->dishes_leftover_primi_total / days);
  printf("  - Avanzi Secondi: %d (media/giorno: %.2f)\n",
         s->dishes_leftover_secondi_total,
         s->dishes_leftover_secondi_total / days);

  if (s->wait_count_total_sim > 0) {
    printf("Tempo medio di attesa complessivo: %ld sec (simulati)\n",
           s->wait_time_total_sim / s->wait_count_total_sim);
  } else {
    printf("Tempo medio di attesa complessivo: N/D\n");
  }

  if (s->wait_count_primi_sim > 0) {
    printf("  - Primi: %ld sec (simulati)\n",
           s->wait_time_primi_sim / s->wait_count_primi_sim);
  } else {
    printf("  - Primi: N/D\n");
  }

  if (s->wait_count_secondi_sim > 0) {
    printf("  - Secondi: %ld sec (simulati)\n",
           s->wait_time_secondi_sim / s->wait_count_secondi_sim);
  } else {
    printf("  - Secondi: N/D\n");
  }

  if (s->wait_count_coffee_sim > 0) {
    printf("  - Caffe: %ld sec (simulati)\n",
           s->wait_time_coffee_sim / s->wait_count_coffee_sim);
  } else {
    printf("  - Caffe: N/D\n");
  }

  if (s->wait_count_cassa_sim > 0) {
    printf("  - Cassa: %ld sec (simulati)\n",
           s->wait_time_cassa_sim / s->wait_count_cassa_sim);
  } else {
    printf("  - Cassa: N/D\n");
  }

  printf("Totale Attivazioni Operatori: %d\n", s->active_operators_total);
  printf("Totale Pause Operatori: %d (media/giorno: %.2f)\n", s->pauses_total,
         s->pauses_total / days);
  printf("Incasso Totale Globale: %.2f EUR (media/giorno: %.2f)\n",
         s->revenue_total, s->revenue_total / days);
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

void stats_record_wait_time(SimStats *s, int sem_id, int station_type,
                            long wait_time_sim_sec) {
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
