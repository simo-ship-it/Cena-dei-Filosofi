#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

// Flag per segnalare se l'applicazione deve terminare
int termina = 0;

// Funzione per gestire il segnale SIGINT (Ctrl+C)
void sigint_handler(int sig) {
    printf("\nRicevuto SIGINT. Terminazione dell'applicazione.\n");
    termina = 1; // Imposta il flag di terminazione
}

// Funzione per la terminazione pulita dell'applicazione
void clean_exit() {
    printf("Terminazione dell'applicazione.\n");
    // Qui liberare eventuali risorse allocate
    // Chiudere file descriptor aperti, liberare memoria allocata, ecc.
    // ...

    // Notifica all'utente che l'applicazione sta terminando
    printf("L'applicazione sta terminando...\n");

    // Termina il processo corrente
    exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[]) {
    // Installa il gestore per SIGINT
    signal(SIGINT, sigint_handler);

    // Esegui il programma
    while (!termina) {
        // Simula un lavoro
        printf("Lavoro in corso...\n");
        sleep(3); // Simula l'attesa di qualche secondo
    }

    // Termina l'applicazione in modo pulito
    clean_exit();

    return 0; // Non raggiunto poiché clean_exit() termina il programma
}