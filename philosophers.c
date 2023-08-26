#include "supper.h"

/* fork()s of a process don't share more then the initial information after fork().
 * Every change in one process is unknown to the other and you would need to pass the
 * information as a message (e.g. through a pipe, see "man pipe"). */

/* raise(SIGINT); */

int limit;

int main(int argc, char *argv[])
{
  /* variabili in uso dal programma */
  int i, n;
  pid_t exitPid;
  char mode;
  sem_t * sem;
  struct timespec ts;
  int pid = getpid();

  /* creazione dello shared object */
  int fd = shm_open(nome_shm, O_CREAT|O_RDWR, S_IRUSR|S_IWUSR);
  /* controllo sui parametri */
  if (argc == 3 && (n = atoi(argv[1])) >= 2 && valid_mode((mode = argv[2][1]))) {
    if (fd != -1) {
      int block_len = sizeof(sem_t)*n+sizeof(int)*2;
      pid_t process_ids[n];
      int nSem = n;
      ftruncate(fd, block_len); // ridimensionamento appropriato dello shm

      /* mapping della memoria */
      if ((sem = mmap(NULL, block_len, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0)) != MAP_FAILED) {
        close(fd);

        for (int p = 0; p < n; p++) {
          /* n semafori per n processi */
          if (sem_init(sem+p, 1, 1) == -1) {
            return 0;
          }
        }

        /* bandiere condivise: modalita' spartana per IPC: lettura da stessi indirizzi */
        global_flag = (int *)(sem+n);
        int * global_cntr = global_flag+1;
        *global_flag = 0;
        printf("%s%p%s%d\n", "Shared flag: ", global_flag, "\nValue: ", *global_flag);

        /* redirezionamento della gestione dei segnali */
        struct sigaction sa;
        memset(&sa, '\0', sizeof(sa));
        sa.sa_sigaction = handler;

        sigaction(SIGINT, &sa, NULL);
        /* sigaction(SIGUSR1, &sa, NULL); */
        /* sigaction(SIGUSR2, &sa, NULL); */

        fprintf(stdout, "%s%d%s\n", "Benvenuti cari ", n, " ospiti");

        /* crea i processi figli */
        for (i = 0; i < n; ++i) {
          if ((process_ids[i] = fork()) < 0) {
            perror("fork");
            exit(-1);
          } else if (process_ids[i] == 0) {

            /* routine dei processi figli */
            exitPid = planned_eating2(mode, n, i, sem, global_flag, &ts, global_cntr);

            exit(1);
          }
        }

        /* attesa dei figli: evita processi zombie */
        int status;
        pid_t pid = 0;
        while (n) {

          pid = wait(&status);

          printf("%s%ld%s%x.\n", "Child with PID ", (long)pid, " exited with status 0x", WEXITSTATUS(status));

          /* attendi n processi che cambino stato */
          n--;
          fprintf(stdout, "%s%d%s\n", "Rimangono ", n, " ospiti");
        }
        /* pulizia della memoria occupata */
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

