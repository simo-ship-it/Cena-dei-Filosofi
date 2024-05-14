#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <pthread.h>

#define NUM_FILOSOFI_MAX 100
#define STARVATION_TIMEOUT 3 // Tempo in secondi prima che un filosofo vada in starvation

sem_t *forks[NUM_FILOSOFI_MAX];
sem_t *stallo_sem;
sem_t *starvation_sem;
int stallo_attivo = 0;
int soluzione_stallo_attiva = 0;
int starvation_attivo = 0;
volatile sig_atomic_t segnale_ricevuto = 0;

// Struttura per tenere traccia dell'ultimo pasto di ciascun filosofo
typedef struct {
    int id;
    time_t last_meal_time;
} FilosofoInfo;

FilosofoInfo filosofi_info[NUM_FILOSOFI_MAX];

// Funzione per controllare se un filosofo è in starvation
void check_starvation(int num_filosofi) {
    for (int i = 0; i < num_filosofi; i++) {
        time_t current_time = time(NULL);
        if (current_time - filosofi_info[i].last_meal_time > STARVATION_TIMEOUT) {
            printf("Filosofo %d è in starvation\n", i);
            sem_post(starvation_sem); // Sveglia il processo principale per terminare il programma
            return;
        }
    }
    // Nessun filosofo è in starvation
}

void filosofo(int id) {
    int sinistra = id;
    int destra = (id + 1) % NUM_FILOSOFI_MAX;

    while (!segnale_ricevuto) {
        printf("Filosofo %d: Pensa\n", id);
        usleep(100000); // Simula il pensiero per 0.1 secondi

        // Controlla se è il filosofo destinato ad andare in starvation
        if (id == 0 && starvation_attivo) {
            printf("Filosofo %d: In attesa di starvation\n", id);
            sem_wait(starvation_sem); // Attende la starvation
            return;
        }

        // Prende la forchetta a sinistra
        sem_wait(forks[sinistra]);
        printf("Filosofo %d: Prende la forchetta sinistra (%d)\n", id, sinistra);

        // Prende la forchetta a destra
        sem_wait(forks[destra]);
        printf("Filosofo %d: Prende la forchetta destra (%d)\n", id, destra);

        // Mangia
        printf("Filosofo %d: Mangia\n", id);

        // Aggiorna il tempo dell'ultimo pasto del filosofo
        filosofi_info[id].last_meal_time = time(NULL);

        // Rilascia la forchetta a sinistra
        sem_post(forks[sinistra]);
        printf("Filosofo %d: Rilascia la forchetta sinistra (%d)\n", id, sinistra);

        // Rilascia la forchetta a destra
        sem_post(forks[destra]);
        printf("Filosofo %d: Rilascia la forchetta destra (%d)\n", id, destra);

        // Introduce un ritardo casuale prima della prossima azione
        // int tempo_di_attesa = rand() % 1000000; // Ritardo casuale fino a 1 secondo
        printf("sono qui\n");
        
        int tempo_di_attesa = 1000000; // Ritardo casuale fino a 1 secondo
        usleep(tempo_di_attesa);
    }
}

void *signal_thread(void *arg) {
    int sig;
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);

    while (1) {
        sigwait(&set, &sig);
        printf("\nProgramma interrotto dall'utente.\n");
        segnale_ricevuto = 1;
        printf("Segnale SIGINT ricevuto. segnale_ricevuto: %d\n", segnale_ricevuto);

        // Forza lo svuotamento del buffer di output
        fflush(stdout);

        sem_unlink("/stallo_sem");
        sem_unlink("/starvation_sem");
        for (int i = 0; i < NUM_FILOSOFI_MAX; i++) {
            sem_unlink("/fork_sem");
        }
        exit(0);
   }

    return NULL;
}

int main(int argc, char *argv[]) {
    pthread_t tid;
    pthread_create(&tid, NULL, signal_thread, NULL);

    if (argc != 5) {
        printf("Usage: %s <num_filosofi> <a_flag> <b_flag> <c_flag>\n", argv[0]);
        return 1;
    }

    int num_filosofi = atoi(argv[1]);
    if (num_filosofi <= 0 || num_filosofi > NUM_FILOSOFI_MAX) {
        printf("Numero di filosofi non valido\n");
        return 1;
    }

    int a_flag = atoi(argv[2]);
    int b_flag = atoi(argv[3]);
    int c_flag = atoi(argv[4]);

    // Controllo che sia specificato solo un flag
    if ((a_flag + b_flag + c_flag) != 1) {
        printf("Errore: specificare esattamente un flag alla volta\n");
        return 1;
    }

    stallo_attivo = a_flag;
    soluzione_stallo_attiva = b_flag;
    starvation_attivo = c_flag;

    // Genera nomi univoci per i semafori
    char stallo_sem_name[30];
    char starvation_sem_name[30];
    char forks_sem_name[30];

    sprintf(stallo_sem_name, "/stallo_sem_%d", getpid());
    printf(stallo_sem_name, "/stallo_sem_%d\n", getpid()); // Stampa per evitare warning
    sprintf(starvation_sem_name, "/starvation_sem_%d", getpid());
    printf(starvation_sem_name, "/starvation_sem_%d\n", getpid()); // Stampa per evitare warning


    // Rimuove il semaforo stallo_sem se presente
    sem_unlink(stallo_sem_name);

    // Rimuove il semaforo starvation_sem se presente
    sem_unlink(starvation_sem_name);

    // Rimuove i semafori forks se presenti
    for (int i = 0; i < num_filosofi; i++) {
        sprintf(forks_sem_name, "/fork_sem_%d_%d", getpid(), i);
        sem_unlink(forks_sem_name);
    }

    // Inizializza i semafori
    stallo_sem = sem_open(stallo_sem_name, O_CREAT | O_EXCL, 0644, 1);
    if (stallo_sem == SEM_FAILED) {
        perror("Errore nell'inizializzazione del semaforo stallo_sem");
        return 1;
    }
    starvation_sem = sem_open(starvation_sem_name, O_CREAT | O_EXCL, 0644, 0); // Inizializzato a 0
    if (starvation_sem == SEM_FAILED) {
        perror("Errore nell'inizializzazione del semaforo starvation_sem");
        return 1;
    }

    // Inizializza le forchette
    for (int i = 0; i < num_filosofi; i++) {
        sprintf(forks_sem_name, "/fork_sem_%d_%d", getpid(), i);
        forks[i] = sem_open(forks_sem_name, O_CREAT | O_EXCL, 0644, 1);
        if (forks[i] == SEM_FAILED) {
            perror("Errore nell'inizializzazione del semaforo forks");
            return 1;
        }
    }

    // Inizializza le informazioni dei filosofi
    for (int i = 0; i < num_filosofi; i++) {
        filosofi_info[i].id = i;
        filosofi_info[i].last_meal_time = time(NULL);
    }

    // Crea i processi filosofi
    pid_t pid;
    for (int i = 0; i < num_filosofi; i++) {
        pid = fork();
        if (pid == 0) { // processo figlio (filosofo)
            filosofo(i);
            exit(0);
        }
    }

    // Introduce ritardo per far andare in starvation almeno un filosofo
    sleep(STARVATION_TIMEOUT + 1);

    if (starvation_attivo) {
        // Se il flag per la starvation è attivo, controlla se un filosofo è in starvation
        check_starvation(num_filosofi);
        // Se un filosofo è in starvation, termina il programma
        printf("Programma terminato a causa della starvation di un filosofo\n");


        // Attendere la terminazione dei filosofi
        for (int i = 0; i < num_filosofi; i++) {
            wait(NULL);
        }

        // Chiudi i semafori
        sem_unlink(stallo_sem_name);
        sem_unlink(starvation_sem_name);
        for (int i = 0; i < num_filosofi; i++) {
            sprintf(forks_sem_name, "/fork_sem_%d_%d", getpid(), i);
            sem_unlink(forks_sem_name);
        }

        pthread_join(tid, NULL);

        return 0;
    }

    // Attendere la terminazione dei filosofi
    for (int i = 0; i < num_filosofi; i++) {
        wait(NULL);
    }

    // Chiudi i semafori
    sem_unlink(stallo_sem_name);
    sem_unlink(starvation_sem_name);
    for (int i = 0; i < num_filosofi; i++) {
        sprintf(forks_sem_name, "/fork_sem_%d_%d", getpid(), i);
        sem_unlink(forks_sem_name);
    }

    pthread_join(tid, NULL);

    return 0;
}
