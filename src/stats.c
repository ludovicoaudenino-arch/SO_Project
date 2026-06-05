/**
 * @file stats.c
 * @brief Implementation of simulation statistics functions.
 */

#include "stats.h"
#include <stdio.h>

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
    printf("Tempo d'attesa medio totale: %ld ns\n", s->wait_time_total_today / s->wait_count_today);
  }
  printf("Pause effettuate dagli operatori: %d\n", s->pauses_today);
  printf("Incasso giornaliero: %.2f EUR\n", s->revenue_today);
  printf("==============================\n\n");
}

void print_final_stats(const SimStats *s, int termination_cause) {
  printf("\n\n===== STATISTICHE FINALI DELLA SIMULAZIONE =====\n");
  printf("Giorni di simulazione completati: %d\n", s->days_completed);
  printf("Motivo terminazione: %s\n", (termination_cause == 0) ? "Normale / Fine Simulazione" : "Interruzione / Errore");
  printf("------------------------------------------------\n");
  printf("Totale Utenti Serviti: %d\n", s->users_served_total);
  printf("Totale Utenti NON Serviti: %d\n", s->users_not_served_total);
  printf("Totale Primi: %d\n", s->dishes_primi_total);
  printf("Totale Secondi: %d\n", s->dishes_secondi_total);
  printf("Totale Caffe: %d\n", s->dishes_coffee_total);
  printf("Totale Avanzi Primi: %d\n", s->dishes_leftover_primi_total);
  printf("Totale Avanzi Secondi: %d\n", s->dishes_leftover_secondi_total);
  
  if (s->wait_count_total_sim > 0) {
    printf("Tempo medio di attesa sull'intera simulazione: %ld ns\n", s->wait_time_total_sim / s->wait_count_total_sim);
  }
  printf("Totale Pause Operatori: %d\n", s->pauses_total);
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
}
