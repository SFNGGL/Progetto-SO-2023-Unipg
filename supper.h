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

/* macro */
#define TIME_TO_EAT 7
#define TIME_TO_STARVE 8
#define BUF_LEN 100

// non usata
#define check_starvation(a, b, c) { do {(a) == 'b' && (b) == -1 && *(c) != 1 ? *(c) = 1 : 0; } while(0); return 1; }

int * global_flag;
const char nome_shm[] = "my_shm";

// non usata
struct controller {
  sem_t * semaphores;
  char mode;
  int n;
  int turn;
  int offset;
  struct timespec * timer;
  int * flag_ptr;
  int * cntr_ptr;
};

// routine dei processi figli
int planned_eating2(char mode, int n, int turn, sem_t * semaphores, int * flag, struct timespec * abs_timeout, int * cntr){
  // per le timedwait: imposta il "cronometro"
  clock_gettime(CLOCK_REALTIME, abs_timeout);
  abs_timeout->tv_sec += TIME_TO_STARVE;
  int iFlagValue;
  while (*flag == 0) {
    /* check sul parametro di attivazione dei controlli */
    if (mode == 'n' || mode == 'd') {
      /* normale routine */
      if (!(*flag)) {
        sem_wait(semaphores+(turn % n));
      }
      fprintf(stdout, "%d%s%d\n", getpid(), ": bloccato il semaforo 1 n.", turn % n);
      if (mode == 'd') {
        /* attesa con la prima risorsa: rischio di stallo
         * n processi cosi' bloccano il programma*/
        (*cntr)++;
        if (*(cntr) >= n) {
          /* condizione di stallo confermata: il child esce direttamente e alza SIGINT*/
          fprintf(stdout, "%s\n", "Stallo");
          raise(SIGINT);
          sem_post(semaphores+(turn % n)); // rilascio della risorsa occupata
          return -2;
        } else {
          /* fprintf(stderr, "%d%s\n", *cntr, " processi a rischio"); */
        }
      }
      if (!(*flag)) {
        // condizione ridondante per evitare blocchi di un child a programma in 
        // fase di chiusura
        sem_wait(semaphores+((turn+1) % n));
      }
      fprintf(stdout, "%d%s%d\n", getpid(), ": bloccato il semaforo 2 n.", (turn + 1) % n);
      if (mode == 'd') {
        // un child ha bloccato e poi rilascera' entrambe le risorse: non c'e' rischio di stallo
        (*cntr)--;
        /* fprintf(stderr, "%d%s\n", *cntr, " processi a rischio"); */
      }

      if (!(*flag)) {
        /* semplice check per evitare delay nei messaggi
         * a schermo quando i processi devono chiudere*/
        sleep(rand() % TIME_TO_EAT);
      }

      sem_post(semaphores+(turn % n));
      sem_post(semaphores+((turn+1) % n));
      fprintf(stdout, "%d%s\n", getpid(), ": rilasciati i semafori");
      /* rilascio delle risorse e notifica a schermo */
    }
    else if (mode == 'a' || mode == 'b') {
      /* soluzione */
     if (turn == n-1) {
        /* caso di asimmetria, soluzione contro rischi di stallo */
        if (mode == 'b') {
          if (sem_timedwait(semaphores+(0 % n), abs_timeout) == -1) {
            /* TIMEOUT: morte di fame */
            fprintf(stderr, "%s\n", "Morto di fame!");
            raise(SIGINT);
            return -1;
          } else {
            /* ha bloccato la prima risorsa */
            fprintf(stdout, "%d%s%d\n", getpid(), ": bloccato il semaforo 1 n.", 0 % n);
            *(cntr)++;
            if (*(cntr) >= n) {
              fprintf(stdout, "%s\n", "Stallo");
              raise(SIGINT);
              sem_post(semaphores+(0 % n));
              return -2;
            } else {
              /* fprintf(stderr, "%d%s\n", *cntr, " processi a rischio"); */
            }
          }
        } else {
          if (!(*flag)) {
            sem_wait(semaphores+(0 % n));
            fprintf(stdout, "%d%s%d\n", getpid(), ": bloccato il semaforo 1 n.", 0 % n);
          }
        }
        if (mode == 'b') {
          if (sem_timedwait(semaphores+(0+turn % n), abs_timeout) == -1) {
            /* TIMEOUT: morte di fame */
            fprintf(stderr, "%s\n", "Morto di fame!");
            raise(SIGINT);
            return -1;
          } else {
            /* non piu' a rischio stallo */
            fprintf(stdout, "%d%s%d\n", getpid(), ": bloccato il semaforo 2 n.", turn % n);
            *(cntr)--;
            /* fprintf(stderr, "%d%s\n", *cntr, " processi a rischio"); */
          }
        } else {
          if (!(*flag)) {
            sem_wait(semaphores+(0+turn % n));
            fprintf(stdout, "%d%s%d\n", getpid(), ": bloccato il semaforo 2 n.", turn % n);
          }
        }

        if (!(*flag)) {
          sleep(rand() % TIME_TO_EAT);
        }

        fprintf(stdout, "%d%s\n", getpid(), ": rilasciati i semafori");
        sem_post(semaphores+(0 % n));
        sem_post(semaphores+(0+turn % n));
      } else {
        /* caso normale: i != n-1 */
        if (mode == 'b') {
          if (sem_timedwait(semaphores+(turn % n), abs_timeout) == -1) {
            /* morto di fame */
            fprintf(stderr, "%s\n", "Morto di fame!");
            raise(SIGINT);
            return -1;
          } else {
            *(cntr)++;
            if (*(cntr) >= n) {
              fprintf(stdout, "%s\n", "Stallo");
              raise(SIGINT);
              sem_post(semaphores+(turn % n));
              return -2;
            } else {
              fprintf(stdout, "%d%s%d\n", getpid(), ": bloccato il semaforo 1 n.", turn % n);
            }
          }
        } else {
          if (!(*flag)) {
            sem_wait(semaphores+(turn % n));
            fprintf(stdout, "%d%s%d\n", getpid(), ": bloccato il semaforo 1 n.", turn % n);
          }
        }
        if (mode == 'b') {
          if (sem_timedwait(semaphores+((turn+1) % n), abs_timeout) == -1) {
            /* morto di fame */
            fprintf(stderr, "%s\n", "Morto di fame!");
            raise(SIGINT);
            return -1;
          } else {
            /* non piu a rischio stallo */
            fprintf(stdout, "%d%s%d\n", getpid(), ": bloccato il semaforo 2 n.", (turn+1) % n);
            *(cntr)--;
            /* fprintf(stdout, "%d%s\n", *cntr, " processi a rischio"); */
          }
        } else {
          if (!(*flag)) {
            sem_wait(semaphores+((turn+1) % n));
            fprintf(stdout, "%d%s%d\n", getpid(), ": bloccato il semaforo 2 n.", (turn+1) % n);
          }
        }

        if (!(*flag)) {
          sleep(rand() % TIME_TO_EAT);
        }

        fprintf(stdout, "%d%s\n", getpid(), ": rilasciati i semafori");
        sem_post(semaphores+(turn % n));
        sem_post(semaphores+((turn+1) % n));
      }
    }
  }
  return getpid();
}

void cleaning(int n, sem_t * semaphores, int shm_len){
  /* chiusura dei semafori */
  for (int i = 0; i < n; i++) {
    sem_destroy(semaphores+i);
  }
  printf("%s\n", "Sparecchiate tutte le stoviglie");
  /* pulizia del mapping e dello spazio di shared memory */
  munmap(semaphores, shm_len);
  shm_unlink(nome_shm);
}

void handler(int iSigNumber, siginfo_t * info, void * other){
  fprintf(stderr, "%s%d%s%s\n", "Ricevuto SIGNAL ", iSigNumber, ": ", strsignal(iSigNumber));
  /* SIGINT : terminare esecuzione */
  iSigNumber == 2 ? 
    *global_flag = 1 :
    fprintf(stderr, "%s\n", "Bad signal");
  return;
}

int valid_mode(char cand_mode){
  /* controllo della validita' dell'opzione dell'utente */
  switch (cand_mode) {
    case 'n': return 1;
    break;
    case 'd': return 1;
    break;
    case 'a': return 1;
    break;
    case 'b': return 1;
    break;
    case 'h': return 1;
    break;
    default: return 0;
  }
}

void raise_all_sem(int n, sem_t * semaphores){
  /* non piu' usata*/
  for (int p = 0; p < n; p++){
    if(sem_post(semaphores+p) != 0){
      fprintf(stderr, "%s%d%s%s\n", "Errore nel raise del semaforo", errno, " = ", strerror(errno));
    } else {
      /* fprintf(stdout, "%s%d\n", "sbloccato il semaforo n.", p); */
    }
  }
}

void draw_status(int n, pid_t * processes, sem_t * semaphores, int * semValue){
  system("clear");
  sleep(1);
  for (int i = 0; i < n; i++) {
    fprintf(stdout, "   %d   ", i);
  }
  putchar('\n');
  putchar('\n');
  for (int i = 0; i < n; i++) {
    sem_getvalue(semaphores+i, semValue);
    fprintf(stdout, "|  %d  |", *semValue);
  }
  putchar('\n');
  for (int i = 0; i < n; i++) {
    fprintf(stdout, "|%ld|", (long)processes[i]);
  }
  sleep(1);
}
