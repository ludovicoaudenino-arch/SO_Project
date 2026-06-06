# Cafeteria Simulation — Operating Systems Project 2025/26

Multi-process simulation of a university cafeteria using System V IPC
(shared memory, semaphores, message queues).

## Requirements

- GCC (C99 or later)
- GNU Make
- Linux with System V IPC support

## Build

```bash
make clean all
```

This compiles three executables into `bin/`:
- `responsabile_mensa` — Coordinator process
- `operatore` — Worker (operator) process
- `utente` — User (student) process

Compilation flags: `-Wvla -Wextra -Werror -D_GNU_SOURCE -Iinclude`

## Run

```bash
# Default configuration
make run

# Timeout termination (completes all SIM_DURATION days)
make run CONF=conf/config_timeout.conf

# Overload termination (queue exceeds threshold)
make run CONF=conf/config_overload.conf

# Direct execution with a specific config
./bin/responsabile_mensa conf/config_timeout.conf
```

## Configuration

All parameters are read at runtime from a configuration file with the
format `KEY = VALUE` (one per line). Lines starting with `#` are comments.

| Parameter             | Description                                    |
|-----------------------|------------------------------------------------|
| `SIM_DURATION`        | Simulation duration in days                    |
| `N_NANO_SECS`         | Real nanoseconds per simulated minute          |
| `NOF_WORKERS`         | Total number of operators                      |
| `NOF_USERS`           | Total number of users                          |
| `NOF_WK_SEATS_*`      | Queue capacity per station                     |
| `NOF_TABLE_SEATS`     | Available table seats                          |
| `AVG_SRVC_*`          | Average service time per station (sim seconds) |
| `AVG_REFILL_*`        | Initial/refill portions for primi and secondi  |
| `MAX_PORZIONI_*`      | Maximum portions per dish type                 |
| `NOF_PAUSE`           | Allowed breaks per operator                    |
| `PRICE_*`             | Price per dish type (EUR)                      |
| `OVERLOAD_THRESHOLD`  | Queue threshold for overload termination       |
| `MENU_FILE`           | Path to the menu file                          |

If no file is specified on the command line, the system checks the
`MENSA_CONFIG` environment variable, then falls back to `conf/config.conf`.

## Menu File

The menu file (`conf/menu.txt`) lists available dishes with the format
`TYPE:DISH_NAME`. Recognized types: `PRIMO`, `SECONDO`, `COFFEE`, `DOLCE`.

## Termination Conditions

1. **Timeout** — All `SIM_DURATION` days completed.
2. **Overload** — Users waiting at end of day exceed `OVERLOAD_THRESHOLD`.
3. **Signal (SIGINT)** — External interruption triggers graceful shutdown.

## IPC Cleanup

If the simulation is forcefully killed (e.g. `kill -9`), System V IPC
resources may remain allocated. Verify and clean up with:

```bash
ipcs        # List all IPC resources
ipcrm -a    # Remove all IPC resources
```

## Project Structure

```
SO_Project/
├── Makefile
├── README.md
├── conf/
│   ├── config.conf
│   ├── config_timeout.conf
│   ├── config_overload.conf
│   └── menu.txt
├── include/
│   ├── common.h
│   ├── config.h
│   ├── ipc_utils.h
│   ├── shared_data.h
│   ├── stats.h
│   └── time_utils.h
├── src/
│   ├── responsabile_mensa.c
│   ├── operatore.c
│   ├── utente.c
│   ├── config.c
│   ├── ipc_utils.c
│   ├── time_utils.c
│   └── stats.c
├── relazione_finale/
│   └── relazione.tex
├── bin/
└── build/
```

## Author

Ludovico Audenino — Università degli Studi di Torino, A.A. 2025/26
