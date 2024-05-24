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


#define NUM_FILOSOFI_MAX 10 // Numero massimo di filosofi
#define STARVATION_TIMEOUT 5 // Tempo in secondi prima che un filosofo vada in starvation


pthread_t tid; // Thread ID per il thread dei segnali SIGINT
int num_filo; ; // Numero di filosofi, si seleziona da terminale con il secondo argomento

int stallo_attivo = 0; // Flag per attivare la rilevazione dello stallo 0 (disattivato) 1 (attivato), di default è disattivato. si attiva con il flag -a da terminale
int soluzione_stallo_attiva = 0; // Flag per attivare la soluzione dello stallo 0 (disattivato) 1 (attivato), di default è disattivato si attiva con il flag -b da terminale
int starvation_attivo = 0; // Flag per attivare la rilevazione della starvation 0 (disattivato) 1 (attivato), di default è disattivato si attiva con il flag -c da terminale

volatile sig_atomic_t segnale_ricevuto = 0; // Flag per indicare se è stato ricevuto un segnale SIGINT

sem_t *sem_shared_memory; // Semaforo utilizzato per far si che i processi non accedano e scrivano contemporaneamente nella memoria condivisa
sem_t *sem_chiusura_padre; // Semaforo utilizzato per far si che il padre termini solo dopo che l'ultimo figlio ha terminato
sem_t *forks[NUM_FILOSOFI_MAX]; // Semafori per le forchette dei filosofi utilizzati per far si che due filosofi non prendano la stessa forchetta

pid_t pid_padre; // PID del processo padre 

char forks_sem_name[30]; // Nome del semaforo per le forchette. è composto da "/fork_sem_" + pid_padre + "_" + id_fork e viene assegnato ad ogni forchetta nel main 

int *termina; // Variabile condivisa utilizzata per segnalare ai processi figli che devono terminare
int *numero_processi_attivi; // Variabile condivisa per tenere traccia del numero di processi attivi

typedef struct { // Struttura per tenere traccia dell'ultimo pasto di ciascun filosofo
    int id;
    time_t last_meal_time;
} FilosofoInfo;

FilosofoInfo filosofi_info[NUM_FILOSOFI_MAX]; // Array di strutture dati (dichiarata nella riga precedente) per tenere traccia dell'ultimo pasto di ciascun filosofo


void clean_exit() // Funzione per la terminazione pulita dell'applicazione
{
    // Nella funzione clean_exit() vengono chiusi e rimossi i semafori, la memoria condivisa e il thread per i segnali

    if (getpid() != pid_padre) // Se il processo corrente non è il processo padre allora termina il processo figlio e non eseguire il codice sottostante
    {
        sem_wait(sem_shared_memory); // Acquisire il semaforo per accedere alla memoria condivisa
        *numero_processi_attivi = *numero_processi_attivi - 1; // Decrementa il numero di processi attivi 
        printf("\n");
        printf("termino il processo figlio %d\n", getpid()); // Stampa il PID del processo figlio che termina
        printf("il numero di processi attivi è: %d\n", *numero_processi_attivi + 1); // Stampa il numero di processi attivi dopo la terminazione del processo figlio (nella riga precedente)
        sem_post(sem_shared_memory); // Rilasciare il semaforo

        if (*numero_processi_attivi == 0) // Nel caso in cui sia stato appena terminato l'ultimo processo figlio
        {
            sem_post(sem_chiusura_padre); // incremeta il semaforo per far terminare il processo padre
        }
        exit(0); // Termina il processo figlio
    }


    // questa parte del codice viene eseguita solo dal processo padre in quanto i processi figli terminano prima di arrivare a questo punto

    sem_wait(sem_chiusura_padre); // Attendi che tutti i processi figli abbiano terminato in quanto fino a che ci sono processi attivi non posso chiudere il processo padre

        for (int i = 0; i < num_filo; i++) // Chiudi e rimuovi i semafori delle forchette
        {
            sprintf(forks_sem_name, "/fork_sem_%d_%d", getpid(), i); // Genera il nome del semaforo per la forchetta e lo memorizza in forks_sem_name
            sem_unlink(forks_sem_name); // Rimuovi il semaforo
        }


        // Chiudi e rimuovi la memoria condivisa "termina"
        munmap(termina, sizeof(int)); 
        shm_unlink("/termina_shm"); 

        // Chiudi e rimuovi la memoria condivisa "numero_processi_attivi"
        munmap(numero_processi_attivi, sizeof(int));
        shm_unlink("/num_proc_attivi_shm");

        // Chiudi il thread tid
        pthread_cancel(tid);

        sem_unlink("/sem_shared_data"); // Chiudi il semaforo relativo alla memoria condivisa: "sem_shared_data" 

        sem_unlink("/sem_chiusura_processo_padre"); // Chiudi il semaforo relativo alla chiusura del processo padre: "sem_chiusura_processo_padre"

        // Termina il processo corrente
        printf("\n");
        printf("termino il processo padre %d\n\n\n", getpid()); // Stampa il PID del processo padre che termina
        fflush(stdout); // Svuota il buffer di output
        usleep(100000); // Ritardo di 0.1 secondi prima di terminare il processo padre
        exit(EXIT_SUCCESS); // Termina il processo padre

}


// Funzione per gestire il segnale SIGINT (Ctrl+C) utilizzato per terminare l'applicazione in modo pulito
void sigint_handler(int sig) 
{
    clean_exit(); // chiamo la funzione clean_exit() per la terminazione pulita dell'applicazione
}

void *signal_thread(void *arg) // Funzione per gestire i segnali in un thread separato
{
    int sig; // Variabile per memorizzare il segnale ricevuto
    sigset_t set; // Insieme dei segnali da attendere
    sigemptyset(&set); // Inizializza l'insieme dei segnali vuoto
    sigaddset(&set, SIGINT); // Aggiunge SIGINT all'insieme dei segnali

    sigwait(&set, &sig); // Attendere il segnale SIGINT (Ctrl+C), altrimenti blocca il while cosi che non venga eseguito il codice sottostante e si evitano cicli infiniti
    
    segnale_ricevuto = 1; // Imposta il flag per indicare che è stato ricevuto un segnale SIGINT (Ctrl+C) che viene utilizzato per terminare l'applicazione in modo pulito
    
    fflush(stdout); // Svuota il buffer di output

    clean_exit(); // chiamo la funzione clean_exit() per la terminazione pulita dell'applicazione

    return NULL; // Termina il thread
}


void check_starvation(int id)   // Funzione per controllare se un filosofo è in starvation
{

    time_t current_time = time(NULL); // Ottiene il tempo corrente in secondi
    if ((current_time - filosofi_info[id].last_meal_time) > STARVATION_TIMEOUT) // Se il tempo trascorso dall'ultimo pasto del filosofo è maggiore del tempo massimo di starvation
    {
        if (*termina == 0){ // Se non è già stata rilevata la starvation per un altro filosofo scrive il filosofo in starvation e imposta la variabile termina a 1
            printf("Filosofo %d è in starvation\n", id);        
            *termina = 1; // Segnala ai processi figli di terminare
        }
        return;
    }
    // se arrivo qui significa che nessun filosofo è in starvation
}

void filosofo(int id) // Funzione per il comportamento di un filosofo 
{
    int sinistra = id; // salvo la forchetta a sinistra del filosofo
    int destra = (id + 1); // salvo la forchetta a destra del filosofo
    if (id + 1 == num_filo) // Se il filosofo è l'ultimo allora la forchetta a destra è la prima (ovvero la forchetta 0)    
    {
        destra = 0;
    } 

    filosofi_info[id].last_meal_time = time(NULL); // Inizializza il tempo dell'ultimo pasto del filosofo la prima volta che viene lanciato il processo

    /* 
    Ciclo while che esegue il funzionamento del filosofo fino a che questo non deve terminare, ovvero fino a che non è stato 
    ricevuto un segnale SIGINT (Ctrl+C) o è stata rilevata la starvation oppure lo stallo. 
    il SIGINT imposta la variabile segnale ricevuto == 1 cosi che il ciclo while termina
    la starvation e lo stallo imposta la variabile termina == 1 cosi che il ciclo while termina
    i printf vengono eseguiti solo se la variabile termina == 0, in modo da non stampare messaggi dopo che è stata rilevata la starvation o lo stallo
    */
    while (!segnale_ricevuto && *termina == 0) 
    {

        if (soluzione_stallo_attiva && id == num_filo - 1) { // se abbiamo attivato con il flag -b l'opzione per evitare lo stallo e il filosofo è l'ultimo, allora si scambia l'ordine in cui prende le forchette

            // Prende la forchetta a destra nel caso in cui sia libera, altrimenti attende
            sem_wait(forks[destra]); 

            if (*termina == 0){ 
                printf("Filosofo %d: Prende la forchetta destra (%d)\n", id, destra);
            }

            sleep(1); // attende un secondo per simulare il tempo necessario per prendere la seconda forchetta

            // Prende la forchetta a sinistra nel caso in cui sia libera, altrimenti attende
            sem_wait(forks[sinistra]);

            if (*termina == 0){
                printf("Filosofo %d: Prende la forchetta sinistra (%d)\n", id, sinistra);
            }

        }
        else { // se non è attiva l'opzione per evitare lo stallo procede normalmente, ovvero ogni filosofo prende prima la forchetta a sinistra e poi quella a destra
           
            // Prende la forchetta a sinistra nel caso in cui sia libera, altrimenti attende
            sem_wait(forks[sinistra]);
            
            if (*termina == 0){ 
                printf("Filosofo %d: Prende la forchetta sinistra (%d)\n", id, sinistra);
            }

            sleep(1); // attende un secondo per simulare il tempo necessario per prendere la seconda forchetta

            // Controlla se c'è uno stallo
            if (stallo_attivo) // Se è attiva l'opzione per rilevare lo stallo con il flag -a da terminale allora controlla se c'è uno stallo
            {
                int sinistra_locked = sem_trywait(forks[sinistra]); // Prova ad acquisire la forchetta a sinistra senza attendere
                int destra_locked = sem_trywait(forks[destra]); // Prova ad acquisire la forchetta a destra senza attendere

                if (sinistra_locked == 0 && destra_locked == 0) // Se entrambe le forchette sono state acquisite allora non c'è stallo
                {
                    sem_post(forks[sinistra]); // Rilascia la forchetta a sinistra ripristinando lo stato precedente
                    sem_post(forks[destra]); // Rilascia la forchetta a destra  ripristinando lo stato precedente
                }
                else
                {
                    // Almeno una delle forchette non è stata acquisita, quindi c'è stallo
                    if (*termina == 0){
                        printf("Filosofo %d è in stallo\n", id); // Stampa il messaggio di stallo
                        *termina = 1; // Segnala ai processi figli di terminare
                    }
                    break; // Termina il ciclo while
                }
            } // fine if (stallo_attivo)

            // Nel caso in cui non sia attiva l'opzione per rilevare lo stallo oppure non è stato rilevato lo stallo procede normalmente
            
            // Prende la forchetta a destra
            sem_wait(forks[destra]);
            if (*termina == 0){
                printf("Filosofo %d: Prende la forchetta destra (%d)\n", id, destra);
            }

        } 

            if (starvation_attivo) // Se è attiva l'opzione per rilevare la starvation con il flag -c da terminale allora controlla se c'è una starvation
            {
                check_starvation(id); // richiama la funzione check_starvation() per controllare se il filosofo è in starvation
            }

            // se non è stata rilevata la starvation o lo stallo oppure le due opeziomi non sono state attivate allora il filosofo mangia

            if (*termina == 0){
                printf("Filosofo %d: Mangia\n", id); // Stampa il messaggio di mangiare
            }
            sleep(1); // Simula il pasto un secondo
            filosofi_info[id].last_meal_time = time(NULL); // Aggiorna il tempo dell'ultimo pasto del filosofo

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
        int tempo_di_attesa = rand() % 100000; // Ritardo casuale fino a 1 secondo (100000 microsecondi)
        usleep(tempo_di_attesa);
    }

    /*
    Quando esco dal ciclo while nel caso un cui sia stato rilevato lo stallo 
    o la starvation oppure sia stato ricevuto un segnale SIGINT (Ctrl+C) 
    allora procedo a terminare il processo figlio
    */
    clean_exit(); //richiamo la fuzione clean_exit() per terminare il processo figlio
}

/*
Funzione main in cui si inizializzano i semafori, 
la memoria condivisa e i processi figli (filosofi)
e si avviano i processi figli (filosofi) e il thread per i segnali
*/
int main(int argc, char *argv[])
{

    // Crea il thread per gestire i segnali
    pthread_create(&tid, NULL, signal_thread, NULL);

    // Installa il gestore per SIGINT
    signal(SIGINT, sigint_handler);


    if (argc > 3 || strlen(argv[1]) != 2 ) // Se sono presenti più di tre argomenti sulla riga di comando (compreso l'avvio dello script) o il flag non è composto da due caratteri (es -a) allora stampa un messaggio di errore e termina il programma
    { 
        system("clear"); // Pulisce il terminale
        printf("inserisci un singolo flag tra: \n -a per la stavation \n -b per evitare lo stallo ma non rilevare la starvation \n -c per evitare lo stallo e rilevare la starvation \n -n per non attivare nessuna delle modalità precedenti\n succesisvaente inserisci il numero di filosofi compreso tra 3 e %d\n", NUM_FILOSOFI_MAX);
        exit(0); // nel caso in cui non siano rispettate le condizioni termina il programma
    }
    else
    { //vaso a assegnare il valore delle variabili in base ai flag passati da terminale
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
                exit(0);
                break;
            }
        }
        else // Se l'argomento non inizia con un trattino
        {
            printf("inserisci un singolo flag tra: \n -a per la stavation \n -b per evitare lo stallo ma non rilevare la starvation \n -c per evitare lo stallo e rilevare la starvation \n -n per non attivare nessuna delle modalità precedenti\n succesisvaente inserisci il numero di filosofi compreso tra 3 e %d\n", NUM_FILOSOFI_MAX);
            exit(0); // nel caso in cui non siano rispettate le condizioni termina il programma
        }

            char *endptr; // Puntatore alla fine della stringa convertita in numero intero con strtol (utilizzato per controllare se la conversione è avvenuta correttamente) presa da terminale
            int filo_selezionati = strtol(argv[2], &endptr, 10); // Converte la stringa in un numero intero
            printf("\nnumero di filosofi selezionati: %d\n\n", filo_selezionati); // Stampa il numero di filosofi selezionati

            if (filo_selezionati > 2 && filo_selezionati <= NUM_FILOSOFI_MAX) // Se il numero di filosofi selezionati è compreso tra 3 e NUM_FILOSOFI_MAX allora assegna il valore a num_filo altrimenti stampa un messaggio di errore e termina il programma
            {
                num_filo = filo_selezionati; // Assegna il numero di filosofi selezionati alla variabile num_filo utilizzata nel programma per indicare il numero di filosofi da gestire
            }
            else
            {
                printf("inserisci un singolo flag tra: \n -a per la stavation \n -b per evitare lo stallo ma non rilevare la starvation \n -c per evitare lo stallo e rilevare la starvation \n -n per non attivare nessuna delle modalità precedenti\n succesisvaente inserisci il numero di filosofi compreso tra 3 e %d\n", NUM_FILOSOFI_MAX);
                exit(0); // nel caso in cui non siano rispettate le condizioni termina il programma
            }
    }


    // Inizializza la memoria condivisa
    int shm_fd = shm_open("/termina_shm", O_CREAT | O_RDWR, 0666); // Crea la memoria condivisa per "termina"
    ftruncate(shm_fd, sizeof(int)); // Imposta la dimensione della memoria condivisa
    termina = mmap(0, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0); // Mappa la memoria condivisa
    *termina = 0; // Inizializza la variabile condivisa "termina" a 0

    // Inizializza la memoria condivisa per "numero_processi_attivi"
    int shm_fd_numero_processi_attivi = shm_open("/num_proc_attivi_shm", O_CREAT | O_RDWR, 0666); // Crea la memoria condivisa per "numero_processi_attivi"
    ftruncate(shm_fd_numero_processi_attivi, sizeof(int)); // Imposta la dimensione della memoria condivisa
    numero_processi_attivi = mmap(0, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd_numero_processi_attivi, 0); // Mappa la memoria condivisa
    *numero_processi_attivi = num_filo; // Inizializza la variabile condivisa "numero_processi_attivi" al numero di filosofi


    // Rimuove i semafori forks se presenti
    for (int i = 0; i < num_filo; i++) 
    {
        sprintf(forks_sem_name, "/fork_sem_%d_%d", getpid(), i); // Genera il nome del semaforo per la forchetta e lo memorizza in forks_sem_name
        sem_unlink(forks_sem_name); // Rimuovi il semaforo con il nome generato in forks_sem_name se è già presente
    }

    // Inizializza i semafori forks per le forchette
    for (int i = 0; i < num_filo; i++)
    {
        sprintf(forks_sem_name, "/fork_sem_%d_%d", getpid(), i); // Genera il nome del semaforo per la forchetta e lo memorizza in forks_sem_name   
        forks[i] = sem_open(forks_sem_name, O_CREAT | O_EXCL, 0644, 1); // Inizializza il semaforo con il nome generato in forks_sem_name
        if (forks[i] == SEM_FAILED) // Se la creazione del semaforo fallisce stampa un messaggio di errore e termina il programma
        {
            perror("Errore nell'inizializzazione del semaforo forks");
            return 1;
        }
    }
    

    sem_unlink("/sem_shared_data"); // Rimuove il semaforo /sem_shared_data utilizzato per la gestione della memoria condivisa se è già presente 

    // Inizializza il semaforo /sem_shared_data utilizzato per la gestione della memoria condivisa
    sem_shared_memory = sem_open("/sem_shared_data", O_CREAT | O_EXCL, 0644, 1); // Inizializza il semaforo con valore 1 e lo memorizza in sem_shared_memory con permessi di lettura e scrittura
    if (sem_shared_memory == SEM_FAILED) { // Se la creazione del semaforo fallisce stampa un messaggio di errore e termina il programma
        perror("Errore nell'inizializzazione del semaforo sem_shared_memory");
        exit(EXIT_FAILURE); 
    }


    sem_unlink("/sem_chiusura_processo_padre"); // Rimuove il semaforo /sem_chiusura_processo_padre utilizzato per far si che il processo padre termini solo dopo che l'ultimo figlio ha terminato se è già presente

    // Inizializza il semaforo /sem_chiusura_processo_padre utilizzato per far si che il processo padre termini solo dopo che l'ultimo figlio ha terminato
    sem_chiusura_padre = sem_open("/sem_chiusura_processo_padre", O_CREAT | O_EXCL, 0644, 0); // Inizializza il semaforo con valore 0 (di default è 0, verrà impostato a 1 nel momento in cui il padre dovrà terminare) e lo memorizza in sem_chiusura_padre con permessi di lettura e scrittura
    if (sem_shared_memory == SEM_FAILED) {
        perror("Errore nell'inizializzazione del semaforo sem_chiusura_padre");
        exit(EXIT_FAILURE);
    }


    // Inizializza le informazioni dei filosofi
    for (int i = 0; i < num_filo; i++)
    {
        filosofi_info[i].id = i; // Assegna l'ID del filosofo
    }

    pid_padre = getpid(); // assegna il PID del processo padre alla variabile pid_padre cosi da tenerlo in memoria, servirà per verificare che chiudano prima i processi figli e per ultimo il processo padre

    // Crea i processi filosofi
    pid_t pid; // dichiaro il PID del processo
    for (int i = 0; i < num_filo; i++) // ciclo for per creare i processi figli (filosofi)
    {
        pid = fork(); // Crea un processo filosofo figlio
        if (pid == 0) // Se il PID è 0 allora è il processo figlio
        { 
            filosofo(i); // chiama la funzione filosofo() per il comportamento del filosofo
            exit(0);
        }
        else if (pid < 0) // Processo padre
        {
            perror("Errore nella creazione del processo figlio");
            return 1; // Termina il programma con errore
        }
    }

    sem_wait(sem_chiusura_padre); // Attendi che tutti i processi figli abbiano terminato in quanto fino a che ci sono processi attivi non posso chiudere il processo padre
    sem_post(sem_chiusura_padre); // incremeta il semaforo appena decrementato così da rimpostarlo ad uno stato di chiusura (utile per quando vado a chiamare la funzione clean_exit() che deve sapere che il processo padre può terminare)

    clean_exit();// chiamo la funzione clean_exit() per la terminazione pulita dell'applicazione

    pthread_join(tid, NULL); // Attendi la terminazione del thread per i segnali, ma non raggiunto perché clean_exit() termina il programma e quindi anche il thread

    return 0; // Non raggiunto poiché clean_exit() termina il programma
}
