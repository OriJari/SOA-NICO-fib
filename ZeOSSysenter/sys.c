/*
 * sys.c - Syscalls implementation
 */
#include <devices.h>

#include <utils.h>

#include <io.h>

#include <mm.h>

#include <mm_address.h>

#include <sched.h>

#include <p_stats.h>

#include <errno.h>

#define LECTURA 0
#define ESCRIPTURA 1

void * get_ebp();

int check_fd(int fd, int permissions)
{
  if (fd!=1) return -EBADF; 
  if (permissions!=ESCRIPTURA) return -EACCES; 
  return 0;
}

void user_to_system(void)
{
  update_stats(&(current()->p_stats.user_ticks), &(current()->p_stats.elapsed_total_ticks));
  //update_stats(&((current()->process)->p_stats.user_ticks), &(current()->p_stats.elapsed_total_ticks));
}

void system_to_user(void)
{
  update_stats(&(current()->p_stats.system_ticks), &(current()->p_stats.elapsed_total_ticks));
  //update_stats(&((current()->process)->p_stats.system_ticks), &(current()->p_stats.elapsed_total_ticks));
}

int sys_ni_syscall()
{
	return -ENOSYS; 
}

int sys_getpid()
{
	return (current()->process)->PID;
}

int sys_gettid()
{
	return current()->tid;
}

int global_PID=1000;
int global_SID=1;

int sys_getthread(struct Pthread_t **th, int tid) {
    if (!access_ok(VERIFY_WRITE, th, sizeof(struct Pthread_t*))) return -EFAULT;
    for (int i = 0; i < NR_THREADS; ++i) {
        if (threads[i].thread.tid == tid) {
            struct Pthread_t* tmp[1];
            tmp[0] = (struct Pthread_t*)&threads[i];
            copy_to_user(tmp,th,sizeof(struct Pthread_t*));
            return 0;
        }
    }
    return -1;
}

int sys_getsem(struct sem_t **sem, int sid) {
    if (!access_ok(VERIFY_WRITE, sem, sizeof(struct sem_t*))) return -EFAULT;
    for (int i = 0; i < NR_SEMS; ++i) {
        if (sems[i].sid == sid) {
            struct sem_t* tmp[1];
            tmp[0] = &sems[i];
            copy_to_user(tmp,sem,sizeof(struct sem_t*));
            return 0;
        }
    }
    return -1;
}

int sys_Sem_init(struct sem_t *sem, int n) {
    if (list_empty(&freesems)) return -EAGAIN;
    if (!access_ok(VERIFY_WRITE, sem, sizeof(struct sem_t))) return -EFAULT;
    struct list_head *lh = list_first(&freesems);
    list_del(lh);
    struct sem_t *sem2 = list_head_to_sem(lh);
    sem2->sid=global_SID++;
    sem2->count = n;
    INIT_LIST_HEAD(&(sem2->blocked));
    copy_to_user(sem2,sem,sizeof(struct sem_t));
    return sem2->sid;
}

int sys_Sem_wait(struct sem_t *sem) {
    if (sem->sid < 1) return -EINVAL;
    sem->count--;
    if (sem->count < 0) {
        struct task_struct *currentp = current()->process;
        update_thread_state_rr(current(), &(sem->blocked));
        if (list_empty(&(currentp->ready))) {
            sched_next_rr();
        } else {
            sched_next_rr_thread();
        }
    }
    return 0;
}

int sys_Sem_post(struct sem_t *sem) {
    if (sem->sid < 1) return -EINVAL;
    sem->count++;
    if (sem->count <= 0 && !list_empty(&(sem->blocked))) {
        struct list_head *lh;
        lh = list_first(&(sem->blocked));
        struct Pthread_t *th;
        th = list_head_to_thread(lh);
        update_thread_state_rr(th, &((th->process)->ready));
    }
    return 0;
}

int sys_Sem_destroy(struct sem_t *sem) {
    if (!list_empty(&(sem->blocked))) return -EBUSY;
    sem->sid = -1;
    list_add_tail(&(sem->list),&freesems);
    return 0;
}

int ret_from_fork()
{
  return 0;
}

int sys_fork(void)
{
  struct task_struct *currentp = current()->process;
  struct list_head *lhcurrent = NULL;
  struct list_head *lhcurrent2 = NULL;
  struct task_struct *uchildtask;
  union thread_union *uchild;
  
  /* Any free task_struct? */
  if (list_empty(&freequeue)) return -ENOMEM;
  if (list_empty(&freethreads)) return -ENOMEM;

  lhcurrent=list_first(&freequeue);
  
  list_del(lhcurrent);
  
  uchildtask = list_head_to_task_struct(lhcurrent);
  
  lhcurrent2=list_first(&freethreads);
  
  list_del(lhcurrent2);
  
  uchild=(union thread_union*)list_head_to_thread(lhcurrent2);
  
  /* Copy the parent's task struct to child's */
  copy_data(currentp, uchildtask, sizeof(struct task_struct));
  copy_data(current(), uchild, sizeof(union thread_union));
  
  /* new pages dir */
  allocate_DIR(uchildtask);
  
  /* Allocate pages for DATA+STACK */
  int new_ph_pag, pag, i;
  page_table_entry *process_PT = get_PT(uchildtask);
  for (pag=0; pag<NUM_PAG_DATA; pag++)
  {
    new_ph_pag=alloc_frame();
    if (new_ph_pag!=-1) /* One page allocated */
    {
      set_ss_pag(process_PT, PAG_LOG_INIT_DATA+pag, new_ph_pag);
    }
    else /* No more free pages left. Deallocate everything */
    {
      /* Deallocate allocated pages. Up to pag. */
      for (i=0; i<pag; i++)
      {
        free_frame(get_frame(process_PT, PAG_LOG_INIT_DATA+i));
        del_ss_pag(process_PT, PAG_LOG_INIT_DATA+i);
      }
      /* Deallocate task_struct */
      list_add_tail(lhcurrent, &freequeue);
      list_add_tail(lhcurrent2, &freethreads);
      
      /* Return error */
      return -EAGAIN; 
    }
  }

  /* Copy parent's SYSTEM and CODE to child. */
  page_table_entry *parent_PT = get_PT(currentp);
  for (pag=0; pag<NUM_PAG_KERNEL; pag++)
  {
    set_ss_pag(process_PT, pag, get_frame(parent_PT, pag));
  }
  for (pag=0; pag<NUM_PAG_CODE; pag++)
  {
    set_ss_pag(process_PT, PAG_LOG_INIT_CODE+pag, get_frame(parent_PT, PAG_LOG_INIT_CODE+pag));
  }
  /* Copy parent's DATA to child. We will use TOTAL_PAGES-1 as a temp logical page to map to */
  for (pag=NUM_PAG_KERNEL+NUM_PAG_CODE; pag<NUM_PAG_KERNEL+NUM_PAG_CODE+NUM_PAG_DATA; pag++)
  {
    /* Map one child page to parent's address space. */
    set_ss_pag(parent_PT, pag+NUM_PAG_DATA, get_frame(process_PT, pag));
    copy_data((void*)(pag<<12), (void*)((pag+NUM_PAG_DATA)<<12), PAGE_SIZE);
    del_ss_pag(parent_PT, pag+NUM_PAG_DATA);
  }
  /* Deny access to the child's memory space */
  set_cr3(get_DIR(currentp));

  uchildtask->PID=++global_PID;
  uchildtask->state=ST_READY;
  uchildtask->nthreads = 1;
  uchild->thread.tid=global_PID;
  uchild->thread.state=ST_READY;
  uchild->thread.pag_userstack=get_frame(process_PT,PAG_LOG_INIT_DATA+NUM_PAG_DATA-1);

  int register_ebp;		/* frame pointer */
  /* Map Parent's ebp to child's stack */
  register_ebp = (int) get_ebp();
  register_ebp=(register_ebp - (int)current()) + (int)(uchild);

  uchild->thread.register_esp=register_ebp + sizeof(DWord);

  DWord temp_ebp=*(DWord*)register_ebp;
  /* Prepare child stack for context switch */
  uchild->thread.register_esp-=sizeof(DWord);
  *(DWord*)(uchild->thread.register_esp)=(DWord)&ret_from_fork;
  uchild->thread.register_esp-=sizeof(DWord);
  *(DWord*)(uchild->thread.register_esp)=temp_ebp;

  /* Set stats to 0 */
  init_stats(&(uchild->thread.p_stats));
  init_stats(&(uchildtask->p_stats));
  
  uchild->thread.exitstatus = -1;
  uchild->thread.process = uchildtask;
  INIT_LIST_HEAD(&(uchild->thread.blocked));
  INIT_LIST_HEAD(&(uchildtask->ready));
  INIT_LIST_HEAD(&(uchildtask->blocked));

  /* Queue child process into readyqueue */
  uchildtask->state=ST_READY;
  list_add_tail(&(uchildtask->list), &readyqueue);
  uchild->thread.state=ST_READY;
  list_add_tail(&(uchild->thread.list), &(uchildtask->ready));
  
  return uchildtask->PID;
}

#define TAM_BUFFER 512

int sys_write(int fd, char *buffer, int nbytes) {
char localbuffer [TAM_BUFFER];
int bytes_left;
int ret;

	if ((ret = check_fd(fd, ESCRIPTURA)))
		return ret;
	if (nbytes < 0)
		return -EINVAL;
	if (!access_ok(VERIFY_READ, buffer, nbytes))
		return -EFAULT;
	
	bytes_left = nbytes;
	while (bytes_left > TAM_BUFFER) {
		copy_from_user(buffer, localbuffer, TAM_BUFFER);
		ret = sys_write_console(localbuffer, TAM_BUFFER);
		bytes_left-=ret;
		buffer+=ret;
	}
	if (bytes_left > 0) {
		copy_from_user(buffer, localbuffer,bytes_left);
		ret = sys_write_console(localbuffer, bytes_left);
		bytes_left-=ret;
	}
	return (nbytes-bytes_left);
}


extern int zeos_ticks;

int sys_gettime()
{
  return zeos_ticks;
}

void sys_exit()
{  
  int i;
  struct task_struct *currentp = current()->process;

  page_table_entry *process_PT = get_PT(currentp);

  // Deallocate all the propietary physical pages
  for (i=0; i<NUM_PAG_DATA; i++)
  {
    free_frame(get_frame(process_PT, PAG_LOG_INIT_DATA+i));
    del_ss_pag(process_PT, PAG_LOG_INIT_DATA+i);
  }
  
  /* Free task_struct */
  list_add_tail(&(currentp->list), &freequeue);
  
  currentp->PID=-1;
  
  if (currentp->nthreads > 0) {
      current()->exitstatus = 0;
      list_add_tail(&(current()->list), &freethreads);
      current()->tid=-1;
      while (!list_empty(&(current()->blocked))) {
          struct list_head* lh;
          lh = list_first(&(current()->blocked));
          struct Pthread_t* tmp;
          tmp = list_head_to_thread(lh);
          update_thread_state_rr(tmp,&((tmp->process)->ready));
      }
      currentp->nthreads -= 1;
      while (!list_empty(&(currentp->ready))) {
          struct list_head* l;
          l = list_first(&(currentp->ready));
          list_del(l);
          struct Pthread_t* th;
          th = list_head_to_thread(l);
          th->exitstatus = 0;
          list_add_tail(&(th->list), &freethreads);
          th->tid=-1;
          free_frame(th->pag_userstack);
          while (!list_empty(&(th->blocked))) {
              struct list_head* lh;
              lh = list_first(&(th->blocked));
              struct Pthread_t* tmp;
              tmp = list_head_to_thread(lh);
              update_thread_state_rr(tmp,&((tmp->process)->ready));
          }
          currentp->nthreads -= 1;
      }
      while (!list_empty(&(currentp->blocked))) {
          struct list_head* l;
          l = list_first(&(currentp->blocked));
          list_del(l);
          struct Pthread_t* th;
          th = list_head_to_thread(l);
          th->exitstatus = 0;
          list_del(&(th->list));
          list_add_tail(&(th->list), &freethreads);
          th->tid=-1;
          free_frame(th->pag_userstack);
          while (!list_empty(&(th->blocked))) {
              struct list_head* lh;
              lh = list_first(&(th->blocked));
              struct Pthread_t* tmp;
              tmp = list_head_to_thread(lh);
              update_thread_state_rr(tmp,&((tmp->process)->ready));
          }
          currentp->nthreads -= 1;
      }
  }
  
  /* Restarts execution of the next process */
  sched_next_rr();
}

/* System call to force a task switch */
int sys_yield()
{
  force_task_switch();
  return 0;
}

extern int remaining_quantum;

int sys_get_stats(int pid, struct stats *st)
{
  int i;
  
  if (!access_ok(VERIFY_WRITE, st, sizeof(struct stats))) return -EFAULT; 
  
  if (pid<0) return -EINVAL;
  for (i=0; i<NR_TASKS; i++)
  {
    if (task[i].PID==pid)
    {
      task[i].p_stats.remaining_ticks=remaining_quantum;
      copy_to_user(&(task[i].p_stats), st, sizeof(struct stats));
      return 0;
    }
  }
  return -ESRCH; /*ESRCH */
}

int sys_get_stats_t(int tid, struct stats *st)
{
  int i;
  
  if (!access_ok(VERIFY_WRITE, st, sizeof(struct stats))) return -EFAULT; 
  
  if (tid<0) return -EINVAL;
  for (i=0; i<NR_THREADS; i++)
  {
    if (threads[i].thread.tid==tid)
    {
      threads[i].thread.p_stats.remaining_ticks=remaining_quantum;
      copy_to_user(&(threads[i].thread.p_stats), st, sizeof(struct stats));
      return 0;
    }
  }
  return -ESRCH; /*ESRCH */
}



int sys_Pthread_create(struct Pthread_t *tid, void *(*start_routine) (void *), void *arg) {
    if (list_empty(&freethreads)) return -ENOMEM;
    if (!access_ok(VERIFY_WRITE, tid, sizeof(struct Pthread_t))) return -EFAULT;
    struct list_head *lhcurrent = NULL;
    lhcurrent=list_first(&freethreads);
    list_del(lhcurrent);
    union thread_union *uchild;
    uchild=(union thread_union*)list_head_to_thread(lhcurrent);
    copy_data(current(), uchild, sizeof(union thread_union));
    struct task_struct *currentp = current()->process;
    
    int new_ph_pag=alloc_frame();
    if (new_ph_pag!=-1) /* One page allocated */
    {
      set_ss_pag(get_PT(currentp), PAG_LOG_INIT_DATA+NUM_PAG_DATA-1, new_ph_pag);
    }
    else /* No more free pages left. Deallocate everything */
    {
      /* Deallocate allocated pages. Up to pag. */
      free_frame(new_ph_pag);
      /* Deallocate task_struct */
      list_add_tail(lhcurrent, &freethreads);
      
      /* Return error */
      return -EAGAIN; 
    }
    set_cr3(get_DIR(currentp));
    DWord* userstack = (DWord*)(((PAG_LOG_INIT_DATA+NUM_PAG_DATA)<<12) - 3*sizeof(DWord));
    userstack[0] = 0;
    userstack[1] = (DWord)arg;
    set_ss_pag(get_PT(currentp), PAG_LOG_INIT_DATA+NUM_PAG_DATA-1, current()->pag_userstack);
    set_cr3(get_DIR(currentp));
    
    uchild->thread.tid = ++global_PID;
    uchild->thread.state = ST_READY;
    uchild->thread.pag_userstack = new_ph_pag;
    int register_ebp;		/* frame pointer */
    /* Map Parent's ebp to child's stack */
    register_ebp = (int) get_ebp();
    register_ebp=(register_ebp - (int)current()) + (int)(uchild);

    uchild->thread.register_esp=register_ebp;
    
    register_ebp += 13*sizeof(DWord);
    *(DWord*)(register_ebp) = (DWord)start_routine;
    register_ebp += 3*sizeof(DWord);
    *(DWord*)(register_ebp) = (DWord)userstack;
    
    init_stats(&(uchild->thread.p_stats));
    uchild->thread.exitstatus = -1;
    uchild->thread.process = currentp;
    INIT_LIST_HEAD(&(uchild->thread.blocked));
    list_add_tail(&(uchild->thread.list), &(currentp->ready));
    
	    currentp->nthreads += 1;
    
    copy_to_user(uchild,tid,sizeof(struct Pthread_t));
    return uchild->thread.tid;
    
}

int sys_Pthread_join(struct Pthread_t *tid, void **status) {
    if (!access_ok(VERIFY_WRITE, *status, sizeof(int))) return -EFAULT;
    if (tid->exitstatus >= 0) {
        copy_to_user(&(tid->exitstatus),*status,sizeof(int));
        return 0;
    }
    struct task_struct *currentp = current()->process;
    update_thread_state_rr(current(), &(tid->blocked));
    if (list_empty(&(currentp->ready))) {
        sched_next_rr();
    } else {
        sched_next_rr_thread();
    }
    copy_to_user(&(tid->exitstatus),*status,sizeof(int));
    return 0;
}

void sys_Pthread_exit(void *status) {
    struct task_struct *currentp = current()->process;
    currentp->nthreads -= 1;
    current()->exitstatus = 0;
    if (access_ok(VERIFY_WRITE, status, sizeof(int))) {
        copy_to_user(&(current()->exitstatus),status,sizeof(int));
    }
    list_add_tail(&(current()->list), &freethreads);
    current()->tid=-1;
    free_frame(current()->pag_userstack);
    while (!list_empty(&(current()->blocked))) {
        struct list_head* lh;
        lh = list_first(&(current()->blocked));
        struct Pthread_t* tmp;
        tmp = list_head_to_thread(lh);
        update_thread_state_rr(tmp,&((tmp->process)->ready));
    }
    if (currentp->nthreads == 0) {
        sys_exit();
    } else {
        if (list_empty(&(currentp->ready))) {
            sched_next_rr();
        } else {
            sched_next_rr_thread();
        }
    }
}
