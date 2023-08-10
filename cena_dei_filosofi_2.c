#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>
#include <time.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/mman.h>

#define TIME_TO_EAT 10
#define BUF_LEN 200
int block, semValue;
const char nome_shm[] = "/my_shm";

void handler(int iSigNumber){
  /* handles ^+c signal */
  /* */
  block = 1;
  return;
}

void planned_eating(int n, int turn, sem_t * semaphores, int * flag, char * text){
  /* implementazione della soluzione per il problema */
  if (*flag) {
    return;
  } else {
    sem_wait(semaphores+(turn % n));
    sprintf(text, "%d%s%d\n", getpid(), ": Prendo la forchetta ", turn % n);
    write(1, text, strlen(text));
    sem_wait(semaphores+(turn+1 % n));
    sprintf(text, "%d%s%d\n", getpid(), ": Prendo la forchetta ", turn+1 % n);
    write(1, text, strlen(text));

    sprintf(text, "%d%s\n", getpid(), ": Sto mangiando...");
    write(1, text, strlen(text));
    sleep(rand() % TIME_TO_EAT);

    sem_post(semaphores+(turn % n));
    sem_post(semaphores+(turn+1 % n));
    sprintf(text, "%d%s\n", getpid(), ": Poso le forchette");
    write(1, text, strlen(text));

    fflush(stdout);

    planned_eating(n, turn, semaphores, flag, text);
  }
}

void random_eating(int n, sem_t * semaphores, int * flag, char * text){
  if (*flag) {
    return;
  } else {
    int slot_1 = rand() % n;
    int slot_2;
    do {
      slot_2 = rand() % n;
    } while(slot_2 == slot_1);
    sem_wait(semaphores+(slot_1 % n));
    sprintf(text, "%d%s%d\n", getpid(), ": Prendo la forchetta ", slot_1 % n);
    write(1, text, strlen(text));
    sem_wait(semaphores+(slot_2 % n));
    sprintf(text, "%d%s%d\n", getpid(), ": Prendo la forchetta ", slot_2 % n);
    write(1, text, strlen(text));

    sprintf(text, "%d%s\n", getpid(), ": Sto mangiando...");
    write(1, text, strlen(text));
    sleep(rand() % TIME_TO_EAT);

    sem_post(semaphores+(slot_1 % n));
    sem_post(semaphores+(slot_2 % n));
    sprintf(text, "%d%s\n", getpid(), ": Poso le forchette");
    write(1, text, strlen(text));

    fflush(stdout);

    /* return; */
    random_eating(n, semaphores, flag, text);
  }
}

void cleaning(int n, sem_t * semaphores, int * shm_len){
  /* pulizia dei semafori */
  for (int i = 0; i < n; i++) {
    sem_destroy(semaphores+i);
  }
  printf("%s\n", "Sparecchiate tutte le stoviglie");
  /* pulizia dello spazio di shared memory */
  munmap(semaphores, *shm_len);
  shm_unlink(nome_shm);
}

void waiting(int n, pid_t * process_ids){
  /* attesa su gli n sottoprocessi (filosofi) */
  for (int i = 0; i < n; i++) {
    waitpid(process_ids[i], NULL, 0);
  }
}

int main(int argc, char *argv[])
{
  sem_t * sem;
  struct timespec ts;
  int pid = getpid();
  int n;
  int * flag = &block;

  /* apertura dello spazio di memoria condivisa */
  int fd = shm_open(nome_shm, O_CREAT|O_RDWR, S_IRUSR|S_IWUSR);
  if (argc >= 2 && (n = atoi(argv[1])) >= 3) {
    if (fd != -1) {
      int block_len = sizeof(sem_t)*n;
      pid_t process_ids[n];
      char* chat[n];
      ftruncate(fd, block_len);

      /* mapping della memoria */
      if ((sem = mmap(NULL, block_len, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0)) != MAP_FAILED) {
        close(fd);

        for (int i = 0; i < n; i++) {
          /* n semafori per n processi */
          if (sem_init(sem+i, 1, 1) == -1) {
            return 0;
          }
        }

        int wstatus;
        char msg[BUF_LEN];

        struct sigaction sa;
        memset(&sa, '\0', sizeof(sa));
        sa.sa_handler = handler;

        sigaction(SIGINT, &sa, NULL);

        /* pid_t pid = fork(); */
        printf("%s\n", "Benvenuti cari ospiti!");

        /* sottoprocessi */
        for (int i = 0; i < n; i++) {
          /* creazione di ogni sottoprocesso */
          process_ids[i] = fork();
          /* contenuto di ogni sottoprocesso */
          if (process_ids[i] < 0) {
            /* errore */
            perror("Fork failed");
            exit(EXIT_FAILURE);
          } else if (process_ids[i] == 0) {
            /* child */
            sprintf(msg, "%s%d\n", "Arrivato il commensale n. ", getpid());
            write(1, msg, strlen(msg));

            planned_eating(n, i, sem, flag, msg);
            /* random_eating(n, sem, flag, msg); */

            exit(EXIT_SUCCESS);
          }
          else {
            /* parent */
          }
        }
        /* fine del pranzo: pulizia */
        waiting(n, process_ids);
        cleaning(n, sem, &block_len);
      }
    }
  } else {
    printf("%s%s%s\n", "Usage: ", 
           argv[0], 
           " [NUMBER]... [FLAGS]...");
  }

  return EXIT_SUCCESS;
}

