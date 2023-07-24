#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/mman.h>

int block, semValue;
const char nome_shm[] = "/my_shm";

void handler(int iSigNumber){
  /* handles ^+c signal */
  block = 1;
}

void print_status(sem_t * utensil, int * value){
  sem_getvalue(utensil, value);
  printf("Forchetta %p: %d\n", utensil, *value);
}

void eating(int n, int turn, sem_t * semaphores){
  if (block) {
    return;
  }

  sem_wait(semaphores+(turn % n));
  printf("%d: Prendo le forchetta a Dx\n", getpid());
  sem_wait(semaphores+(turn+1 % n));
  printf("%d: Prendo le forchetta a Sx\n", getpid());

  printf("%d: ...Mangiando...\n", getpid());
  sleep(rand() % 8);

  sem_post(semaphores+(turn % n));
  sem_post(semaphores+(turn+1 % n));
  printf("%d: Poso le forchette\n", getpid());

  fflush(stdout);

  /* eating(n, turn, semaphores); */
}

void cleaning(int n, sem_t * semaphores){
  for (int i = 0; i < n; i++) {
    sem_destroy(semaphores+i);
  }
}

void waiting(int n, pid_t proc_IDs[]){
  for (int i = 0; i < n; i++) {
    waitpid(proc_IDs[i], NULL, 0);
  }
}

int main(int argc, char *argv[])
{
  sem_t *sem;
  struct timespec ts;
  int pid = getpid();
  int n;
  int *flag = &block;

  int fd = shm_open(nome_shm, O_CREAT|O_RDWR, S_IRUSR|S_IWUSR);
  if (argc >= 2 && (n = atoi(argv[1])) >= 3) {
    if (fd != -1) {
      int block_len = sizeof(sem_t)*n;
      pid_t process_ids[n];
      char * names[n];

      ftruncate(fd, block_len);

      if ((sem = mmap(NULL, block_len, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0)) != MAP_FAILED) {
        close(fd);

        for (int i = 0; i < n; i++) {
          if(sem_init(sem+i, 1, 1) == -1){
            return EXIT_FAILURE;
          }
        }

        struct sigaction sa;
        memset(&sa, '\0', sizeof(sa));
        sa.sa_handler = handler;

        sigaction(SIGINT, &sa, NULL);

        for (int i = 0; i < n; i++) {
          process_ids[i] = fork();
          if (process_ids[i] < 0) {
            perror("Fork failed");
            exit(EXIT_FAILURE);
          } else if (process_ids[i] == 0) {
            /* child */
            for (;block == 0;) {
              eating(n, i, sem);
            }
          } else {
            /* parent */
          }
        }
      }

      /* "main parent": attesa e pulizia di tutte le risorse usate */

      waiting(n, process_ids);
      cleaning(n, sem);
      shm_unlink(nome_shm);
    }
  } else {
    printf("Usage: %s [NUMBER]... [FLAGS]...\n", argv[0]);
  }

  return EXIT_SUCCESS;
}

