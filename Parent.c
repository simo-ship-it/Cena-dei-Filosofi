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
#define STARVATION_TIMEOUT 5 // Tempo in secondi prima che un filosofo vada in starvation

sem_t *forks[NUM_FILOSOFI_MAX];

int stallo_attivo = 0;
int soluzione_stallo_attiva = 0;
int starvation_attivo = 0;
volatile sig_atomic_t segnale_ricevuto = 0;

// pid_t child_pids[NUM_FILOSOFI_MAX];
pid_t pid_padre;

char forks_sem_name[30];

int termina = 0;
// Flag per segnalare se l'applicazione deve terminare


// Struttura per tenere traccia dell'ultimo pasto di ciascun filosofo
typedef struct
{
    int id;
    time_t last_meal_time;
} FilosofoInfo;

FilosofoInfo filosofi_info[NUM_FILOSOFI_MAX];

// Funzione per la terminazione pulita dell'applicazione
void clean_exit()
{
    // printf("Terminazione dell'applicazione.\n");
    // Qui liberare eventuali risorse allocate
    // Chiudere file descriptor aperti, liberare memoria allocata, ecc.

    // for (int i = 0; i < num_filo; i++)
    // {
    //     printf("child_pids[%d]: %d\n", i, child_pids[i]);
    // }
    // chiudi i processi figli
    // for (int i = 0; i < num_filo; i++)
    // {
        
        if (getpid() != pid_padre)
        {   
            printf("termino il processo figlio %d\n", getpid());
            exit(0);
        }
    // }

    // // Attende la terminazione di tutti i processi figli
    // for (int i = 0; i < num_filo; i++) {
    //     printf("2\n");
    //     if (child_pids[i] > 0) {
    //         printf("entra nel 3\n");
    //         printf("Attendo la terminazione del processo figlio %d\n", child_pids[i]);
    //         waitpid(child_pids[i], NULL, 0);
    //     }
    // }
    // Chiudi i semafori
    for (int i = 0; i < num_filo; i++)
    {
        sprintf(forks_sem_name, "/fork_sem_%d_%d", getpid(), i);
        sem_unlink(forks_sem_name);
        // printf("Semaforo %s rimosso\n", forks_sem_name);
    }


    // Notifica all'utente che l'applicazione sta terminando
    // printf("L'applicazione sta terminando...\n");

    // Termina il processo corrente
    sleep(1);
    printf("termino il processo padre %d\n", getpid());
    exit(EXIT_SUCCESS);
}

// Funzione per gestire il segnale SIGINT (Ctrl+C)
void sigint_handler(int sig)
{
    printf("\nRicevuto SIGINT. Terminazione dell'applicazione.\n");

    clean_exit();
    // exit(0);
    // termina = 1; // Imposta il flag di terminazione
}

void *signal_thread(void *arg)
{
    int sig;
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);

    while (1)
    { // Ciclo infinito per attendere i segnali e terminare l'applicazione in modo pulito
        sigwait(&set, &sig);
        // printf("\nProgramma interrotto dall'utente.\n");
        segnale_ricevuto = 1;
        // printf("Segnale SIGINT ricevuto. segnale_ricevuto: %d\n", segnale_ricevuto);

        // Forza lo svuotamento del buffer di output
        fflush(stdout);

        // for (int i = 0; i < NUM_FILOSOFI_MAX; i++) {
        //     sem_unlink("/fork_sem");
        // }
        clean_exit();
        
        // exit(0);
        return NULL;
    }

}

// Funzione per controllare se un filosofo è in starvation
void check_starvation(int id)
{

    time_t current_time = time(NULL);
    printf(" il filosofo %ld ha mangiato %d secondi fa\n", id, current_time - filosofi_info[id].last_meal_time);
    // printf("il tempo attuale è %ld\n", current_time);
    // printf("il tempo dell'ultimo pasto è %ld\n", filosofi_info[id].last_meal_time);

    if ((current_time - filosofi_info[id].last_meal_time) > STARVATION_TIMEOUT)
    {
        printf("Filosofo %d è in starvation\n", id);
        // termina=1;
        // segnale_ricevuto = 1;
        clean_exit();
        return;
    }
    // Nessun filosofo è in starvation
}

void filosofo(int id)
{

    int sinistra = id;
    int destra = (id + 1) % NUM_FILOSOFI_MAX;
    if (id + 1 == num_filo)
    {
        destra = 0;
    }

    // Aggiorna il tempo dell'ultimo pasto del filosofo
    filosofi_info[id].last_meal_time = time(NULL);

    // printf("sini: %d, destra: %d\n", sinistra, destra);
    while (!segnale_ricevuto)
    {
        if (termina==1){
            printf("termino il filosofo %d\n", id);
            exit(0);
        }
        printf("Filosofo %d: Pensa\n", id);
        usleep(100000); // Simula il pensiero per 0.1 secondi

        // Controlla se c'è uno stallo
        if (stallo_attivo)
        {
            int sinistra_locked = sem_trywait(forks[sinistra]);
            int destra_locked = sem_trywait(forks[destra]);

            if (sinistra_locked == 0 && destra_locked == 0)
            {
                // Entrambe le forchette sono state acquisite, quindi non c'è stallo
                sem_post(forks[sinistra]);
                sem_post(forks[destra]);
            }
            else
            {
                // Almeno una delle forchette non è stata acquisita, quindi c'è stallo
                printf("Filosofo %d è in stallo\n", id);

                // clean_exit();
                return;
            }
        }

        if (soluzione_stallo_attiva && id == num_filo - 1)
        { // se abbiamo attivato con il flac -b l'opzione per evitare lo stallo e il filosofo è l'ultimo allora scamambia l'ordine in cui prende le forchette
            // Se tutti i filosofi hanno preso la forchetta sinistra, verifichiamo lo stato di stallo

            printf("sono qui\n  ");
            printf("id: %d\n", id);

            // Prende la forchetta a destra
            sem_wait(forks[destra]);
            printf("Filosofo %d: Prende la forchetta destra (%d)\n", id, destra);

            sleep(3); // attende 3 secondi

            // Prende la forchetta a sinistra
            sem_wait(forks[sinistra]);
            printf("Filosofo %d: Prende la forchetta sinistra (%d)\n", id, sinistra);

            // Controlla se è il filosofo destinato ad andare in starvation
            if (starvation_attivo)
            {
                // printf("il filosofo %d è in attesa di blabla\n", id);
                check_starvation(id);
            }

            // Mangia
            printf("Filosofo %d: Mangia\n", id);

            // Aggiorna il tempo dell'ultimo pasto del filosofo
            filosofi_info[id].last_meal_time = time(NULL);
            printf("il filosofo %d ha mangiato alle %ld\n", id, filosofi_info[id].last_meal_time);

            // Rilascia la forchetta a destra
            sem_post(forks[destra]);
            printf("Filosofo %d: Rilascia la forchetta destra (%d)\n", id, destra);

            // Rilascia la forchetta a sinistra
            sem_post(forks[sinistra]);
            printf("Filosofo %d: Rilascia la forchetta sinistra (%d)\n", id, sinistra);
        }
        else
        { // se non è attiva l'opzione per evitare lo stallo procede normalmente, ovvero ogni filosofo prende prima la forchetta a sinistra e poi quella a destra
            // Prende la forchetta a sinistra
            sem_wait(forks[sinistra]);
            printf("Filosofo %d: Prende la forchetta sinistra (%d)\n", id, sinistra);

            sleep(3); // attende 3 secondi

            // Prende la forchetta a destra
            sem_wait(forks[destra]);
            printf("Filosofo %d: Prende la forchetta destra (%d)\n", id, destra);

            // Controlla se è il filosofo destinato ad andare in starvation
            if (starvation_attivo)
            {
                // printf("il filosofo %d è in attesa di blabla\n", id);
                check_starvation(id);
            }

            // Mangia
            printf("Filosofo %d: Mangia\n", id);
            sleep(3); // Simula il pasto (3 secondi

            // Aggiorna il tempo dell'ultimo pasto del filosofo
            filosofi_info[id].last_meal_time = time(NULL);
            printf("il filosofo %d ha mangiato alle %ld\n", id, filosofi_info[id].last_meal_time);

            // Rilascia la forchetta a sinistra
            sem_post(forks[sinistra]);
            printf("Filosofo %d: Rilascia la forchetta sinistra (%d)\n", id, sinistra);

            // Rilascia la forchetta a destra
            sem_post(forks[destra]);
            printf("Filosofo %d: Rilascia la forchetta destra (%d)\n", id, destra);
        }

        // Introduce un ritardo casuale prima della prossima azione
        int tempo_di_attesa = rand() % 100000; // Ritardo casuale fino a 1 secondo
        // int tempo_di_attesa = 1000000; // Ritardo casuale fino a 1 secondo
        usleep(tempo_di_attesa);
    }
}


int main(int argc, char *argv[])
{

    // // Inizializza l'array dei PID dei processi figli
    // for (int i = 0; i < NUM_FILOSOFI_MAX; i++) {
    // child_pids[i] = -1; // Imposta a -1 per indicare che il PID non è valido
    // }

    // Crea il thread per gestire i segnali
    pthread_t tid;
    pthread_create(&tid, NULL, signal_thread, NULL);
    
    // Installa il gestore per SIGINT
    signal(SIGINT, sigint_handler);

    // int stallo = 0; // Flag per l'opzione 'a'
    // int evita_stallo_ma_non_stavation = 0; // Flag per l'opzione 'a'
    // int evita_stallo_e_evita_stavation = 0; // Flag per l'opzione 'b'

    // Esegui il programma
    // while (!termina) {

    if (argc > 2 || strlen(argv[1]) > 2)
    { // Se sono presenti argomenti sulla riga di comando o La parola inserita è più lunga di due caratteri
        printf("inserisci un singolo flag tra: \n -a per la stavation \n -b per evitare lo stallo ma non rilevare la starvation \n -c per evitare lo stallo e rilevare la starvation \n -n per non attivare nessuna delle modalità precedenti\n ");
        exit(0);
    }
    else
    { // Se sono presenti argomenti sulla riga di comando
        if (argv[1][0] == '-')
        { // Se l'argomento inizia con un trattino
            switch (argv[1][1])
            { // Controlla il carattere dopo il trattino
            case 'a':
                stallo_attivo = 1; // solo rilevamento dello stallo
                printf("stallo_attivo: %d\n", stallo_attivo);
                break;
            case 'b':
                soluzione_stallo_attiva = 1; // evita lo stallo ma non rileva starvation
                printf("soluzione_stallo_attiva: %d\n", soluzione_stallo_attiva);
                break;
            case 'c':
                soluzione_stallo_attiva = 1; // evita lo stallo ma non rileva starvation
                starvation_attivo = 1;
                ; // rileva starvation
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
    for (int i = 0; i < num_filo; i++)
    {
        sprintf(forks_sem_name, "/fork_sem_%d_%d", getpid(), i);
        sem_unlink(forks_sem_name);
        // printf("Semaforo %s rimosso\n", forks_sem_name);
    }


    // Inizializza le forchette
    for (int i = 0; i < num_filo; i++)
    {
        sprintf(forks_sem_name, "/fork_sem_%d_%d", getpid(), i);
        forks[i] = sem_open(forks_sem_name, O_CREAT | O_EXCL, 0644, 1);
        // printf("Semaforo %s creato\n", forks_sem_name);
        if (forks[i] == SEM_FAILED)
        {
            perror("Errore nell'inizializzazione del semaforo forks");
            return 1;
        }
    }

    // Inizializza le informazioni dei filosofi
    for (int i = 0; i < num_filo; i++)
    {
        filosofi_info[i].id = i;
        filosofi_info[i].last_meal_time = time(NULL);
    }

    pid_padre = getpid();  // Ottenere il PID del processo corrente
    printf("il pid padre è: %d\n", pid_padre);

    // Crea i processi filosofi
    pid_t pid;
    for (int i = 0; i < num_filo; i++)
    {
        // printf("il pid è: %d\n", pid);
        pid = fork();
        // printf("il pid è: %d\n", pid);

        if (pid == 0)
        { // processo figlio (filosofo)
            filosofo(i);
            exit(0);
        }
        else if (pid > 0) // Processo padre
        {                       
            // child_pids[i] = pid; // Memorizza il PID del processo figlio
            printf("Processo figlio %d creato\n", pid);
        }
        else
        {
            perror("Errore nella creazione del processo figlio");
            return 1;
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