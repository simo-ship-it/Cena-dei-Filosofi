#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <signal.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>

#define NUM_PHILOSOPHERS_DEFAULT 5

// Dichiarazioni globali
bool stop_execution = false;
int semaphore_id;

// Struttura per l'operazione sul semaforo
struct sembuf acquire_semaphore = {0, -1, SEM_UNDO};
struct sembuf release_semaphore = {0, 1, SEM_UNDO};

// Funzione per l'acquisizione del semaforo
void acquire_sem() {
    semop(semaphore_id, &acquire_semaphore, 1);
}

// Funzione per il rilascio del semaforo
void release_sem() {
    semop(semaphore_id, &release_semaphore, 1);
}

// Gestore del segnale SIGINT
void handle_interrupt(int sig) {
    stop_execution = true;
}

// Funzione per il filosofo
void filosofo(int id_filosofo, int num_filosofi) {
    int forchetta_sinistra = id_filosofo;
    int forchetta_destra = (id_filosofo + 1) % num_filosofi;

    // Modifica per l'ultimo filosofo
    if (id_filosofo == num_filosofi - 1) {
        // Inverti l'ordine delle forchette per l'ultimo filosofo
        forchetta_sinistra = num_filosofi - 1;
        forchetta_destra = id_filosofo;
        printf ("Il filosofo %d ha invertito l'ordine delle forchette.\n", id_filosofo);
    }

    while (!stop_execution) {
        // Inizia ad attendere la presa della prima forchetta
        printf("Il filosofo %d sta attendendo di prendere la forchetta sinistra %d.\n", id_filosofo, forchetta_sinistra);
        sleep(rand() % 3 + 1); // Simulazione del tempo di attesa per prendere la forchetta

        // Acquisisci la prima forchetta
        acquire_sem();

        // Inizia ad attendere la presa della seconda forchetta
        printf("Il filosofo %d sta attendendo di prendere la forchetta destra %d.\n", id_filosofo, forchetta_destra);
        sleep(rand() % 3 + 1); // Simulazione del tempo di attesa per prendere la forchetta

        // Acquisisci la seconda forchetta
        acquire_sem();

        // Mangia
        printf("Il filosofo %d sta mangiando.\n", id_filosofo);
        sleep(rand() % 5 + 1);

        // Rilascia le forchette
        release_sem();
        release_sem();

        // Riposo
        printf("Il filosofo %d sta riposando.\n", id_filosofo);
        sleep(rand() % 3 + 1); // Tempo di riposo prima di iniziare un nuovo ciclo
    }
}

// Funzione per terminare tutti i processi figlio
void terminate_processes(int num_processes) {
    for (int i = 0; i < num_processes; i++) {
        kill(i + 1, SIGTERM); // Invia il segnale di terminazione a tutti i processi figlio
    }
}

int main(int argc, char *argv[]) {
    int num_filosofi = NUM_PHILOSOPHERS_DEFAULT;

    // Installa il gestore del segnale SIGINT
    signal(SIGINT, handle_interrupt);

    // Creazione del semaforo condiviso
    semaphore_id = semget(IPC_PRIVATE, 1, IPC_CREAT | 0666);
    if (semaphore_id == -1) {
        perror("Errore nella creazione del semaforo");
        exit(EXIT_FAILURE);
    }

    // Inizializzazione del semaforo
    if (semctl(semaphore_id, 0, SETVAL, 1) == -1) {
        perror("Errore nell'inizializzazione del semaforo");
        exit(EXIT_FAILURE);
    }

    // Creazione dei processi per i filosofi
    for (int i = 0; i < num_filosofi; i++) {
        pid_t pid = fork();
        if (pid == -1) {
            perror("Errore nella fork");
            exit(EXIT_FAILURE);
        } else if (pid == 0) {
            // Processo figlio (filosofo)
            filosofo(i, num_filosofi);
            exit(EXIT_SUCCESS);
        }
    }

    // Attesa della terminazione dei processi figlio
    while (!stop_execution) {
        // Attendiamo il segnale di interruzione o la terminazione dei processi figlio
        pause();
    }

    // Terminazione di tutti i processi figlio
    terminate_processes(num_filosofi);

    // Deallocazione del semaforo condiviso
    semctl(semaphore_id, 0, IPC_RMID);

    printf("Applicazione terminata correttamente.\n");

    return 0;
}



