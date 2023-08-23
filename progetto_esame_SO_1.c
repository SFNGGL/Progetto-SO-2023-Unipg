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

/* fork()s of a process don't share more then the initial information after fork().
 * Every change in one process is unknown to the other and you would need to pass the
 * information as a message (e.g. through a pipe, see "man pipe"). */

/* raise(SIGINT); */

#define TIME_TO_EAT 7
#define TIME_TO_STARVE 8
#define BUF_LEN 100

#define check_starvation(a, b, c) { do {(a) == 'b' && (b) == -1 && *(c) != 1 ? *(c) = 1 : 0; } while(0); return 1; }

int limit;
int * global_flag;
const char nome_shm[] = "/my_shm";
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

void handler(int iSigNumber, siginfo_t * info, void * other);
int valid_mode(char cand_mode);
int routine(struct controller * cntrl, int task);
int planned_eating(char mode, int n, int turn, sem_t * semaphores, int * flag, struct timespec * abs_timeout, int * cntr);
int planned_eating2(char mode, int n, int turn, sem_t * semaphores, int * flag, struct timespec * abs_timeout, int * cntr);
void cleaning(int n, sem_t * semaphores, int shm_len);
void raise_all_sem(int n, sem_t * semaphores);
void draw_status(int n, pid_t * processes, sem_t * semaphores, int * semValue);
void dummy_work(int * flag);

int main(int argc, char *argv[])
{
  int i, n;
  pid_t exitPid;
  char mode;
  sem_t * sem;
  struct timespec ts;
  int pid = getpid();

  int fd = shm_open(nome_shm, O_CREAT|O_RDWR, S_IRUSR|S_IWUSR);
  if (argc == 3 && (n = atoi(argv[1])) >= 2 && valid_mode((mode = argv[2][1]))) {
    if (fd != -1) {
      int block_len = sizeof(sem_t)*n+sizeof(int)*2;
      pid_t process_ids[n];
      int nSem = n;
      ftruncate(fd, block_len);

      /* mapping della memoria */
      if ((sem = mmap(NULL, block_len, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0)) != MAP_FAILED) {
        close(fd);

        for (int p = 0; p < n; p++) {
          /* n semafori per n processi */
          if (sem_init(sem+p, 1, 1) == -1) {
            return 0;
          }
        }

        global_flag = (int *)(sem+n);
        int * global_cntr = global_flag+1;
        *global_flag = 0;
        printf("%s%p%s%d\n", "Shared flag: ", global_flag, "\nValue: ", *global_flag);

        struct sigaction sa;
        memset(&sa, '\0', sizeof(sa));
        sa.sa_sigaction = handler;

        sigaction(SIGINT, &sa, NULL);
        sigaction(SIGUSR1, &sa, NULL);
        sigaction(SIGUSR2, &sa, NULL);

        fprintf(stdout, "%s%d%s\n", "Benvenuti cari ", n, " ospiti");

        /* crea i processi figli */
        for (i = 0; i < n; ++i) {
          if ((process_ids[i] = fork()) < 0) {
            perror("fork");
            exit(-1);
          } else if (process_ids[i] == 0) {

            /* child work */
            exitPid = planned_eating2(mode, n, i, sem, global_flag, &ts, global_cntr);

            exit(1);
          }
        }

        /* Wait for children to exit. */
        int status;
        pid_t pid = 0;
        while (n) {

          pid = wait(&status);

          printf("%s%ld%s%x.\n", "Child with PID ", (long)pid, " exited with status 0x", WEXITSTATUS(status));

          if (mode == 'd' || mode == 'b') {
            /* raise_all_sem(nSem, sem); */
          }

          n--;
          fprintf(stdout, "%s%d%s\n", "Rimangono ", n, " ospiti");
        }
        cleaning(nSem, sem, block_len);

        printf("%s\n", "Banchetto finito!");

        return EXIT_SUCCESS;
      } else {
        perror("Fallito mapping");
        return 0;
      }
    } else {
      perror("Fallita apertura spazio di memoria");
      return 0;
    }
  } else {
    printf("%s%s%s\n%s\n%s\n%s\n%s\n%s\n", 
           "Usage: ", argv[0], " [NUMBER]... [FLAG]...", 
           "-n\tnessuna funzionalita' attivata", 
           "-d\trivelo del solo stallo (non attiva soluzione)", 
           "-a\tattivazione della soluzione (non rivela starvation)", 
           "-b\trivelo sia di stallo che di starvation (attiva la soluzione)", 
           "NUMBER dev'essere un intero positivo compreso tra [0 e 255]");
    return 0;
  }
}

int planned_eating2(char mode, int n, int turn, sem_t * semaphores, int * flag, struct timespec * abs_timeout, int * cntr){
  clock_gettime(CLOCK_REALTIME, abs_timeout);
  abs_timeout->tv_sec += TIME_TO_STARVE;
  int iFlagValue;
  while (*flag == 0) {
    if (mode == 'n' || mode == 'd') {
      /* normale routine */
      if (!(*flag)) {
        sem_wait(semaphores+(turn % n));
      }
      fprintf(stdout, "%d%s%d\n", getpid(), ": bloccato il semaforo 1 n.", turn % n);
      if (mode == 'd') {
        (*cntr)++;
        if (*(cntr) >= n) {
          fprintf(stdout, "%s\n", "Stallo");
          raise(SIGINT);
          sem_post(semaphores+(turn % n));
          return -2;
        } else {
          /* fprintf(stderr, "%d%s\n", *cntr, " processi a rischio"); */
        }
      }
      if (!(*flag)) {
        sem_wait(semaphores+((turn+1) % n));
      }
      fprintf(stdout, "%d%s%d\n", getpid(), ": bloccato il semaforo 2 n.", (turn + 1) % n);
      if (mode == 'd') {
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
    }
    else if (mode == 'a' || mode == 'b') {
      /* soluzione */
     if (turn == n-1) {
        /* caso di asimmetria, soluzione a rischi di stallo */
        if (mode == 'b') {
          if (sem_timedwait(semaphores+(0 % n), abs_timeout) == -1) {
            /* morto di fame */
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
            /* morto di fame */
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
  for (int i = 0; i < n; i++) {
    sem_destroy(semaphores+i);
  }
  printf("%s\n", "Sparecchiate tutte le stoviglie");
  /* pulizia dello spazio di shared memory */
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
    /* case 't': return 1; */
    /* break; */
    default: return 0;
  }
}

void raise_all_sem(int n, sem_t * semaphores){
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
