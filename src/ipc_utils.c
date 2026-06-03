#include "ipc_utils.h"
#include <asm-generic/errno-base.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>

int create_shared_memory(size_t size) {
  int shm_id = shmget(IPC_PRIVATE, size, (IPC_CREAT | IPC_EXCL) | 0666);
  if (shm_id == -1) {
    perror("ERROR CREATING SHM");
    exit(EXIT_FAILURE);
  }

  return shm_id;
}

void *attach_shared_memory(int shmid) {
  if (shmid < 0) {
    fprintf(stderr, "INVALID ARGUMENT FOR %s\n", __func__);
    return NULL;
  }

  void *shm = shmat(shmid, NULL, 0);
  if (shm == (void *)-1) {
    perror("ERROR ATTACHING SHM");
    exit(EXIT_FAILURE);
  }

  return shm;
}

void detach_shared_memory(void *ptr) {
  if (ptr == NULL) {
    fprintf(stderr, "INVALID ARGUMENT FOR %s\n", __func__);
    return;
  }

  if (shmdt(ptr) == -1) {
    perror("ERRORE DETACHING SHM");
    exit(EXIT_FAILURE);
  }
}

void remove_shared_memory(int shmid) {
  if (shmid < 0) {
    fprintf(stderr, "INVALID ARGUMENT FOR %s\n", __func__);
    return;
  }

  if (shmctl(shmid, IPC_RMID, NULL) == -1) {
    perror("ERRORE DELETING SHM");
    exit(EXIT_FAILURE);
  }
}

int create_semaphore_set(int nsems) {
  if (nsems < 0) {
    fprintf(stderr, "INVALID ARGUMENT FOR %s\n", __func__);
    return -1;
  }

  int sem_id = semget(IPC_PRIVATE, nsems, (IPC_CREAT | IPC_EXCL) | 0666);
  if (sem_id == -1) {
    perror("ERROR CREATING SEMAPHORES");
    exit(EXIT_FAILURE);
  }

  return sem_id;
}

#if defined(__GNU_LIBRARY__) && !defined(_SEM_SEMUN_UNDEFINED)
#else
union semun {
  int val;               /* value for SETVAL */
  struct semid_ds *buf;  /* buffer for IPC_STAT, IPC_SET */
  unsigned short *array; /* array for GETALL, SETALL */
  struct seminfo *__buf; /* buffer for IPC_INFO */
};
#endif

void set_semaphore(int semid, int sem_num, int val) {
  union semun arg;
  arg.val = val;
  if (semctl(semid, sem_num, SETVAL, arg) == -1) {
    perror("ERROR SETTING SEMAPHORE VALUE");
    exit(EXIT_FAILURE);
  }
}

void set_all_semaphores(int semid, unsigned short *values) {
  union semun arg;
  arg.array = values;
  if (semctl(semid, 0, SETALL, arg) == -1) {
    perror("ERROR SETALL");
    exit(EXIT_FAILURE);
  }
}

void sem_op(int semid, int sem_num, int op, short flags) {
  struct sembuf sb = {sem_num, op, flags};
  while (semop(semid, &sb, 1) == -1) {
    if (errno == EINTR) {
      continue;
    }
    perror("ERROR SEMOP");
    exit(EXIT_FAILURE);
  }
}

void remove_semaphores(int semid) {
  if (semid < 0) {
    fprintf(stderr, "INVALID ARGUMENT FOR %s\n", __func__);
    return;
  }

  if (semctl(semid, 0, IPC_RMID)) {
    perror("ERROR REMOVING SEMAPHORE");
    exit(EXIT_FAILURE);
  }

  return;
}

int create_message_queue() {
  int msg_id = msgget(IPC_PRIVATE, (IPC_CREAT | IPC_EXCL) | 0666);
  if (msg_id == -1) {
    perror("ERROR CREATING MSG QUEUE");
    exit(EXIT_FAILURE);
  }

  return msg_id;
}

void send_message(int mqid, void *msg, size_t size, int flags) {
  if (msgsnd(mqid, msg, size, flags) == -1) {
    perror("ERROR SENDING MESSAGE");
    exit(EXIT_FAILURE);
  }
}

int receive_message(int mqid, void *msg, size_t size, long mtype, int flags) {
  if (msgrcv(mqid, msg, size, mtype, flags) == -1) {
    if (errno == EINTR) {
      return -1;
    }
    perror("ERROR RECIVING MESSAGE");
    exit(EXIT_FAILURE);
  }
  return 0;
}

void remove_message_queue(int mqid) {

  if (msgctl(mqid, IPC_RMID, NULL) == -1) {
    perror("ERROR DELETING MSGQ");
    exit(EXIT_FAILURE);
  }
}