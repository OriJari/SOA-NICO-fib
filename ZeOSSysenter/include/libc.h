/*
 * libc.h - macros per fer els traps amb diferents arguments
 *          definició de les crides a sistema
 */
 
#ifndef __LIBC_H__
#define __LIBC_H__

#include <stats.h>
#include <sched.h>

extern int errno;

int write(int fd, char *buffer, int size);

int Pthread_create(struct Pthread_t *tid, void *(*start_routine) (void *), void *arg);
int Pthread_join(struct Pthread_t *tid, void **status);
void Pthread_exit(void *status);

int Sem_init(struct sem_t *sem, int n);
int Sem_wait(struct sem_t *sem);
int Sem_post(struct sem_t *sem);
int Sem_destroy(struct sem_t *sem);

void itoa(int a, char *b);

int strlen(char *a);

void perror();

int getpid();
int gettid();
int getthread(struct Pthread_t **th, int tid);
int getsem(struct sem_t **sem, int sid);

int gettime();

int fork();

void exit();

int yield();

int get_stats(int pid, struct stats *st);
int get_stats_t(int pid, struct stats *st);

#endif  /* __LIBC_H__ */
