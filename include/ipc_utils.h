#ifndef IPC_UTILS_H
#define IPC_UTILS_H

#include <stddef.h>

/**
 * @file ipc_utils.h
 * @brief Inter-Process Communication (IPC) utilities.
 *
 * This file provides wrapper functions for creating, managing,
 * and destroying System V IPC resources such as Shared Memory,
 * Semaphores, and Message Queues. All functions include
 * built-in error handling and will terminate the process on
 * unrecoverable system call failures unless otherwise documented.
 */

/** @name Shared Memory Utilities */
/** @{ */

/**
 * @brief Creates a new System V shared memory segment.
 *
 * This function calls shmget() with IPC_PRIVATE and permission 0666.
 * In case of failure to create the segment, it prints an error via perror()
 * and terminates the process with exit(EXIT_FAILURE).
 *
 * @param size The size of the shared memory segment in bytes.
 * @return The shared memory identifier (shmid) on success.
 */
int create_shared_memory(size_t size);

/**
 * @brief Attaches a shared memory segment to the process's address space.
 *
 * If the provided shmid is negative, it prints an invalid argument warning
 * to stderr and returns NULL.
 * In case shmat() fails, it prints an error via perror() and terminates
 * the process with exit(EXIT_FAILURE).
 *
 * @param shmid The shared memory ID.
 * @return A pointer to the attached shared memory on success, or NULL if shmid is invalid.
 */
void *attach_shared_memory(int shmid);

/**
 * @brief Detaches a shared memory segment from the process's address space.
 *
 * If the provided pointer is NULL, it prints an invalid argument warning
 * to stderr and returns immediately.
 * In case shmdt() fails, it prints an error via perror() and terminates
 * the process with exit(EXIT_FAILURE).
 *
 * @param ptr Pointer to the previously attached shared memory.
 */
void detach_shared_memory(void *ptr);

/**
 * @brief Removes a shared memory segment from the system.
 *
 * If the provided shmid is negative, it prints an invalid argument warning
 * to stderr and returns immediately.
 * In case shmctl(IPC_RMID) fails, it prints an error via perror() and terminates
 * the process with exit(EXIT_FAILURE).
 *
 * @param shmid The shared memory ID to be removed.
 */
void remove_shared_memory(int shmid);

/** @} */

/** @name Semaphore Utilities */
/** @{ */

/**
 * @brief Creates a new System V semaphore set.
 *
 * If nsems is negative, it prints an invalid argument warning to stderr
 * and returns -1.
 * In case semget() fails, it prints an error via perror() and terminates
 * the process with exit(EXIT_FAILURE).
 *
 * @param nsems The number of semaphores in the set.
 * @return The semaphore set ID (semid) on success, or -1 if nsems is invalid.
 */
int create_semaphore_set(int nsems);

/**
 * @brief Performs a semaphore operation (wait/signal).
 *
 * This function wraps semop(). It automatically handles EINTR (interrupted
 * system call) by retrying the operation. For any other unrecoverable failures,
 * it prints an error via perror() and terminates the process with exit(EXIT_FAILURE).
 *
 * @param semid The semaphore set ID.
 * @param sem_num The index of the semaphore within the set.
 * @param op The operation value (e.g., -1 to wait, +1 to signal).
 * @param flags Operation flags (e.g., SEM_UNDO, IPC_NOWAIT).
 */
void sem_op(int semid, int sem_num, int op, short flags);

/**
 * @brief Sets the value of a specific semaphore.
 *
 * This function uses semctl() with the SETVAL command.
 * In case of failure, it prints an error via perror() and terminates
 * the process with exit(EXIT_FAILURE).
 *
 * @param semid The semaphore set ID.
 * @param sem_num The index of the semaphore within the set.
 * @param val The value to assign to the semaphore.
 */
void set_semaphore(int semid, int sem_num, int val);

/**
 * @brief Sets the values of all semaphores in a set simultaneously.
 *
 * This function uses semctl() with the SETALL command.
 * In case of failure, it prints an error via perror() and terminates
 * the process with exit(EXIT_FAILURE).
 *
 * @param semid The semaphore set ID.
 * @param values Pointer to an array of unsigned short values containing the new values.
 */
void set_all_semaphores(int semid, unsigned short *values);

/**
 * @brief Removes a semaphore set from the system.
 *
 * If the provided semid is negative, it prints an invalid argument warning
 * to stderr and returns immediately.
 * In case semctl(IPC_RMID) fails, it prints an error via perror() and terminates
 * the process with exit(EXIT_FAILURE).
 *
 * @param semid The semaphore set ID to be removed.
 */
void remove_semaphores(int semid);

/** @} */

/** @name Message Queue Utilities */
/** @{ */

/**
 * @brief Creates a new System V message queue.
 *
 * This function calls msgget() with IPC_PRIVATE and permission 0666.
 * In case of failure, it prints an error via perror() and terminates
 * the process with exit(EXIT_FAILURE).
 *
 * @return The message queue ID (mqid) on success.
 */
int create_message_queue();

/**
 * @brief Sends a message to the specified message queue.
 *
 * If the call is interrupted (EINTR), the queue is removed (EIDRM), or the
 * parameters are invalid (EINVAL), the function catches the failure and
 * returns -1. For other errors, it prints an error via perror() and
 * terminates the process with exit(EXIT_FAILURE).
 *
 * @param mqid The message queue ID.
 * @param msg Pointer to the message structure (must start with a long type field).
 * @param size The size of the message payload (excluding the long type field).
 * @param flags Operation flags (e.g., IPC_NOWAIT).
 * @return 0 on success, or -1 on recoverable failure (EINTR, EIDRM, EINVAL).
 */
int send_message(int mqid, void *msg, size_t size, int flags);

/**
 * @brief Receives a message from the specified message queue.
 *
 * If the call is interrupted (EINTR), the queue is removed (EIDRM), or the
 * parameters are invalid (EINVAL), the function catches the failure and
 * returns -1. For other errors, it prints an error via perror() and
 * terminates the process with exit(EXIT_FAILURE).
 *
 * @param mqid The message queue ID.
 * @param msg Pointer to the buffer where the message will be stored.
 * @param size The maximum payload size to receive (excluding the long type field).
 * @param mtype The message type to receive (0 for first message, >0 for a specific type).
 * @param flags Operation flags (e.g., IPC_NOWAIT).
 * @return 0 on success, or -1 on recoverable failure (EINTR, EIDRM, EINVAL).
 */
int receive_message(int mqid, void *msg, size_t size, long mtype, int flags);

/**
 * @brief Removes a message queue from the system.
 *
 * In case msgctl(IPC_RMID) fails, it prints an error via perror() and
 * terminates the process with exit(EXIT_FAILURE).
 *
 * @param mqid The message queue ID to be removed.
 */
void remove_message_queue(int mqid);

/** @} */

#endif
