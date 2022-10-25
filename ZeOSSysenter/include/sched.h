/*
 * sched.h - Estructures i macros pel tractament de processos
 */

#ifndef __SCHED_H__
#define __SCHED_H__

#include <list.h>
#include <types.h>
#include <mm_address.h>
#include <stats.h>


#define NR_TASKS      10
#define NR_THREADS 20
#define NR_SEMS 20
#define KERNEL_STACK_SIZE	1024

enum state_t { ST_RUN, ST_READY, ST_BLOCKED };

struct task_struct {
  int PID;			/* Process ID. This MUST be the first field of the struct. */
  page_table_entry * dir_pages_baseAddr;
  struct list_head list;	/* Task struct enqueuing */
  //int register_esp;		/* position in the stack */
  enum state_t state;		/* State of the process */
  int total_quantum;		/* Total quantum of the process */
  struct stats p_stats;		/* Process stats */
  struct list_head ready;
  struct list_head blocked;
  int nthreads;
};

struct Pthread_t {
    int tid;
    struct list_head list;
    struct list_head list2;
    int register_esp;
    enum state_t state;
    int total_quantum;
    struct stats p_stats;
    struct task_struct *process;
    int exitstatus;
    struct list_head blocked;
    unsigned int pag_userstack;
};

struct sem_t {
    int sid;
    struct list_head list;
    int count;
    struct list_head blocked;
};

union thread_union {
  struct Pthread_t thread;
  unsigned long stack[KERNEL_STACK_SIZE];    /* pila de sistema, per procés */
};

/*union task_union {
  struct task_struct task;
  unsigned long stack[KERNEL_STACK_SIZE];    
};*/

extern struct task_struct protected_tasks[NR_TASKS+2];
extern struct task_struct *task; /* Vector de tasques */
extern union thread_union protected_threads[NR_THREADS+2];
extern union thread_union *threads;
extern struct task_struct *idle_task;
extern struct list_head freethreads;
extern struct sem_t sems[NR_SEMS];
extern struct list_head freesems;


#define KERNEL_ESP(t)       	(DWord) &(t)->stack[KERNEL_STACK_SIZE]

#define INITIAL_ESP       	KERNEL_ESP(&threads[1])

extern struct list_head freequeue;
extern struct list_head readyqueue;

/* Inicialitza les dades del proces inicial */
void init_task1(void);

void init_idle(void);

void init_sched(void);

void schedule(void);

struct Pthread_t * current();

void task_switch(union thread_union*t);
void switch_stack(int * save_sp, int new_sp);

void sched_next_rr(void);

void force_task_switch(void);

struct task_struct *list_head_to_task_struct(struct list_head *l);
struct Pthread_t* list_head_to_thread(struct list_head *l);
struct sem_t* list_head_to_sem(struct list_head *l);

int allocate_DIR(struct task_struct *t);

page_table_entry * get_PT (struct task_struct *t) ;

page_table_entry * get_DIR (struct task_struct *t) ;

/* Headers for the scheduling policy */
void sched_next_rr();
void sched_next_rr_thread(void);
void update_thread_state_rr(struct Pthread_t *t, struct list_head *dest);
void update_process_state_rr(struct task_struct *t, struct list_head *dest);
int needs_sched_rr();
void update_sched_data_rr();

void init_stats(struct stats *s);

#endif  /* __SCHED_H__ */
