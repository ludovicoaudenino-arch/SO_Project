# Piano di Implementazione — "Servizio Mensa Oasi del Golfo" (Versione 24/30)

> Riferimento: `Progetto_SO_2025_26 (1).pdf`, sezioni 5.1–5.6, 7, 8.

---

## Indice

1. [Panoramica architetturale](#1-panoramica-architetturale)
2. [Struttura directory e file del progetto](#2-struttura-directory-e-file-del-progetto)
3. [Parametri di configurazione](#3-parametri-di-configurazione)
4. [Risorse IPC — Progettazione dettagliata](#4-risorse-ipc--progettazione-dettagliata)
5. [Processo responsabile_mensa (main)](#5-processo-responsabile_mensa-main)
6. [Risorse di tipo stazione](#6-risorse-di-tipo-stazione)
7. [Processo operatore](#7-processo-operatore)
8. [Processo operatore_cassa](#8-processo-operatore_cassa)
9. [Processo utente](#9-processo-utente)
10. [Gestione del tempo simulato](#10-gestione-del-tempo-simulato)
11. [Statistiche](#11-statistiche)
12. [Terminazione](#12-terminazione)
13. [Makefile e compilazione](#13-makefile-e-compilazione)
14. [File di configurazione di test](#14-file-di-configurazione-di-test)
15. [Piano di verifica](#15-piano-di-verifica)
16. [Ordine di implementazione (fasi)](#16-ordine-di-implementazione-fasi)

---

## 1. Panoramica architetturale

La simulazione è basata su **processi Unix separati** che comunicano tramite **IPC System V** (o POSIX dove appropriato):

| Meccanismo IPC    | Uso principale                                                                 |
|-------------------|--------------------------------------------------------------------------------|
| **Shared Memory** | Struttura dati globale della simulazione: stato stazioni, contatori porzioni, statistiche, flag di terminazione |
| **Semafori**      | Sincronizzazione degli accessi concorrenti: posti alle stazioni, code utenti, mutua esclusione su dati condivisi |
| **Code di messaggi** *(o pipe)* | Comunicazione asincrona tra processi (es. utente → cassa: lista piatti scelti; resp. mensa → operatori: segnali di inizio/fine giornata) |

Ogni tipo di processo è compilato come **eseguibile separato** e lanciato tramite `fork()`+`execve()`.

### Schema dei processi

```
responsabile_mensa (main)
├── fork+exec → operatore       ×NOF_WORKERS (assegnati a primi/secondi/coffee)
├── fork+exec → operatore_cassa ×(almeno 1, dal pool NOF_WORKERS)
└── fork+exec → utente          ×NOF_USERS
```

> **Nota:** Il responsabile_mensa decide quanti operatori assegnare a ciascuna stazione. La specifica richiede almeno 1 per stazione e priorità alle stazioni con `AVG_SRVC_*` più alto.

---

## 2. Struttura directory e file del progetto

```
SO_Project/
├── Makefile
├── include/
│   ├── config.h          # parsing della configurazione + struttura parametri
│   ├── ipc_utils.h       # creazione/attach/cleanup risorse IPC
│   ├── shared_data.h     # strutture dati in shared memory
│   ├── time_utils.h      # funzioni per il tempo simulato (nanosleep, random timing)
│   ├── stats.h           # struttura statistiche e funzioni di aggiornamento
│   └── common.h          # costanti comuni, macro, include di sistema
├── src/
│   ├── responsabile_mensa.c   # processo padre / coordinatore
│   ├── operatore.c            # processo operatore stazione cibo (opzionale)
│   ├── operatore_cassa.c      # processo cassiere (opzionale)
│   ├── stazione.c             # [NUOVO] processo unificato / manager di stazione
│   ├── utente.c               # processo utente
│   ├── config.c               # implementazione parsing configurazione
│   ├── ipc_utils.c            # implementazione utility IPC
│   ├── time_utils.c           # implementazione utility tempo
│   └── stats.c                # implementazione funzioni statistiche
├── conf/
│   ├── config.conf            # configurazione di default
│   ├── config_timeout.conf    # genera terminazione per timeout
│   ├── config_overload.conf   # genera terminazione per overload
│   └── menu.txt               # menu con primi, secondi, dolce-caffè
├── staff/                     # cartella privata (ignorata da git)
│   ├── Progetto_SO_2025_26 (1).pdf
│   └── piano_implementazione.md   # questo file
├── build/                     # directory di build (generata)
├── bin/                       # eseguibili (generati)
└── .gitignore
```

### Eseguibili prodotti (in `bin/`)

| Eseguibile            | Sorgente principale          | Descrizione                          |
|-----------------------|------------------------------|--------------------------------------|
| `responsabile_mensa`  | `responsabile_mensa.c`       | Coordinatore / main della simulazione |
| `utente`              | `utente.c`                   | Processo utente                      |
| `stazione`            | `stazione.c`                 | [Alternativa] Lavoratore stazione / cassa unificato |
| `operatore`           | `operatore.c`                | [Alternativa] Operatore generico cibo |
| `operatore_cassa`     | `operatore_cassa.c`          | [Alternativa] Cassiere               |

---

## 3. Parametri di configurazione

Tutti i parametri sono letti **a runtime** da un file `.conf` (path passato come argomento al `responsabile_mensa` o via variabile d'ambiente `MENSA_CONFIG`). Non devono richiedere ricompilazione.

### Formato proposto: `KEY = VALUE` (uno per riga, `#` per commenti)

```conf
# --- Simulazione ---
SIM_DURATION         = 3          # giorni
N_NANO_SECS          = 1000000    # nanosecondi per minuto simulato

# --- Risorse umane ---
NOF_WORKERS          = 8
NOF_USERS            = 50

# --- Postazioni per stazione ---
NOF_WK_SEATS_PRIMI   = 2
NOF_WK_SEATS_SECONDI = 2
NOF_WK_SEATS_COFFEE  = 2
NOF_WK_SEATS_CASSA   = 2

# --- Posti tavolo ---
NOF_TABLE_SEATS      = 30

# --- Tempi medi di servizio (in secondi simulati) ---
AVG_SRVC_PRIMI       = 4
AVG_SRVC_MAIN_COURSE = 5
AVG_SRVC_COFFEE      = 3
AVG_SRVC_CASSA       = 2

# --- Porzioni ---
AVG_REFILL_PRIMI     = 20       # porzioni iniziali per tipo, per giorno
AVG_REFILL_SECONDI   = 20
MAX_PORZIONI_PRIMI   = 40       # massimo dopo refill
MAX_PORZIONI_SECONDI = 40

# --- Pause operatore ---
NOF_PAUSE            = 3        # massimo pause per operatore per giornata

# --- Prezzi ---
PRICE_PRIMI          = 4.50
PRICE_SECONDI        = 5.50
PRICE_COFFEE         = 1.20

# --- Terminazione ---
OVERLOAD_THRESHOLD   = 20       # utenti in coda a fine giornata → overload

# --- Menu ---
MENU_FILE            = conf/menu.txt
```

### Struttura dati in C

```c
typedef struct {
    int   sim_duration;
    long  n_nano_secs;
    int   nof_workers;
    int   nof_users;
    int   nof_wk_seats_primi;
    int   nof_wk_seats_secondi;
    int   nof_wk_seats_coffee;
    int   nof_wk_seats_cassa;
    int   nof_table_seats;
    int   avg_srvc_primi;
    int   avg_srvc_main_course;
    int   avg_srvc_coffee;
    int   avg_srvc_cassa;
    int   avg_refill_primi;
    int   avg_refill_secondi;
    int   max_porzioni_primi;
    int   max_porzioni_secondi;
    int   nof_pause;
    float price_primi;
    float price_secondi;
    float price_coffee;
    int   overload_threshold;
    char  menu_file[256];
} Config;
```

### Funzione di parsing

- `config.c` → `int parse_config(const char *path, Config *cfg);`
- Ignora righe vuote e commenti (`#`).
- Trim degli spazi attorno a `=`.
- Validazione: tutti i campi obbligatori devono essere presenti; errore fatale in caso contrario.

---

## 4. Risorse IPC — Progettazione dettagliata

### 4.1 Shared Memory

Un **unico segmento** di shared memory contiene tutte le strutture condivise.

```c
typedef struct {
    // --- Flag di controllo ---
    int   simulation_running;       // 1=attiva, 0=terminata
    int   day_running;              // 1=giornata in corso
    int   current_day;              // giorno corrente (1..SIM_DURATION)
    int   termination_cause;        // 0=timeout, 1=overload
    int   all_ready;                // contatore sincro iniziale

    // --- Stazioni distribuzione ---
    StationData station_primi;
    StationData station_secondi;
    StationData station_coffee;
    StationData station_cassa;

    // --- Porzioni disponibili ---
    // Per ciascun tipo di primo (almeno 2)
    int   porzioni_primo[MAX_DISH_TYPES];
    int   porzioni_secondo[MAX_DISH_TYPES];
    int   num_tipi_primo;
    int   num_tipi_secondo;

    // --- Tavoli ---
    int   table_seats_free;          // posti libero ai tavoli

    // --- Statistiche ---
    SimStats stats;

    // --- Configurazione (copia read-only per i figli) ---
    Config config;
} SharedData;
```

```c
typedef struct {
    int queue_length;        // utenti in coda alla stazione
    int active_operators;    // operatori attualmente al lavoro
} StationData;
```

### 4.2 Semafori

Utilizziamo **set di semafori System V** (`semget`, `semop`).

| Indice semaforo | Nome logico            | Valore iniziale             | Scopo                                              |
|:---:|------------------------|-----------------------------|------------------------------------------------------|
| 0   | `SEM_MUTEX_SHM`       | 1                           | Mutua esclusione accesso a `SharedData`                |
| 1   | `SEM_SEATS_PRIMI`     | `NOF_WK_SEATS_PRIMI`        | Posti liberi alla stazione primi                       |
| 2   | `SEM_SEATS_SECONDI`   | `NOF_WK_SEATS_SECONDI`      | Posti liberi alla stazione secondi                     |
| 3   | `SEM_SEATS_COFFEE`    | `NOF_WK_SEATS_COFFEE`       | Posti liberi alla stazione coffee                      |
| 4   | `SEM_SEATS_CASSA`     | `NOF_WK_SEATS_CASSA`        | Posti liberi alla stazione cassa                       |
| 5   | `SEM_TABLE_SEATS`     | `NOF_TABLE_SEATS`            | Posti liberi ai tavoli                                 |
| 6   | `SEM_READY`           | 0                           | Barriera di sincronizzazione: tutti pronti             |
| 7   | `SEM_DAY_START`       | 0                           | Segnale inizio giornata (broadcast)                    |
| 8   | `SEM_MUTEX_STATS`     | 1                           | Mutua esclusione accesso statistiche                   |
| 9   | `SEM_MUTEX_PORZIONI_P`| 1                           | Mutua esclusione porzioni primi                        |
| 10  | `SEM_MUTEX_PORZIONI_S`| 1                           | Mutua esclusione porzioni secondi                      |
| 11  | `SEM_QUEUE_PRIMI`     | 0                           | Coda utenti stazione primi (contatore)                 |
| 12  | `SEM_QUEUE_SECONDI`   | 0                           | Coda utenti stazione secondi                           |
| 13  | `SEM_QUEUE_COFFEE`    | 0                           | Coda utenti stazione coffee                            |
| 14  | `SEM_QUEUE_CASSA`     | 0                           | Coda utenti stazione cassa                             |
| 15  | `SEM_PAUSE_GUARD`     | varia                       | Garantisce almeno 1 operatore attivo per stazione      |

> **Nota:** Per le "code" utenti-operatori, un approccio alternativo è usare **code di messaggi** (vedi sotto). I semafori qui descritti servono alla sincronizzazione dei contatori e dei posti.

### 4.3 Code di messaggi

Una **coda di messaggi per stazione** per la comunicazione utente ↔ operatore:

| Coda         | Direzione            | Contenuto del messaggio                       |
|--------------|----------------------|-----------------------------------------------|
| `mq_primi`   | utente → operatore   | `{ mtype=1, user_pid, dish_choice }`          |
| `mq_secondi` | utente → operatore   | `{ mtype=1, user_pid, dish_choice }`          |
| `mq_coffee`  | utente → operatore   | `{ mtype=1, user_pid, coffee_type }`          |
| `mq_cassa`   | utente → cassiere    | `{ mtype=1, user_pid, piatti_scelti[], total }` |
| `mq_served`  | operatore → utente   | `{ mtype=user_pid, served=1/0 }`              |

**Struttura messaggio (esempio):**

```c
typedef struct {
    long mtype;             // tipo messaggio (1 per richiesta, pid utente per risposta)
    pid_t user_pid;
    int   dish_choice;      // indice piatto scelto
    int   served;           // 1 = servito, 0 = piatto esaurito
} MensaMsg;
```

**Pattern di interazione utente-operatore:**

1. **Utente** invia richiesta sulla coda della stazione scelta (`mtype=1`).
2. **Operatore** legge dalla coda (`msgrcv` con `mtype=1`), simula il tempo di servizio, verifica/decrementa le porzioni disponibili.
3. **Operatore** invia risposta sulla stessa coda o su una coda dedicata (`mtype=user_pid`).
4. **Utente** legge la risposta filtrando su `mtype=proprio_pid`.

### 4.4 Funzioni utility IPC (`ipc_utils.c`)

```
int   create_shared_memory(size_t size);
void* attach_shared_memory(int shmid);
void  detach_shared_memory(void *ptr);
void  remove_shared_memory(int shmid);

int   create_semaphore_set(int nsems);
void  sem_op(int semid, int sem_num, int op);  // wrapper semop
void  remove_semaphores(int semid);

int   create_message_queue();
void  send_message(int mqid, void *msg, size_t size);
void  receive_message(int mqid, void *msg, size_t size, long mtype);
void  remove_message_queue(int mqid);
```

Tutte le funzioni di creazione restituiscono l'ID IPC e stampano un errore fatale in caso di fallimento.

---

## 5. Processo responsabile_mensa (main)

### File: `src/responsabile_mensa.c`

### 5.1 Fase di inizializzazione

1. **Parsing configurazione**: legge il file `.conf` (path dal primo argomento o da `$MENSA_CONFIG`).
2. **Parsing menu**: legge `menu.txt` e popola la lista piatti (almeno 2 primi, 2 secondi, dolce+caffè).
3. **Creazione risorse IPC**:
   - Allocazione shared memory → `SharedData`.
   - Creazione set semafori → inizializzazione valori.
   - Creazione code di messaggi (una per stazione).
4. **Inizializzazione SharedData**:
   - `simulation_running = 1`, `day_running = 0`, `current_day = 0`.
   - Porzioni primi/secondi inizializzate a 0 (verranno riempite a inizio giornata).
   - `table_seats_free = NOF_TABLE_SEATS`.
   - Azzeramento statistiche.
   - Copia della configurazione nella shared memory.

### 5.2 Politica di assegnazione operatori alle stazioni

**Algoritmo:**

1. Calcola i tempi medi: `AVG_SRVC_PRIMI`, `AVG_SRVC_MAIN_COURSE`, `AVG_SRVC_COFFEE`, `AVG_SRVC_CASSA`.
2. Ordina le 4 stazioni per tempo medio decrescente.
3. Assegna 1 operatore a ciascuna stazione (4 operatori consumati).
4. Distribuisci i restanti `NOF_WORKERS - 4` operatori uno alla volta alla stazione con il tempo medio più alto, in ordine.
5. Rispetta i vincoli di `NOF_WK_SEATS_*`: non assegnare più operatori dei posti disponibili per ciascuna stazione.

**Risultato:** un array `assignment[NOF_WORKERS]` dove ciascun elemento indica la stazione assegnata (`STATION_PRIMI`, `STATION_SECONDI`, `STATION_COFFEE`, `STATION_CASSA`).

### 5.3 Creazione processi figli

Per ciascun operatore `i` (0..NOF_WORKERS-1):

```
pid = fork();
if (pid == 0) {
    // Prepara argomenti: shmid, semid, mqid, station_type, ...
    execve("bin/operatore" o "bin/operatore_cassa", argv, envp);
}
```

Per ciascun utente `j` (0..NOF_USERS-1):

```
pid = fork();
if (pid == 0) {
    execve("bin/utente", argv, envp);
}
```

### 5.4 Barriera di sincronizzazione (tutti pronti)

- Ogni processo figlio, al termine della propria inizializzazione, fa `sem_op(SEM_READY, +1)`.
- Il responsabile mensa attende che il contatore raggiunga `NOF_WORKERS + NOF_USERS`.
- **Implementazione:** loop con `semctl(GETVAL)` su `SEM_READY` fino al valore atteso, **oppure** (meglio, per evitare busy-wait) ogni figlio invia un messaggio "pronto" e il padre fa `NOF_WORKERS + NOF_USERS` receive.
- Alternativa elegante: usare un semaforo "barriera":
  - Il padre blocca tutti i figli su `SEM_DAY_START` (inizializzato a 0).
  - Quando tutti hanno segnalato "ready", il padre sblocca con `sem_op(SEM_DAY_START, +(NOF_WORKERS + NOF_USERS))`.

### 5.5 Loop giornaliero

Per `current_day = 1` fino a `SIM_DURATION`:

1. **Inizio giornata:**
   - Riassegna operatori alle stazioni (ricalcola la politica se necessario, o mantieni fissa).
   - Reinizializza porzioni: `porzioni_primo[i] = AVG_REFILL_PRIMI` per ogni tipo.
   - `porzioni_secondo[i] = AVG_REFILL_SECONDI` per ogni tipo.
   - `day_running = 1`.
   - Sblocca operatori e utenti (`SEM_DAY_START`).

2. **Durante la giornata:**
   - **Refill periodico** (ogni 10 minuti simulati):
     - Ogni 10 * `N_NANO_SECS` nanosecondi reali, il responsabile aggiunge porzioni fino a `MAX_PORZIONI_PRIMI` e `MAX_PORZIONI_SECONDI`.
     - Implementazione: `nanosleep()` + incremento atomico (protetto da semaforo mutex).
   - **Monitoraggio:** controlla periodicamente lo stato della simulazione.

3. **Fine giornata:**
   - Setta `day_running = 0`.
   - Attende che tutti gli utenti in fase di servizio completino (o desistano).
   - **Stampa statistiche** giornaliere e cumulative (vedi §11).
   - **Controllo overload:** se `utenti_in_attesa > OVERLOAD_THRESHOLD` → `termination_cause = OVERLOAD`, `simulation_running = 0`.

4. **Fine simulazione (dopo il loop o per overload):**
   - Setta `simulation_running = 0`.
   - Segnala terminazione a tutti i processi figli (tramite flag in shared memory + segnali SIGUSR1 o semafori).
   - `waitpid()` per tutti i figli.
   - Stampa statistiche finali + causa di terminazione.
   - **Cleanup IPC:** rimuovi shared memory, semafori, code di messaggi.

---

## 6. Risorse di tipo stazione

### File: definite in `include/shared_data.h`, gestite nei rispettivi processi

### 6.1 Stazioni distribuzione (primi, secondi, coffee)

Ciascuna stazione è definita da:

- **Numero postazioni** (`NOF_WK_SEATS_*`): il semaforo corrispondente gestisce i posti disponibili per gli operatori.
- **Coda utenti**: coda di messaggi per le richieste.
- **Porzioni** (solo primi/secondi): array in shared memory, decrementato dagli operatori, incrementato dal refill.
- **Coffee**: porzioni illimitate (nessun contatore).

### 6.2 Stazione cassa

- Simile ma senza porzioni.
- Riceve messaggi con la lista piatti scelti dall'utente → calcola il totale → aggiorna ricavo in shared memory.

### 6.3 Stazioni refezione (tavoli)

- Semplicemente un semaforo contatore `SEM_TABLE_SEATS` inizializzato a `NOF_TABLE_SEATS`.
- L'utente fa `sem_op(SEM_TABLE_SEATS, -1)` per sedersi (blocca se pieno).
- Dopo aver mangiato, fa `sem_op(SEM_TABLE_SEATS, +1)`.

### 6.4 Politica di assegnazione operatori (inizio giornata)

All'inizio di ogni giornata il responsabile_mensa:

1. Ordina le stazioni per `AVG_SRVC_*` decrescente.
2. Assegna 1 operatore a ciascuna delle 4 stazioni.
3. Distribuisci i restanti per priorità, rispettando `NOF_WK_SEATS_*`.

### 6.5 Alternativa di Design: Stazione come Processo Autonomo (stazione.c)

Nel piano d'azione originale abbiamo considerato le stazioni come entità puramente logiche gestite tramite risorse IPC (semafori e code di messaggi) su cui operano direttamente i processi dei singoli lavoratori (`operatore.c` e `operatore_cassa.c`).

Tuttavia, esiste l'opportunità di implementare un processo dedicato alle stazioni tramite un sorgente **`stazione.c`** (che compila nell'eseguibile `./bin/stazione`). Questa opzione apre la strada a due eccezionali varianti architetturali:

#### Variante B1: Processo Unificato "Lavoratore di Stazione" (Consolidamento)
Invece di compilare due file separati `operatore.c` and `operatore_cassa.c`, implementiamo un unico sorgente `stazione.c`.
- Ciascun operatore o cassiere è un processo che esegue `./bin/stazione`.
- All'avvio, il processo riceve come argomento il tipo di stazione assegnata (`0` per primi, `1` per secondi, `2` per coffee, `3` per cassa).
- Il codice di `stazione.c` adotta dinamicamente la logica corretta: esegue la competizione per la postazione corretta, gestisce le porzioni (se cibo) o il calcolo del conto (se cassa).
- **Perché sceglierlo:** Semplifica notevolmente il Makefile, unifica la gestione dei segnali (SIGUSR1 per fine giornata) e azzera la duplicazione di codice per l'attesa del broadcast di inizio giornata.

#### Variante B2: Processo "Manager di Stazione" + Thread Operatori
Il responsabile mensa avvia esattamente 4 processi `./bin/stazione` (uno per Primi, uno per Secondi, uno per Coffee, uno per Cassa).
- Ciascun processo stazione gestisce in modo centralizzato la propria coda di messaggi, leggendo le richieste degli utenti e coordinando l'erogazione.
- Le postazioni di lavoro (`seats`) all'interno di ciascuna stazione sono gestite tramite **thread concorrenti** (`pthread_create`) interni al processo della stazione stessa.
- Gli operatori sono quindi modellati come thread all'interno del processo stazione, semplificando la sincronizzazione delle pause e la gestione della memoria locale.
- **Perché sceglierlo:** Altamente aderente all'astrazione ad oggetti e modulare, riduce drasticamente il numero totale di processi pesanti attivi nel sistema operativo.

---

## 7. Processo operatore

### File: `src/operatore.c`

### Argomenti ricevuti (via `argv` o variabili d'ambiente)

- `shmid` → ID shared memory
- `semid` → ID set semafori
- `mqid_station` → ID coda messaggi della stazione assegnata
- `mqid_served` → ID coda risposte
- `station_type` → tipo di stazione (`PRIMI`, `SECONDI`, `COFFEE`)

### Ciclo di vita (per ogni giornata)

```
LOOP giornata:
  1. Attende segnale inizio giornata (SEM_DAY_START)
  2. Compete per un posto nella stazione: sem_op(SEM_SEATS_<station>, -1)
     - Se non trova posto → resta bloccato finché un altro operatore va in pausa
  3. Incrementa active_operators nella StationData
  4. LOOP servizio:
     a. Controlla: day_running == 0? → esci dal loop servizio
     b. Controlla: devo andare in pausa? (vedi §7.1)
     c. msgrcv(mqid_station, &msg, mtype=1, IPC_NOWAIT o con timeout)
        - Se nessun utente in coda → breve attesa e riprova
        - Se utente ricevuto:
          i.   Genera tempo di servizio casuale: AVG_SRVC_* ± variazione%
          ii.  nanosleep(tempo di servizio convertito)
          iii. [Solo primi/secondi] Lock mutex porzioni → decrementa porzione → unlock
               - Se porzione esaurita → invia risposta "esaurito" all'utente
               - Altrimenti → invia risposta "servito"
          iv.  [Coffee] Sempre "servito" (illimitato)
          v.   Aggiorna statistiche (piatti distribuiti, tempo attesa)
  5. Fine giornata → rilascia posto: sem_op(SEM_SEATS_<station>, +1)
  6. Decrementa active_operators
```

### 7.1 Gestione pause

- L'operatore tiene un contatore `pause_taken` (locale).
- Criterio per decidere la pausa: casuale (es. ogni X utenti serviti, probabilità `1/K`), oppure periodico.
- **Vincolo critico**: prima di andare in pausa, verificare che `active_operators > 1` per la propria stazione.
  - **Implementazione:** semaforo "guardia" (`SEM_PAUSE_GUARD`) o check su `StationData.active_operators` protetto da mutex.
  - Se `active_operators == 1` → non andare in pausa.
- **Procedura pausa:**
  1. Termina di servire il cliente attuale.
  2. Lock mutex → decrementa `active_operators` → unlock.
  3. `sem_op(SEM_SEATS_<station>, +1)` → libera il posto (un operatore in attesa può entrare).
  4. `nanosleep(durata_pausa_casuale)`.
  5. `sem_op(SEM_SEATS_<station>, -1)` → riprende il posto.
  6. Lock mutex → incrementa `active_operators` → unlock.
  7. Aggiorna statistiche pause.

---

## 8. Processo operatore_cassa

### File: `src/operatore_cassa.c`

Analogo all'operatore ma con logica specifica:

### Ciclo di vita

```
LOOP giornata:
  1. Attende inizio giornata (SEM_DAY_START)
  2. Compete per posto cassa: sem_op(SEM_SEATS_CASSA, -1)
  3. LOOP servizio:
     a. msgrcv(mq_cassa, &msg, mtype=1)
     b. Legge lista piatti dall'utente
     c. Calcola totale:
        totale = n_primi * PRICE_PRIMI + n_secondi * PRICE_SECONDI + n_coffee * PRICE_COFFEE
     d. Genera tempo servizio: AVG_SRVC_CASSA ± 20%
     e. nanosleep(tempo)
     f. Lock mutex stats → aggiorna ricavo_giornaliero, ricavo_totale → unlock
     g. Invia risposta "pagamento ok" all'utente (mtype = user_pid)
  4. Fine giornata → rilascia posto
```

### Gestione pause (identica all'operatore generico)

- Max `NOF_PAUSE` pause.
- Vincolo: almeno 1 cassiere attivo.

---

## 9. Processo utente

### File: `src/utente.c`

### Argomenti ricevuti

- `shmid`, `semid`, `mqid_primi`, `mqid_secondi`, `mqid_coffee`, `mqid_cassa`, `mqid_served`
- `user_id` (indice logico)

### Ciclo di vita (per ogni giornata)

```
LOOP giornata:
  1. Attende inizio giornata (SEM_DAY_START)
  2. Sceglie il menu:
     a. Legge menu.txt (o la copia in shared memory)
     b. Sceglie almeno un primo, un secondo, e opzionalmente dolce-caffè
     c. Strategia di scelta: casuale tra le opzioni disponibili
  3. Decide l'ordine di visita alle stazioni (basato sulle code):
     a. Legge queue_length di stazione_primi e stazione_secondi (e coffee se scelto)
     b. Va prima alla stazione con coda più corta
  4. PER CIASCUNA stazione scelta:
     a. Incrementa queue_length della stazione (protetto da mutex)
     b. Invia richiesta sulla coda messaggi della stazione
     c. Attende risposta (msgrcv con mtype = proprio pid)
     d. Decrementa queue_length
     e. SE risposta = "esaurito":
        - Sceglie un altro piatto dello stesso tipo
        - Se tutti i piatti del tipo esauriti → salta quel tipo
     f. SE tutti i piatti di ENTRAMBI i tipi esauriti → desiste, esce (incrementa utenti_non_serviti)
     g. Registra il tempo di attesa per le statistiche
  5. [Se ha almeno un piatto] Va alla cassa:
     a. Incrementa queue_length cassa
     b. Invia messaggio con lista piatti scelti
     c. Attende risposta "pagamento ok"
     d. Decrementa queue_length cassa
  6. Si siede a un tavolo:
     a. sem_op(SEM_TABLE_SEATS, -1)  → blocca se pieno
     b. Mangia per un tempo proporzionale al numero di piatti:
        tempo_mangiare = n_piatti * K_EATING_TIME * N_NANO_SECS
     c. sem_op(SEM_TABLE_SEATS, +1)  → libera il posto
  7. Aggiorna statistiche: utente servito
  8. Fine giornata: se è ancora in coda → incrementa utenti_non_serviti, esce dalla coda
```

### 9.1 Gestione fine giornata mentre si è in coda

- L'utente controlla periodicamente `day_running` nella shared memory.
- **Implementazione con timeout:** usare `msgrcv` con `IPC_NOWAIT` in un loop che controlla `day_running`, oppure utilizzare un segnale (SIGUSR1) inviato dal responsabile mensa a fine giornata per interrompere la `msgrcv`.
- Se `day_running == 0` e l'utente non è stato servito → abbandona.

### 9.2 Scelta del piatto quando è esaurito

```
Piatto scelto esaurito?
├── SÌ → Prova altro piatto dello stesso tipo
│        ├── Trovato → procedi
│        └── Tutti esauriti per quel tipo?
│             ├── Ha almeno un piatto dell'altro tipo → continua (mangia solo quello)
│             └── Entrambi i tipi esauriti → desiste, esce
└── NO → servito, procedi
```

---

## 10. Gestione del tempo simulato

### File: `include/time_utils.h`, `src/time_utils.c`

### Principio

- 1 minuto simulato = `N_NANO_SECS` nanosecondi reali.
- 1 secondo simulato = `N_NANO_SECS / 60` nanosecondi reali.
- 1 giornata simulata (ipotesi: 8 ore lavorative = 480 minuti) = `480 * N_NANO_SECS` nanosecondi reali.

### Funzioni

```c
// Converte secondi simulati in struct timespec per nanosleep
struct timespec sim_seconds_to_real(int sim_seconds, long n_nano_secs);

// Genera un tempo casuale nell'intorno ± percent% del valore medio
int random_service_time(int avg, int percent);
// Esempio: random_service_time(5, 50) → [2.5, 7.5] arrotondato

// Converte minuti simulati in nanosecondi reali
long sim_minutes_to_nanos(int sim_minutes, long n_nano_secs);

// Sleep per un tempo simulato (wrapper di nanosleep con gestione EINTR)
void sim_sleep(int sim_seconds, long n_nano_secs);
```

### Generazione tempi casuali

Per ciascuna stazione, il tempo di servizio è:

| Stazione | Media           | Variazione |
|----------|-----------------|------------|
| Primi    | `AVG_SRVC_PRIMI`       | ±50% |
| Secondi  | `AVG_SRVC_MAIN_COURSE` | ±50% |
| Coffee   | `AVG_SRVC_COFFEE`      | ±80% |
| Cassa    | `AVG_SRVC_CASSA`       | ±20% |

```c
// Esempio per primi:
int t = random_service_time(cfg->avg_srvc_primi, 50);
sim_sleep(t, cfg->n_nano_secs);
```

---

## 11. Statistiche

### File: `include/stats.h`, `src/stats.c`

### Struttura dati (in shared memory)

```c
typedef struct {
    // --- Per giornata (resettate a inizio giorno) ---
    int   users_served_today;
    int   users_not_served_today;
    int   dishes_primi_today;
    int   dishes_secondi_today;
    int   dishes_coffee_today;
    int   dishes_leftover_primi_today;
    int   dishes_leftover_secondi_today;
    long  wait_time_total_today;         // nanosecondi cumulati
    long  wait_time_primi_today;
    long  wait_time_secondi_today;
    long  wait_time_coffee_today;
    long  wait_time_cassa_today;
    int   wait_count_today;              // numero di attese (per calcolo media)
    int   wait_count_primi_today;
    int   wait_count_secondi_today;
    int   wait_count_coffee_today;
    int   wait_count_cassa_today;
    int   active_operators_today;
    int   pauses_today;
    float revenue_today;

    // --- Cumulativi (su tutta la simulazione) ---
    int   users_served_total;
    int   users_not_served_total;
    int   dishes_primi_total;
    int   dishes_secondi_total;
    int   dishes_coffee_total;
    int   dishes_leftover_primi_total;
    int   dishes_leftover_secondi_total;
    long  wait_time_total_sim;
    long  wait_time_primi_sim;
    long  wait_time_secondi_sim;
    long  wait_time_coffee_sim;
    long  wait_time_cassa_sim;
    int   wait_count_total_sim;
    int   wait_count_primi_sim;
    int   wait_count_secondi_sim;
    int   wait_count_coffee_sim;
    int   wait_count_cassa_sim;
    int   active_operators_total;
    int   pauses_total;
    float revenue_total;
    int   days_completed;
} SimStats;
```

### Chi aggiorna le statistiche

| Dato                      | Aggiornato da          | Quando                           |
|---------------------------|------------------------|----------------------------------|
| `users_served`            | Utente                 | Dopo aver pagato e mangiato       |
| `users_not_served`        | Utente                 | Quando desiste o fine giornata    |
| `dishes_*_today/total`    | Operatore              | Dopo aver servito un piatto       |
| `dishes_leftover_*`       | Responsabile mensa     | A fine giornata (porzioni rimaste)|
| `wait_time_*`             | Utente                 | Differenza timestamp pre/post coda|
| `active_operators`        | Operatore              | A inizio/fine turno               |
| `pauses`                  | Operatore              | Ogni pausa effettuata             |
| `revenue`                 | Operatore cassa        | Dopo ogni pagamento               |

### Protezione accesso

Ogni aggiornamento è protetto da `SEM_MUTEX_STATS` per garantire atomicità.

### Funzione di stampa (chiamata dal responsabile mensa)

```c
void print_daily_stats(const SimStats *s, int day);
void print_final_stats(const SimStats *s, int termination_cause);
```

**Output a fine giornata:**

```
=== GIORNATA 2 ===
Utenti serviti oggi: 42        | Totale simulazione: 87
Utenti non serviti oggi: 8     | Totale simulazione: 13
Piatti primi distribuiti oggi: 38    | Totale: 80
Piatti secondi distribuiti oggi: 35  | Totale: 72
Piatti caffè distribuiti oggi: 25    | Totale: 51
Piatti primi avanzati oggi: 2        | Totale: 5
...
Tempo medio attesa complessivo oggi: 3.2 min sim
Tempo medio attesa primi oggi: 2.8 min sim
...
Operatori attivi oggi: 7 | Totale simulazione: 14
Pause effettuate oggi: 12 | Totale simulazione: 25
Ricavo oggi: €234.50 | Totale: €478.00
Media piatti primi/giorno: 40.0
...
```

### Reset giornaliero

A inizio di ogni giornata il responsabile mensa:

1. Salva i dati giornalieri in quelli cumulativi (addizione).
2. Azzera tutti i campi `*_today`.
3. Incrementa `days_completed`.

---

## 12. Terminazione

### Due cause possibili

1. **Timeout**: `current_day > SIM_DURATION` → terminazione naturale.
2. **Overload**: a fine giornata, se il numero di utenti ancora in coda (= `sum(queue_length)` di tutte le stazioni) > `OVERLOAD_THRESHOLD`.

### Procedura di terminazione

```
1. responsabile_mensa setta:
   simulation_running = 0
   termination_cause = TIMEOUT o OVERLOAD

2. Invia SIGUSR1 a tutti i processi figli
   (per svegliare chi è bloccato su msgrcv/nanosleep/semop)

3. Ogni processo figlio:
   - Gestore SIGUSR1: setta un flag locale `should_exit = 1`
   - Il loop principale controlla `should_exit` e `simulation_running`
   - Rilascia risorse locali
   - exit(0)

4. responsabile_mensa:
   - waitpid() per tutti i figli (o waitpid(-1, ...) in loop)
   - Stampa statistiche finali + causa di terminazione
   - Cleanup IPC:
     shmctl(shmid, IPC_RMID, NULL)
     semctl(semid, 0, IPC_RMID)
     msgctl(mqid_*, IPC_RMID, NULL)
   - exit(0)
```

### Gestione segnali

```c
// In ogni processo figlio:
volatile sig_atomic_t should_exit = 0;

void handler_sigusr1(int sig) {
    (void)sig;
    should_exit = 1;
}

// Nel main del figlio:
struct sigaction sa;
sa.sa_handler = handler_sigusr1;
sigemptyset(&sa.sa_mask);
sa.sa_flags = 0; // NO SA_RESTART: per interrompere le syscall bloccanti
sigaction(SIGUSR1, &sa, NULL);
```

### Gestione SIGINT (pulizia di emergenza)

Il `responsabile_mensa` installa un handler per SIGINT/SIGTERM che:

1. Setta `simulation_running = 0`.
2. Invia SIGTERM a tutti i figli.
3. `waitpid()` per tutti.
4. Cleanup IPC.
5. `exit(1)`.

---

## 13. Makefile e compilazione

### Flag obbligatori

```makefile
CC       = gcc
CFLAGS   = -Wvla -Wextra -Werror -D_GNU_SOURCE -Iinclude
LDFLAGS  = -lrt -lpthread   # se serve
```

### Struttura Makefile

```makefile
CC       = gcc
CFLAGS   = -Wvla -Wextra -Werror -D_GNU_SOURCE -Iinclude
LDFLAGS  =

SRC_DIR  = src
BUILD_DIR = build
BIN_DIR  = bin
INC_DIR  = include

# Moduli comuni (compilati come .o e linkati in ogni eseguibile)
COMMON_OBJS = $(BUILD_DIR)/config.o $(BUILD_DIR)/ipc_utils.o \
              $(BUILD_DIR)/time_utils.o $(BUILD_DIR)/stats.o

# Eseguibili
TARGETS = $(BIN_DIR)/responsabile_mensa \
          $(BIN_DIR)/stazione \
          $(BIN_DIR)/utente

# Target alternativi (se si usano eseguibili separati per gli operatori)
TARGETS_ALT = $(BIN_DIR)/operatore \
              $(BIN_DIR)/operatore_cassa

all: dirs $(TARGETS) $(TARGETS_ALT)

dirs:
	@mkdir -p $(BUILD_DIR) $(BIN_DIR)

# Regola generica per .c → .o
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Eseguibili (ciascuno linka i propri .o + i moduli comuni)
$(BIN_DIR)/responsabile_mensa: $(BUILD_DIR)/responsabile_mensa.o $(COMMON_OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(BIN_DIR)/stazione: $(BUILD_DIR)/stazione.o $(COMMON_OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(BIN_DIR)/operatore: $(BUILD_DIR)/operatore.o $(COMMON_OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(BIN_DIR)/operatore_cassa: $(BUILD_DIR)/operatore_cassa.o $(COMMON_OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(BIN_DIR)/utente: $(BUILD_DIR)/utente.o $(COMMON_OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

clean:
	rm -rf $(BUILD_DIR)/*.o $(BIN_DIR)/*

.PHONY: all clean dirs
```

---

## 14. File di configurazione di test

### `conf/config_timeout.conf`

Configurazione che genera terminazione per **timeout** (simulazione lunga ma risorse sufficienti):

```conf
SIM_DURATION         = 2
N_NANO_SECS          = 500000
NOF_WORKERS          = 10
NOF_USERS            = 20
NOF_WK_SEATS_PRIMI   = 3
NOF_WK_SEATS_SECONDI = 3
NOF_WK_SEATS_COFFEE  = 2
NOF_WK_SEATS_CASSA   = 2
NOF_TABLE_SEATS      = 30
AVG_SRVC_PRIMI       = 3
AVG_SRVC_MAIN_COURSE = 4
AVG_SRVC_COFFEE      = 2
AVG_SRVC_CASSA       = 2
AVG_REFILL_PRIMI     = 30
AVG_REFILL_SECONDI   = 30
MAX_PORZIONI_PRIMI   = 60
MAX_PORZIONI_SECONDI = 60
NOF_PAUSE            = 2
PRICE_PRIMI          = 4.50
PRICE_SECONDI        = 5.50
PRICE_COFFEE         = 1.20
OVERLOAD_THRESHOLD   = 999
MENU_FILE            = conf/menu.txt
```

> `OVERLOAD_THRESHOLD = 999` → disabilita di fatto la terminazione per overload.

### `conf/config_overload.conf`

Configurazione che genera terminazione per **overload** (troppi utenti, poche risorse):

```conf
SIM_DURATION         = 10
N_NANO_SECS          = 500000
NOF_WORKERS          = 4
NOF_USERS            = 100
NOF_WK_SEATS_PRIMI   = 1
NOF_WK_SEATS_SECONDI = 1
NOF_WK_SEATS_COFFEE  = 1
NOF_WK_SEATS_CASSA   = 1
NOF_TABLE_SEATS      = 5
AVG_SRVC_PRIMI       = 8
AVG_SRVC_MAIN_COURSE = 10
AVG_SRVC_COFFEE      = 6
AVG_SRVC_CASSA       = 5
AVG_REFILL_PRIMI     = 10
AVG_REFILL_SECONDI   = 10
MAX_PORZIONI_PRIMI   = 15
MAX_PORZIONI_SECONDI = 15
NOF_PAUSE            = 3
PRICE_PRIMI          = 4.50
PRICE_SECONDI        = 5.50
PRICE_COFFEE         = 1.20
OVERLOAD_THRESHOLD   = 10
MENU_FILE            = conf/menu.txt
```

> Tanti utenti, pochi operatori, pochi posti → overflow sicuro.

### `conf/menu.txt`

```
# Menu della mensa - Oasi del Golfo
# Formato: TIPO:NOME_PIATTO

PRIMO:Pasta al pomodoro
PRIMO:Risotto ai funghi
SECONDO:Cotoletta con patate
SECONDO:Pesce al forno con verdure
COFFEE:Caffè normale
COFFEE:Caffè macchiato
COFFEE:Caffè decaffeinato
COFFEE:Caffè al ginseng
DOLCE:Tiramisù
```

---

## 15. Piano di verifica

### 15.1 Compilazione

```bash
make clean && make
# Deve compilare senza warning né errori con -Wvla -Wextra -Werror
```

### 15.2 Test funzionali

| Test | Descrizione | Verifica |
|------|-------------|----------|
| T1 - Avvio base | `./bin/responsabile_mensa conf/config.conf` | Tutti i processi creati, barriera funzionante |
| T2 - Timeout | `./bin/responsabile_mensa conf/config_timeout.conf` | Termina per timeout dopo SIM_DURATION giorni |
| T3 - Overload | `./bin/responsabile_mensa conf/config_overload.conf` | Termina per overload |
| T4 - Statistiche | Confrontare le statistiche stampate con i piatti/utenti effettivi | Coerenza dei numeri |
| T5 - Cleanup IPC | Dopo terminazione: `ipcs` non mostra risorse residue | Pulizia corretta |
| T6 - SIGINT | Ctrl+C durante esecuzione | Tutti i figli terminano, IPC ripulite |
| T7 - Concorrenza | Esecuzione su macchina con ≥2 core | Nessun deadlock, nessuna race condition |
| T8 - No busy-wait | `top`/`htop` durante esecuzione | CPU usage non al 100% per nessun processo |
| T9 - Piatti esauriti | Config con poche porzioni | Utenti cambiano piatto o desistono correttamente |
| T10 - Posti tavolo pieni | Config con pochi `NOF_TABLE_SEATS` | Utenti attendono, nessun deadlock |

### 15.3 Strumenti di debug

- `ipcs` — verifica risorse IPC allocate
- `ipcrm` — rimozione manuale in caso di crash
- `strace -f` — traccia delle syscall per debug IPC
- `valgrind` — memory leak (limitato con fork/exec ma utile pre-fork)

---

## 16. Ordine di implementazione (fasi)

### Fase 0: Infrastruttura (1-2 giorni)

- [v] Creare la struttura directory (`include/`, `src/`, `conf/`, `bin/`, `build/`)
- [v] Implementare `config.h` / `config.c` — parsing del file di configurazione
- [v] Implementare `common.h` — costanti, macro, include di sistema
- [v] Implementare `ipc_utils.h` / `ipc_utils.c` — creazione/cleanup risorse IPC
- [v] Implementare `time_utils.h` / `time_utils.c` — conversione tempi, random timing, sim_sleep
- [v] Scrivere il Makefile completo (4 target eseguibili + moduli comuni)
- [v] Creare `conf/config.conf`, `conf/menu.txt` con valori di default
- [v] Verificare: `make` compila senza errori tutti gli eseguibili (con `main` vuoti)

### Fase 1: Responsabile mensa — scheletro (2-3 giorni)

- [v] `responsabile_mensa.c`: parsing config, creazione risorse IPC
- [v] Inizializzazione SharedData
- [v] Politica di assegnazione operatori
- [v] Fork+exec di un singolo operatore (test IPC)
- [v] Fork+exec di un singolo utente (test IPC)
- [v] Barriera di sincronizzazione
- [v] Loop giornaliero minimale (inizio → fine giornata, senza refill)
- [v] Cleanup IPC e waitpid
- [v] Verificare: `ipcs` prima e dopo l'esecuzione → nessuna risorsa residua

### Fase 2: Operatore — funzionamento base (2-3 giorni)

- [v] `operatore.c`: attach shared memory, setup signal handler
- [v] Segnalazione "pronto" al responsabile (barriera)
- [v] Attesa inizio giornata
- [v] Competizione per il posto alla stazione (semaforo)
- [v] Loop di servizio: ricezione messaggio, simulazione tempo, risposta
- [v] Decremento porzioni (primi/secondi) con mutex
- [v] Gestione piatto esaurito → risposta "esaurito"
- [v] Fine giornata → rilascio posto
- [v] Verificare: con un utente e un operatore, il flusso funziona

### Fase 3: Operatore cassa (1-2 giorni)

- [v] Ricezione lista piatti, calcolo totale, aggiornamento ricavo
- [v] Tempo di servizio con variazione ±20%
- [v] Verificare: utente → stazione → cassa funziona end-to-end

### Fase 4: Utente — flusso completo (2-3 giorni)

- [v] `utente.c`: scelta menu, decisione ordine stazioni
- [v] Invio richiesta alla stazione, attesa risposta
- [v] Gestione piatto esaurito (fallback ad altro piatto o desistenza)
- [v] Passaggio alla cassa
- [v] Seduta al tavolo (semaforo) + tempo di consumo
- [v] Aggiornamento statistiche utente servito/non servito
- [v] Gestione fine giornata in coda (SIGUSR1 o check periodico)
- [v] Verificare: simulazione completa con N utenti e M operatori

### Fase 5: Refill porzioni (1 giorno)

- [ ] Timer nel responsabile mensa: ogni 10 minuti simulati
- [ ] Incremento porzioni fino al massimo
- [ ] Protezione con mutex
- [ ] Verificare: con poche porzioni iniziali e refill, gli utenti vengono serviti anche tardi nella giornata

### Fase 6: Pause operatore (1-2 giorni)

- [ ] Logica di decisione pausa (criterio casuale o periodico)
- [ ] Vincolo: almeno 1 operatore attivo per stazione
- [ ] Rilascio/riacquisizione posto
- [ ] Aggiornamento statistiche pause
- [ ] Verificare: nessun deadlock quando operatori vanno in pausa

### Fase 7: Statistiche complete (1-2 giorni)

- [ ] Implementare tutte le voci statistiche richieste
- [ ] `stats.c`: funzioni di aggiornamento thread-safe (con semaforo mutex)
- [v] `print_daily_stats()` e `print_final_stats()`
- [v] Reset giornaliero
- [ ] Calcolo medie (al giorno, per stazione)
- [v] Piatti avanzati = porzioni restanti a fine giornata
- [ ] Verificare: confronto manuale numeri statistiche con log di debug

### Fase 8: Terminazione e segnali (1-2 giorni)

- [ ] Terminazione per timeout
- [ ] Terminazione per overload
- [ ] Signal handler SIGUSR1 in tutti i figli
- [ ] Signal handler SIGINT nel responsabile mensa (cleanup di emergenza)
- [ ] Stampa causa di terminazione
- [ ] Verificare: T2 (timeout), T3 (overload), T6 (SIGINT)

### Fase 9: Robustezza e test finali (2-3 giorni)

- [ ] Creare `conf/config_timeout.conf` e `conf/config_overload.conf`
- [ ] Test con diverse configurazioni
- [ ] Verificare assenza di deadlock con `strace`
- [ ] Verificare assenza di busy-wait con `top`/`htop`
- [ ] Test su macchina con ≥2 CPU
- [ ] Verificare cleanup IPC con `ipcs` dopo crash (+ aggiungere script `cleanup_ipc.sh`)
- [ ] Codice pulito: commenti, formattazione, nomi variabili significativi

### Fase 10: Documentazione (1 giorno)

- [ ] Breve relazione sulle scelte progettuali:
  - Architettura multi-processo
  - Meccanismi IPC scelti e motivazioni
  - Politica assegnazione operatori
  - Gestione sincronizzazione e concorrenza
  - Gestione terminazione
  - Struttura file di configurazione
- [ ] README con istruzioni di compilazione e utilizzo

---

## Stima tempo totale: ~15-20 giorni di lavoro

| Fase | Giorni stimati |
|------|:-:|
| 0 - Infrastruttura | 1-2 |
| 1 - Responsabile mensa | 2-3 |
| 2 - Operatore | 2-3 |
| 3 - Operatore cassa | 1-2 |
| 4 - Utente | 2-3 |
| 5 - Refill | 1 |
| 6 - Pause | 1-2 |
| 7 - Statistiche | 1-2 |
| 8 - Terminazione | 1-2 |
| 9 - Test e robustezza | 2-3 |
| 10 - Documentazione | 1 |
| **Totale** | **~15-20** |

---

> **Nota finale:** Questo piano copre la **versione minima (24/30)**. La versione completa (30/30) aggiunge: processo `communication_disorder`, aggiunta dinamica di utenti, output CSV delle statistiche, e distinzione utenti con/senza ticket.

Edited common.h
Edited responsabile_mensa.c
Viewed responsabile_mensa.c

Il **`responsabile_mensa`** è il processo padre coordinatore (l'orchestratore) dell'intera simulazione. Il suo ciclo di vita si articola in cinque fasi logiche fondamentali.

Di seguito trovi la spiegazione concettuale di ciascuna fase, con consigli pratici su come strutturare la logica e piccolissimi esempi generici per illustrare i principi implementativi in C.

---

### Fase 1: Inizializzazione (Fase Critica)

Prima di creare qualsiasi processo figlio, il responsabile deve preparare l'ambiente di esecuzione:

1. **Lettura Configurazione**: Chiama la funzione di parsing per caricare la struttura `Config` a partire dal file `.conf`.
2. **Allocazione Risorse IPC**:
   - Alloca un segmento di memoria condivisa (`shmget`) sufficientemente grande da contenere la struttura `SharedData`.
   - Crea il set di semafori (`semget`) necessari a coordinare i processi concorrenti.
   - Crea le code di messaggi (`msgget`) per lo scambio di richieste/risposte tra utenti ed operatori.
3. **Inizializzazione SHM**: Associa il segmento shm al proprio spazio di indirizzamento (`shmat`) e popola i valori iniziali della simulazione (es. azzeramento statistiche, copia della configurazione read-only).

> [!IMPORTANT]
> Se una qualsiasi chiamata alle funzioni IPC fallisce durante questa fase, il programma deve immediatamente interrompersi effettuando la pulizia delle risorse già allocate per evitare memory leak nel sistema operativo.

---

### Fase 2: Politica di Assegnazione e Fork-Exec (Lancio degli Attori)

Il responsabile mensa decide quanti operatori assegnare a ciascuna stazione di distribuzione del cibo seguendo i parametri letti. Una volta calcolato il piano di assegnazione (es. tramite un array locale), il padre avvia i figli.

Il progetto richiede l'uso di **processi separati**, perciò la creazione di ciascun figlio deve seguire il pattern `fork()` + `execve()`.

*Esempio generico di ciclo fork-exec (massimo 3 righe):*

```c
if (fork() == 0) {
    execve("./bin/utente", child_argv, envp);
}
```

---

### Fase 3: Barriera di Sincronizzazione (La Partenza)

Per evitare che gli utenti inizino a mettersi in coda prima che tutti gli operatori siano pronti ed abbiano agganciato la memoria condivisa, è necessaria una barriera di sincronizzazione all'avvio.

- **Come funziona?** Il padre inizializza un semaforo a `0` (es. `SEM_READY`). Ciascun processo figlio, appena completata la propria inizializzazione locale, incrementa il semaforo di 1.
- Il responsabile mensa esegue un'attesa finché il valore del semaforo non raggiunge la somma esatta di tutti gli operatori e gli utenti generati (`NOF_WORKERS + NOF_USERS`).

---

### Fase 4: Ciclo Giornaliero e Refill Periodico

La simulazione si sviluppa su un ciclo di giornate lavorative (da `1` a `SIM_DURATION`). Per ogni giornata il responsabile deve:

1. **Avvio Giornata**: Rifornire le porzioni iniziali di cibo in memoria condivisa e notificare l'inizio giornata tramite un semaforo di broadcast.
2. **Durante la Giornata (Refill)**: Eseguire un ciclo parallelo (o un timer basato sul tempo simulato) che ogni *10 minuti simulati* ripristina le porzioni fino al valore massimo impostato. L'accesso a questi contatori in shm deve essere protetto da semafori di mutua esclusione (*mutex*).
3. **Fine Giornata**: Interrompere il flag di servizio, attendere il completamento degli utenti correnti, stampare le statistiche giornaliere ed effettuare il controllo di **overload** (se gli utenti rimasti in coda superano la soglia massima).

---

### Fase 5: Terminazione e Pulizia Finale (Gestione Sicurezza)

Quando la simulazione finisce (per timeout naturale o per overload), il coordinatore deve spegnere il sistema in modo pulito:

1. Invia un segnale di interruzione (es. `SIGUSR1`) a tutti i figli rimasti attivi per sbloccarli dalle chiamate bloccanti (`msgrcv`, `semop`).
2. Attende la terminazione di ciascun processo figlio raccogliendo il relativo stato con un ciclo di `waitpid()`.
3. Stampa le statistiche finali cumulative.
4. **Rimozione Risorse IPC (FONDAMENTALE)**: Rilascia esplicitamente tutte le risorse System V create all'avvio. Se dimentichi questo passaggio, la memoria shm e i semafori rimarranno allocati nel kernel del sistema operativo anche dopo la chiusura del programma.

*Esempio generico di rimozione risorsa IPC (massimo 3 righe):*

```c
shmctl(shmid, IPC_RMID, NULL);
semctl(semid, 0, IPC_RMID);
```
