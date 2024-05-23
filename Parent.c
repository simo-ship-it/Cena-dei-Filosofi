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
#include <sys/mman.h>
#include <fcntl.h>

#define num_filo 5
#define NUM_FILOSOFI_MAX 100
#define STARVATION_TIMEOUT 3 // Tempo in secondi prima che un filosofo vada in starvation

sem_t *forks[num_filo];

pthread_t tid;

int stallo_attivo = 0;
int soluzione_stallo_attiva = 0;
int starvation_attivo = 0;
volatile sig_atomic_t segnale_ricevuto = 0;

// pid_t child_pids[NUM_FILOSOFI_MAX];

sem_t *sem_shared_memory; // Semaforo per la memoria condivisa

pid_t pid_padre;

char forks_sem_name[30];

int *termina; // Variabile condivisa

int *numero_processi_attivi; // Variabile condivisa per tenere traccia del numero di processi attivi

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

    if (getpid() != pid_padre)
    {

        sem_wait(sem_shared_memory); // Acquisire il semafor
        *numero_processi_attivi = *numero_processi_attivi - 1; // Decrementa il numero di processi attivi
        printf("\n");
        printf("termino il processo figlio %d\n", getpid());
        printf("il numero di processi attivi è: %d\n", *numero_processi_attivi + 1);
        sem_post(sem_shared_memory); // Rilasciare il semaforo
        exit(0);
    }

    while (1)
    {

        if (*numero_processi_attivi == 0)
        { // fino a che ci sono processi attivi non posso chiudere il processo padre

            // posso terminare il processo padre che deve essere lultimo a chiudersi

            // Chiudi i semafori forks
            for (int i = 0; i < num_filo; i++)
            {
                sprintf(forks_sem_name, "/fork_sem_%d_%d", getpid(), i);
                sem_unlink(forks_sem_name);
                // printf("Semaforo %s rimosso\n", forks_sem_name);
            }


            // Chiudi e rimuovi la memoria condivisa
            munmap(termina, sizeof(int));
            shm_unlink("/termina_shm");

            munmap(numero_processi_attivi, sizeof(int));
            shm_unlink("/num_proc_attivi_shm");

            // Chiudi il thread tid
            pthread_cancel(tid);

            sem_unlink("/sem_shared_data"); // Chiudi il semaforo della memoria condivisa 

            // Termina il processo corrente
            // sleep(1);
            printf("\n");
            printf("termino il processo padre %d\n\n\n", getpid());
            exit(EXIT_SUCCESS);
        }
        usleep(100000); // Ritardo di 0.1 secondi prima di riprovare a chiudere il processo padre
    }
}

// Funzione per gestire il segnale SIGINT (Ctrl+C)
void sigint_handler(int sig)
{
    // printf("\nRicevuto SIGINT. Terminazione dell'applicazione.\n");
    clean_exit(); // chiamo la funzione clean_exit() per la terminazione pulita dell'applicazione
}

void *signal_thread(void *arg) // Funzione per gestire i segnali in un thread separato
{
    int sig;
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);

    while (1)
    { // Ciclo infinito per attendere i segnali e terminare l'applicazione in modo pulito
        sigwait(&set, &sig);
        segnale_ricevuto = 1;

        fflush(stdout); // Svuota il buffer di output

        clean_exit(); // chiamo la funzione clean_exit() per la terminazione pulita dell'applicazione

        return NULL;
    }
}

// Funzione per controllare se un filosofo è in starvation
void check_starvation(int id)
{

    time_t current_time = time(NULL);
    if ((current_time - filosofi_info[id].last_meal_time) > STARVATION_TIMEOUT)
    {
        if (*termina == 0){ // Se non è già stata rilevata la starvation per un altro filosofo scrive il filosofo in starvation e imposta la variabile termina a 1
            printf("Filosofo %d è in starvation\n", id);        
            *termina = 1; // Segnala ai processi figli di terminare
        }
        return;
    }
    // Nessun filosofo è in starvation
}

void filosofo(int id) // Funzione per il comportamento di un filosofo 
{

    int sinistra = id;
    int destra = (id + 1) % NUM_FILOSOFI_MAX;
    if (id + 1 == num_filo)
    {
        destra = 0;
    }

    // Aggiorna il tempo dell'ultimo pasto del filosofo
    filosofi_info[id].last_meal_time = time(NULL);

    while (!segnale_ricevuto && *termina == 0) 
    {




        if (soluzione_stallo_attiva && id == num_filo - 1)
        {   // se abbiamo attivato con il flag -b l'opzione per evitare lo stallo e il filosofo è l'ultimo allora scamambia l'ordine in cui prende le forchette
            // Se tutti i filosofi hanno preso la forchetta sinistra, verifichiamo lo stato di stallo

            // Prende la forchetta a destra
            sem_wait(forks[destra]);

            if (*termina == 0){
                printf("Filosofo %d: Prende la forchetta destra (%d)\n", id, destra);
            }

            sleep(1); // attende un secondo

            // Prende la forchetta a sinistra
            sem_wait(forks[sinistra]);

            if (*termina == 0){
                printf("Filosofo %d: Prende la forchetta sinistra (%d)\n", id, sinistra);
            }
            
        }
        else
        { // se non è attiva l'opzione per evitare lo stallo procede normalmente, ovvero ogni filosofo prende prima la forchetta a sinistra e poi quella a destra
            // Prende la forchetta a sinistra
            sem_wait(forks[sinistra]);
            if (*termina == 0){
                printf("Filosofo %d: Prende la forchetta sinistra (%d)\n", id, sinistra);
            }

            sleep(1); // attende un secondo

            

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
                    if (*termina == 0){
                        printf("Filosofo %d è in stallo\n", id);
                        *termina = 1; // Segnala ai processi figli di terminare
                    }
                    break;
            }
        }
            // Prende la forchetta a destra
            sem_wait(forks[destra]);
            if (*termina == 0){
                printf("Filosofo %d: Prende la forchetta destra (%d)\n", id, destra);
            }

        }

            // Controlla se è il filosofo destinato ad andare in starvation
            if (starvation_attivo)
            {
                // printf("il filosofo %d è in attesa di blabla\n", id);
                check_starvation(id);
            }

            // Mangia
            if (*termina == 0){
                printf("Filosofo %d: Mangia\n", id);
            }
            sleep(1); // Simula il pasto un secondo

    
            // Aggiorna il tempo dell'ultimo pasto del filosofo
            filosofi_info[id].last_meal_time = time(NULL);

            // Rilascia la forchetta a sinistra
            sem_post(forks[sinistra]);
            if (*termina == 0){
                printf("Filosofo %d: Rilascia la forchetta sinistra (%d)\n", id, sinistra);
            }

            // Rilascia la forchetta a destra
            sem_post(forks[destra]);
            if (*termina == 0){
            printf("Filosofo %d: Rilascia la forchetta destra (%d)\n", id, destra);
            }

        // Introduce un ritardo casuale prima della prossima azione
        int tempo_di_attesa = rand() % 100000; // Ritardo casuale fino a 1 secondo
        // int tempo_di_attesa = 1000000; // Ritardo casuale fino a 1 secondo
        usleep(tempo_di_attesa);
    }

    //richiamo la fuzione clean_exit() per terminare il processo figlio
    clean_exit();
}

int main(int argc, char *argv[])
{

    // Inizializza la memoria condivisa
    int shm_fd = shm_open("/termina_shm", O_CREAT | O_RDWR, 0666);
    ftruncate(shm_fd, sizeof(int));
    termina = mmap(0, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    *termina = 0;

    // Inizializza la memoria condivisa per "numero_processi_attivi"
    int shm_fd_numero_processi_attivi = shm_open("/num_proc_attivi_shm", O_CREAT | O_RDWR, 0666);
    ftruncate(shm_fd_numero_processi_attivi, sizeof(int));
    numero_processi_attivi = mmap(0, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd_numero_processi_attivi, 0);
    *numero_processi_attivi = num_filo; // Inizializziamo il numero di processi attivi a 0


    // Crea il thread per gestire i segnali
    pthread_create(&tid, NULL, signal_thread, NULL);

    // Installa il gestore per SIGINT
    signal(SIGINT, sigint_handler);


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
                starvation_attivo = 1; // rileva starvation
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
    }

    // Inizializza i semafori forks per le forchette
    for (int i = 0; i < num_filo; i++)
    {
        sprintf(forks_sem_name, "/fork_sem_%d_%d", getpid(), i);
        forks[i] = sem_open(forks_sem_name, O_CREAT | O_EXCL, 0644, 1);
        if (forks[i] == SEM_FAILED)
        {
            perror("Errore nell'inizializzazione del semaforo forks");
            return 1;
        }
    }
    
    // sem_close(sem_shared_memory);
    sem_unlink("/sem_shared_data");

    // Inizializzare il semaforo della memoria condivisa
    sem_shared_memory = sem_open("/sem_shared_data", O_CREAT | O_EXCL, 0644, 1);
    if (sem_shared_memory == SEM_FAILED) {
        printf("Errore nell'inizializzazione del semaforo della memoria condivisa\n");
        perror("sem_open");
        exit(EXIT_FAILURE);
    }


    // Inizializza le informazioni dei filosofi
    for (int i = 0; i < num_filo; i++)
    {
        filosofi_info[i].id = i;
        filosofi_info[i].last_meal_time = time(NULL);
    }

    pid_padre = getpid(); // Ottenere il PID del processo corrente

    // Crea i processi filosofi
    pid_t pid;
    for (int i = 0; i < num_filo; i++)
    {
        pid = fork();

        if (pid == 0)
        { // processo figlio (filosofo)
            filosofo(i);
            exit(0);
        }
        else if (pid > 0) // Processo padre
        {
        }
        else
        {
            perror("Errore nella creazione del processo figlio");
            return 1;
        }
    }


    while (*termina == 0) // fino a che sono presenti processi figli il processo padre resta attivo in attesa
    {
        sleep(1);
    }


    clean_exit();// chiamo la funzione clean_exit() per la terminazione pulita dell'applicazione

    pthread_join(tid, NULL); // Attendi la terminazione del thread per i segnali, ma non raggiunto perché clean_exit() termina il programma

    return 0; // Non raggiunto poiché clean_exit() termina il programma
}