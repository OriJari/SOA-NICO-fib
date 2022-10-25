#include <libc.h>
#include <sched.h>

char buff[24];

int pid;

struct sem_t *sem[1];

void threadtask1() {
    int i = 0;
    while (gettime() < 2000) {
    int j = gettime();
    if (j > i) i = j;
    if (i%50 == 0) {
    write(1,"hijo",4);
    char tmp1[64];
    itoa(gettid(),tmp1);
    write(1,tmp1,strlen(tmp1));
    i++;
    }
    }
    int status;
    Pthread_exit(&status);
}

void threadtask2(int l) {
    Sem_wait(sem[0]);
    int i = gettime();
    int j = i+1000;
    while (i < j) {
        int k = gettime();
        if (k > i) i = k;
        if (i%100 == 0) {
            if (l == 1) {
                write(1,"hi1jo",5);
            } else {
                write(1,"hijo",4);
            }
            char tmp1[64];
            itoa(gettid(),tmp1);
            write(1,tmp1,strlen(tmp1));
            i++;
        }
    }
    Sem_post(sem[0]);
    int status;
    Pthread_exit(&status);
}

void jp1() {
  //fork, exit, create thread, exit thread
  struct Pthread_t tid;
  int res = fork();
  for (int i = 0; i < 5; ++i) {
  Pthread_create(&tid, (void *)threadtask1, NULL);
  }
  int i = 0;
  while(1) { 
      char tmp[64];
    int j = gettime();
    if (j > i) i = j;
    if (i%100 == 0 ) {
    itoa(getpid(), tmp);
    write(1,"padre",5);
    write(1, tmp, strlen(tmp));
    i++;
    }
    if (j > 3000 && res == 0) exit();
  }
}

void jp2() {
  //join thread
  struct Pthread_t tid;
  struct Pthread_t tid2;
  Pthread_create(&tid, (void *)threadtask1, NULL);
  Pthread_create(&tid2, (void *)threadtask1, NULL);
  struct Pthread_t* tidp[1];
  struct Pthread_t* tidp2[1];
  getthread(tidp,tid.tid);
  getthread(tidp2,tid2.tid);
  int i = 0;
  int res[1];
  res[0] = -1;
  int* res2[1];
  res2[0] = res;
  Pthread_join(tidp[0],(void **)res2);
  res[0] = -1;
  while(1) { 
      char tmp[64];
    int j = gettime();
    if (j > i) i = j;
    if (i%100 == 0 ) {
    itoa(getpid(), tmp);
    write(1,"padre",5);
    write(1, tmp, strlen(tmp));
    i++;
    }
    if (j > 3000 && res[0] == -1) {
        Pthread_join(tidp2[0], (void **)res2);
        write(1,"join",4);
    }
  }
}

void jp3() {
  //Semaforos
  struct sem_t sem1;
  int i = -1;
  i = Sem_init(&sem1, 1);
  if (i != -1) write(1,"seminit",7);
  getsem(sem,i);
  struct Pthread_t tid;
  struct Pthread_t tid2;
  Pthread_create(&tid, (void *)threadtask2, (void *)1);
  Pthread_create(&tid2, (void *)threadtask2, (void *)0);
  struct Pthread_t* tidp[1];
  struct Pthread_t* tidp2[1];
  getthread(tidp,tid.tid);
  getthread(tidp2,tid2.tid);
  i = -1;
  int res[1];
  res[0] = -1;
  int* res2[1];
  res2[0] = res;
  Pthread_join(tidp[0],(void **)res2);
  Pthread_join(tidp2[0], (void **)res2);
  i = Sem_destroy(sem[0]);
  if (i == 0) write(1,"semdestroy",10);
  write(1,"end",3);
//  while(1) {}
}

int time1,time2,time3,time4;

void rutina_s(){
    write(1,"\nTardo en crearme: ",20);
    time2=gettime();
    char c;
    itoa(time2-time1,&c);
    write(1,&c,strlen(&c));
    write(1," ticks de reloj.",16);
    int status;
    Pthread_exit(&status);
}


void cargas(){
    struct Pthread_t tid;
    int res[1];
    res[0] = -1;
    int* res2[1];
    res2[0] = res;
    time1=gettime();
    Pthread_create(&tid,(void *)rutina_s,NULL);
    struct Pthread_t* tidp[1];
    getthread(tidp,tid.tid);
    time3=gettime();
    Pthread_join(tidp[0],(void **)res2);
    time4=gettime();
    write(1,"\nTardo en destruirme: ",23);
    char c;
    itoa(time4-time3,&c);
    write(1,&c,strlen(&c));
    write(1," ticks de reloj.",16);
}

int __attribute__ ((__section__(".text.main")))
  main(void)
{
    /* Next line, tries to move value 0 to CR3 register. This register is a privileged one, and so it will raise an exception */
     /* __asm__ __volatile__ ("mov %0, %%cr3"::"r" (0) ); */
     //jp1();
     //jp2();
     //jp3();
     cargas();
    while(1) {}
}

