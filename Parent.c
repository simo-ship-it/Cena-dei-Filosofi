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

#define num_filo 5
#define NUM_FILOSOFI_MAX 100
#define STARVATION_TIMEOUT 3 // Tempo in secondi prima che un filosofo vada in starvation

sem_t *forks[NUM_FILOSOFI_MAX];


int stallo_attivo = 0;
int soluzione_stallo_attiva = 0;
int starvation_attivo = 0;
volatile sig_atomic_t segnale_ricevuto = 0;



char forks_sem_name[30];

// Flag per segnalare se l'applicazione deve terminare
int termina = 0;

// Struttura per tenere traccia dell'ultimo pasto di ciascun filosofo
typedef struct {
    int id;
    time_t last_meal_time;
} FilosofoInfo;

FilosofoInfo filosofi_info[NUM_FILOSOFI_MAX];


// Funzione per la terminazione pulita dell'applicazione
void clean_exit() {
    // printf("Terminazione dell'applicazione.\n");
    // Qui liberare eventuali risorse allocate
    // Chiudere file descriptor aperti, liberare memoria allocata, ecc.
    // Chiudi i semafori


    for (int i = 0; i < num_filo; i++) {
        sprintf(forks_sem_name, "/fork_sem_%d_%d", getpid(), i);
        sem_unlink(forks_sem_name);
        // printf("Semaforo %s rimosso\n", forks_sem_name);
        
    }


    // Notifica all'utente che l'applicazione sta terminando
    // printf("L'applicazione sta terminando...\n");

    // Termina il processo corrente
    exit(EXIT_SUCCESS);
}

// Funzione per gestire il segnale SIGINT (Ctrl+C)
void sigint_handler(int sig) {
    printf("\nRicevuto SIGINT. Terminazione dell'applicazione.\n");

    clean_exit();
    // exit(0);
    termina = 1; // Imposta il flag di terminazione
}

// Funzione per controllare se un filosofo è in starvation
void check_starvation(int num_filosofi) {
    for (int i = 0; i < num_filosofi; i++) {
        time_t current_time = time(NULL);
        if (current_time - filosofi_info[i].last_meal_time > STARVATION_TIMEOUT) {
            printf("Filosofo %d è in starvation\n", i);

            clean_exit();   
            return;
        }
    }
    // Nessun filosofo è in starvation
}

void filosofo(int id) {
    int sinistra = id;
    int destra = (id + 1) % NUM_FILOSOFI_MAX;
    if (id + 1 == num_filo) {
        destra = 0;
    }

    while (!segnale_ricevuto) {
        printf("Filosofo %d: Pensa\n", id);
        // usleep(100000); // Simula il pensiero per 0.1 secondi

        // Controlla se è il filosofo destinato ad andare in starvation
        if (starvation_attivo) {
            check_starvation(id);
        }


        // Controlla se c'è uno stallo
        if (stallo_attivo ) {
            int sinistra_locked = sem_trywait(forks[sinistra]);
            int destra_locked = sem_trywait(forks[destra]);

            if (sinistra_locked == 0 && destra_locked == 0) {
                // Entrambe le forchette sono state acquisite, quindi non c'è stallo
                sem_post(forks[sinistra]);
                sem_post(forks[destra]);
            } else {
                // Almeno una delle forchette non è stata acquisita, quindi c'è stallo
                printf("Filosofo %d è in stallo\n", id);

                // clean_exit();
                return;
            }
        }


        // Prende la forchetta a sinistra
        sem_wait(forks[sinistra]);
        printf("Filosofo %d: Prende la forchetta sinistra (%d)\n", id, sinistra);

        sleep(10);

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
        int tempo_di_attesa = rand() % 1000000; // Ritardo casuale fino a 1 secondo
        // int tempo_di_attesa = 1000000; // Ritardo casuale fino a 1 secondo
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
    // Installa il gestore per SIGINT
    signal(SIGINT, sigint_handler);

	// int stallo = 0; // Flag per l'opzione 'a'
    // int evita_stallo_ma_non_stavation = 0; // Flag per l'opzione 'a'
    // int evita_stallo_e_evita_stavation = 0; // Flag per l'opzione 'b'


    // Esegui il programma
    // while (!termina) {
        




    if (argc > 2 || strlen(argv[1]) > 2 ) { // Se sono presenti argomenti sulla riga di comando o La parola inserita è più lunga di due caratteri
        printf("inserisci un singolo flag tra: \n -a per la stavation \n -b per evitare lo stallo ma non rilevare la starvation \n -c per evitare lo stallo e rilevare la starvation \n -n per non attivare nessuna delle modalità precedenti\n ");
        exit(0);
    }
    else { // Se sono presenti argomenti sulla riga di comando
            if (argv[1][0] == '-') { // Se l'argomento inizia con un trattino
                switch (argv[1][1]) { // Controlla il carattere dopo il trattino
                    case 'a':
                        stallo_attivo = 1; // solo rilevamento dello stallo 
                        printf("stallo_attivo: %d\n", stallo_attivo);
                        break;
                    case 'b':
                        soluzione_stallo_attiva = 1; // evita lo stallo ma non rileva starvation
                        printf("soluzione_stallo_attiva: %d\n", soluzione_stallo_attiva);
                        break;
                    case 'c':
                        starvation_attivo = 1;; // evita lo stallo e rileva starvation
                        printf("starvation_attivo: %d\n", starvation_attivo);
                        break;
                    case 'n':
                        printf("nessun flag attivato\n");
                        break;
                    default:
                        printf("Opzione non riconosciuta: %s\n", argv[1]);
                        break;
                }
            }

    }






    // Rimuove i semafori forks se presenti
    for (int i = 0; i < num_filo; i++) {
        sprintf(forks_sem_name, "/fork_sem_%d_%d", getpid(), i);
        sem_unlink(forks_sem_name);
        // printf("Semaforo %s rimosso\n", forks_sem_name);
    }

    // Inizializza i semafori


    // Inizializza le forchette
    for (int i = 0; i < num_filo; i++) {
        sprintf(forks_sem_name, "/fork_sem_%d_%d", getpid(), i);
        forks[i] = sem_open(forks_sem_name, O_CREAT | O_EXCL, 0644, 1);
        // printf("Semaforo %s creato\n", forks_sem_name);
        if (forks[i] == SEM_FAILED) {
            perror("Errore nell'inizializzazione del semaforo forks");
            return 1;
        }
    }

    // Inizializza le informazioni dei filosofi
    for (int i = 0; i < num_filo; i++) {
        filosofi_info[i].id = i;
        filosofi_info[i].last_meal_time = time(NULL);
    }

    // Crea i processi filosofi
    pid_t pid;
    for (int i = 0; i < num_filo; i++) {
        // printf("il pid è: %d\n", pid);
        pid = fork();
        
        if (pid == 0) { // processo figlio (filosofo)
            filosofo(i);
            exit(0);
        }
    }

    // Introduce ritardo per far andare in starvation almeno un filosofo
    // sleep(STARVATION_TIMEOUT + 1);

	
    // printf("Lavoro in corso...\n");


    pthread_join(tid, NULL);

    // Termina l'applicazione in modo pulito
    clean_exit();


    return 0; // Non raggiunto poiché clean_exit() termina il programma
}