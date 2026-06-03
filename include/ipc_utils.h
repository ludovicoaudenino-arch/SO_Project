#ifndef IPC_UTILS_H
#define IPC_UTILS_H

#include "shared_data.h"
#include <stddef.h>

/**
 * @file ipc_utils.h
 * @brief Inter-Process Communication (IPC) utilities.
 *
 * This file provides wrapper functions for creating, managing,
 * and destroying System V IPC resources such as Shared Memory,
 * Semaphores, and Message Queues. All functions include
 * built-in error handling.
 */

/** @name Shared Memory Utilities */
/** @{ */

/**
 * @brief Creates a new System V shared memory segment.
 * @param size The size of the shared memory segment in bytes.
 * @return The shared memory ID (shmid).
 */
int create_shared_memory(size_t size);

/**
 * @brief Attaches a shared memory segment to the process's address space.
 * @param shmid The shared memory ID.
 * @return A pointer to the attached shared memory.
 */
void *attach_shared_memory(int shmid);

/**
 * @brief Detaches a shared memory segment from the process's address space.
 * @param ptr Pointer to the previously attached shared memory.
 */
void detach_shared_memory(void *ptr);

/**
 * @brief Removes a shared memory segment from the system.
 * @param shmid The shared memory ID to be removed.
 */
void remove_shared_memory(int shmid);

/** @} */

/** @name Semaphore Utilities */
/** @{ */

/**
 * @brief Creates a new System V semaphore set.
 * @param nsems The number of semaphores in the set.
 * @return The semaphore set ID (semid).
 */
int create_semaphore_set(int nsems);

/**
 * @brief Performs a semaphore operation.
 * @param semid The semaphore set ID.
 * @param sem_num The index of the semaphore within the set.
 * @param op The operation to perform (e.g., -1 for wait/lock, +1 for
 * signal/unlock).
 * @param flags The operation flags (e.g., 0, SEM_UNDO, IPC_NOWAIT).
 */
void sem_op(int semid, int sem_num, int op, short flags);

/**
 * @brief Sets the value of a specific semaphore.
 * @param semid The semaphore set ID.
 * @param sem_num The index of the semaphore within the set.
 * @param val The value to set.
 */
void set_semaphore(int semid, int sem_num, int val);

/**
 * @brief Sets the values of all semaphores in a set simultaneously.
 *
 * This function performs a single System V IPC semctl() call with the
 * SETALL command to initialize all semaphores within the set to the
 * values provided in the input array.
 *
 * @param semid The semaphore set ID.
 * @param values Pointer to an array of unsigned short values (must contain
 *               at least as many elements as there are semaphores in the set).
 */
void set_all_semaphores(int semid, unsigned short *values);

/**
 * @brief Removes a semaphore set from the system.
 * @param semid The semaphore set ID to be removed.
 */
void remove_semaphores(int semid);

/** @} */

/** @name Message Queue Utilities */
/** @{ */

/**
 * @brief Creates a new System V message queue.
 * @return The message queue ID (mqid).
 */
int create_message_queue();

/**
 * @brief Sends a message to the specified message queue.
 * @param mqid The message queue ID.
 * @param msg Pointer to the message structure (must start with long mtype).
 * @param size The size of the message text (excluding the mtype field).
 * @param flags The operation flags (e.g., 0, IPC_NOWAIT).
 */
void send_message(int mqid, void *msg, size_t size, int flags);

/**
 * @brief Receives a message from the specified message queue.
 * @param mqid The message queue ID.
 * @param msg Pointer to the buffer where the message will be stored.
 * @param size The maximum size of the message text to receive.
 * @param mtype The type of message to receive (0 for first message, >0 for a
 * specific type).
 * @param flags The operation flags (e.g., 0, IPC_NOWAIT).
 */
int receive_message(int mqid, void *msg, size_t size, long mtype, int flags);

/**
 * @brief Removes a message queue from the system.
 * @param mqid The message queue ID to be removed.
 */
void remove_message_queue(int mqid);

/** @} */

#endif
