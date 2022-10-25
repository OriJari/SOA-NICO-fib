/*
 * sched.c - initializes struct for task 0 anda task 1
 */

#include <types.h>
#include <hardware.h>
#include <segment.h>
#include <sched.h>
#include <mm.h>
#include <io.h>
#include <utils.h>
#include <p_stats.h>

/**
 * Container for the Task array and 2 additional pages (the first and the last one)
 * to protect against out of bound accesses.
 */
struct task_struct protected_tasks[NR_TASKS+2]
  __attribute__((__section__(".data.task")));

struct task_struct *task = &protected_tasks[1]; /* == union task_union task[NR_TASKS] */

union thread_union protected_threads[NR_THREADS+2]
  __attribute__((__section__(".data.threads")));

union thread_union *threads = &protected_threads[1];

#if 0
struct task_struct *list_head_to_task_struct(struct list_head *l)
{
  return list_entry( l, struct task_struct, list);
}
#endif

extern struct list_head blocked;

// Free task structs
struct list_head freequeue;
// Ready queue
struct list_head readyqueue;

struct list_head freethreads;

struct sem_t sems[NR_SEMS];
struct list_head freesems;

void init_stats(struct stats *s)
{
	s->user_ticks = 0;
	s->system_ticks = 0;
	s->blocked_ticks = 0;
	s->ready_ticks = 0;
	s->elapsed_total_ticks = get_ticks();
	s->total_trans = 0;
	s->remaining_ticks = get_ticks();
}

/* get_DIR - Returns the Page Directory address for task 't' */
page_table_entry * get_DIR (struct task_struct *t) 
{
	return t->dir_pages_baseAddr;
}

/* get_PT - Returns the Page Table address for task 't' */
page_table_entry * get_PT (struct task_struct *t) 
{
	return (page_table_entry *)(((unsigned int)(t->dir_pages_baseAddr->bits.pbase_addr))<<12);
}


int allocate_DIR(struct task_struct *t) 
{
	int pos;

	pos = ((int)t-(int)task)/sizeof(struct task_struct);

	t->dir_pages_baseAddr = (page_table_entry*) &dir_pages[pos]; 

	return 1;
}

void cpu_idle(void)
{
	__asm__ __volatile__("sti": : :"memory");

	while(1)
	{
	;
	}
}

#define DEFAULT_QUANTUM 10

int remaining_quantum=0;
int remaining_quantum_thread=0;

int get_quantum(struct task_struct *t)
{
  return t->total_quantum;
}

int get_quantum_thread(struct Pthread_t *t)
{
  return t->total_quantum;
}

void set_quantum(struct task_struct *t, int new_quantum)
{
  t->total_quantum=new_quantum;
}

void set_quantum_thread(struct Pthread_t *t, int new_quantum)
{
  t->total_quantum=new_quantum;
}

struct task_struct *idle_task=NULL;

void update_sched_data_rr(void)
{
  remaining_quantum--;
  remaining_quantum_thread--;
}

int needs_sched_rr(void)
{
  struct task_struct *p = current()->process;
  if ((remaining_quantum==0)&&(!list_empty(&readyqueue))) return 1;
  if (remaining_quantum==0) remaining_quantum=get_quantum(p);
  if ((remaining_quantum_thread==0)&&(!list_empty(&(p->ready)))) return 2;
  if (remaining_quantum_thread==0) remaining_quantum_thread=get_quantum_thread(current());
  return 0;
}

void update_process_state_rr(struct task_struct *t, struct list_head *dst_queue)
{
  if (t->state!=ST_RUN) list_del(&(t->list));
  if (dst_queue!=NULL)
  {
    list_add_tail(&(t->list), dst_queue);
    if (dst_queue!=&readyqueue) t->state=ST_BLOCKED;
    else
    {
      update_stats(&(t->p_stats.system_ticks), &(t->p_stats.elapsed_total_ticks));
      t->state=ST_READY;
    }
  }
  else t->state=ST_RUN;
}

void update_thread_state_rr(struct Pthread_t *t, struct list_head *dst_queue)
{
  struct task_struct *p = t->process;
  if (t->state!=ST_RUN) {
      list_del(&(t->list));
      if (t->state==ST_BLOCKED) list_del(&(t->list2));
  }
  if (dst_queue!=NULL)
  {
    list_add_tail(&(t->list), dst_queue);
    if (dst_queue!=&(p->ready)) {
        t->state=ST_BLOCKED;
        list_add_tail(&(t->list2), &(p->blocked));
        if (list_empty(&(p->ready))) update_process_state_rr(p,&blocked);
    }
    else
    {
      update_stats(&(t->p_stats.system_ticks), &(t->p_stats.elapsed_total_ticks));
      t->state=ST_READY;
      if (p->state == ST_BLOCKED) update_process_state_rr(p,&readyqueue);
    }
  }
  else t->state=ST_RUN;
}

void sched_next_rr(void)
{
  struct list_head *e;
  struct task_struct *t;

  if (!list_empty(&readyqueue)) {
	e = list_first(&readyqueue);
    list_del(e);

    t=list_head_to_task_struct(e);
  }
  else
    t=idle_task;

  t->state=ST_RUN;
  struct list_head *e2;
  e2 = list_first(&(t->ready));
  list_del(e2);
  struct Pthread_t *t2;
  t2 = list_head_to_thread(e2);
  t2->state=ST_RUN;
  remaining_quantum=get_quantum(t);
  remaining_quantum_thread=get_quantum_thread(t2);

  update_stats(&(current()->p_stats.system_ticks), &(current()->p_stats.elapsed_total_ticks));
  update_stats(&((current()->process)->p_stats.system_ticks), &((current()->process)->p_stats.elapsed_total_ticks));
  update_stats(&(t->p_stats.ready_ticks), &(t->p_stats.elapsed_total_ticks));
  t->p_stats.total_trans++;
  update_stats(&(t2->p_stats.ready_ticks), &(t2->p_stats.elapsed_total_ticks));
  t2->p_stats.total_trans++;

  task_switch((union thread_union*)t2);
}

void sched_next_rr_thread(void)
{
  struct task_struct *t;

  t=current()->process;

  struct list_head *e2;
  e2 = list_first(&(t->ready));
  list_del(e2);
  struct Pthread_t *t2;
  t2 = list_head_to_thread(e2);
  t2->state=ST_RUN;
  remaining_quantum_thread=get_quantum_thread(t2);

  update_stats(&(current()->p_stats.system_ticks), &(current()->p_stats.elapsed_total_ticks));
  update_stats(&(t->p_stats.system_ticks), &(t->p_stats.elapsed_total_ticks));
  update_stats(&(t2->p_stats.ready_ticks), &(t2->p_stats.elapsed_total_ticks));
  t2->p_stats.total_trans++;

  task_switch((union thread_union*)t2);
}

void schedule()
{
  update_sched_data_rr();
  int needs_sched = needs_sched_rr();
  struct task_struct *p = current()->process;
  if (needs_sched==1)
  {
    update_thread_state_rr(current(),&(p->ready));
    update_process_state_rr(p, &readyqueue);
    sched_next_rr();
  }
  else if (needs_sched==2)
  {
    update_thread_state_rr(current(),&(p->ready));
    sched_next_rr_thread();
  }
}

void init_idle (void)
{
  struct list_head *l = list_first(&freequeue);
  list_del(l);
  struct task_struct *c = list_head_to_task_struct(l);
  
  struct list_head *l2 = list_first(&freethreads);
  list_del(l2);
  struct Pthread_t *c2 = list_head_to_thread(l2);
  union thread_union *uc = (union thread_union*)c2;

  c->PID=0;

  c->total_quantum=DEFAULT_QUANTUM;
  
  INIT_LIST_HEAD(&(c->ready));
  INIT_LIST_HEAD(&(c->blocked));
  
  list_add_tail(&(c2->list), &(c->ready));
  
  c2->tid=0;
  c2->total_quantum=DEFAULT_QUANTUM;
  c2->process=c;
  c2->exitstatus=-1;
  INIT_LIST_HEAD(&(c2->blocked));

  init_stats(&c->p_stats);
  init_stats(&c2->p_stats);

  allocate_DIR(c);

  uc->stack[KERNEL_STACK_SIZE-1]=(unsigned long)&cpu_idle; /* Return address */
  uc->stack[KERNEL_STACK_SIZE-2]=0; /* register ebp */

  c2->register_esp=(int)&(uc->stack[KERNEL_STACK_SIZE-2]); /* top of the stack */
  
  c->nthreads = 1;

  idle_task=c;
}

void setMSR(unsigned long msr_number, unsigned long high, unsigned long low);

void init_task1(void)
{
  struct list_head *l = list_first(&freequeue);
  list_del(l);
  struct task_struct *c = list_head_to_task_struct(l);
  
  struct list_head *l2 = list_first(&freethreads);
  list_del(l2);
  struct Pthread_t *c2 = list_head_to_thread(l2);
  union thread_union *uc = (union thread_union*)c2;

  c->PID=1;

  c->total_quantum=DEFAULT_QUANTUM*2;

  c->state=ST_RUN;
  
  INIT_LIST_HEAD(&(c->ready));
  INIT_LIST_HEAD(&(c->blocked));
  
  c2->tid=1;
  c2->total_quantum=DEFAULT_QUANTUM;
  c2->state=ST_RUN;
  c2->process=c;
  c2->exitstatus=-1;
  INIT_LIST_HEAD(&(c2->blocked));

  remaining_quantum=c->total_quantum;
  remaining_quantum_thread=c2->total_quantum;

  init_stats(&c->p_stats);
  init_stats(&c2->p_stats);

  allocate_DIR(c);

  set_user_pages(c);
  
  c2->pag_userstack = get_frame(get_PT(c), PAG_LOG_INIT_DATA+NUM_PAG_DATA-1);
  
  c->nthreads = 1;

  tss.esp0=(DWord)&(uc->stack[KERNEL_STACK_SIZE]);
  setMSR(0x175, 0, (unsigned long)&(uc->stack[KERNEL_STACK_SIZE]));

  set_cr3(c->dir_pages_baseAddr);
  
}

void init_freequeue()
{
  int i;

  INIT_LIST_HEAD(&freequeue);

  /* Insert all task structs in the freequeue */
  for (i=0; i<NR_TASKS; i++)
  {
    task[i].PID=-1;
    list_add_tail(&(task[i].list), &freequeue);
  }
}

void init_freethreads()
{
  int i;

  INIT_LIST_HEAD(&freethreads);

  /* Insert all task structs in the freequeue */
  for (i=0; i<NR_THREADS; i++)
  {
    threads[i].thread.tid=-1;
    threads[i].thread.exitstatus=0;
    list_add_tail(&(threads[i].thread.list), &freethreads);
  }
}

void init_freesems()
{
  int i;

  INIT_LIST_HEAD(&freesems);

  /* Insert all task structs in the freequeue */
  for (i=0; i<NR_SEMS; i++)
  {
    sems[i].sid=-1;
    list_add_tail(&(sems[i].list), &freesems);
  }
}

void init_sched()
{
  init_freequeue();
  init_freethreads();
  init_freesems();
  INIT_LIST_HEAD(&readyqueue);
  INIT_LIST_HEAD(&blocked);
}

struct Pthread_t* current()
{
  int ret_value;
  
  return (struct Pthread_t*)( ((unsigned int)&ret_value) & 0xfffff000);
}

struct task_struct* list_head_to_task_struct(struct list_head *l)
{
  return (struct task_struct*)((int)l - sizeof(page_table_entry) - sizeof(int));
}

struct Pthread_t* list_head_to_thread(struct list_head *l)
{
  return (struct Pthread_t*)((int)l&0xfffff000);
}

struct sem_t* list_head_to_sem(struct list_head *l)
{
  return (struct sem_t*)((int)l - sizeof(int));
}

/* Do the magic of a task switch */
void inner_task_switch(union thread_union *new)
{
  //pq aqui state de new es ST_READY si ya lo hemos puesto antes en ST_RUN???
  new->thread.state = ST_RUN; //necesitamos hacerlo otra vez
  
  page_table_entry *new_DIR = get_DIR(new->thread.process);

  /* Update TSS and MSR to make it point to the new stack */
  tss.esp0=(int)&(new->stack[KERNEL_STACK_SIZE]);
  setMSR(0x175, 0, (unsigned long)&(new->stack[KERNEL_STACK_SIZE]));

  /* TLB flush. New address space */
  set_ss_pag(get_PT(new->thread.process),PAG_LOG_INIT_DATA+NUM_PAG_DATA-1,new->thread.pag_userstack);
  set_cr3(new_DIR);

  switch_stack(&current()->register_esp, new->thread.register_esp);
}


/* Force a task switch assuming that the scheduler does not work with priorities */
void force_task_switch()
{
  struct task_struct *p = current()->process;
  update_thread_state_rr(current(),&(p->ready));
  update_process_state_rr(p, &readyqueue);
  sched_next_rr();
}
