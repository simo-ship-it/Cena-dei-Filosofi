#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>

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

	int stallo = 0; // Flag per l'opzione 'a'
    int evita_stallo_ma_non_stavation = 0; // Flag per l'opzione 'a'
    int evita_stallo_e_evita_stavation = 0; // Flag per l'opzione 'b'

    // Esegui il programma
    while (!termina) {




if (argc > 2 || strlen(argv[1]) > 2) { // Se sono presenti argomenti sulla riga di comando o La parola inserita è più lunga di due caratteri
	printf("inserisci un singolo flag tra: \n -a per la stavation \n -b per evitare lo stallo ma non rilevare la starvation \n -c per evitare lo stallo e rilevare la starvation");
}
else { // Se sono presenti argomenti sulla riga di comando
        if (argv[1][0] == '-') { // Se l'argomento inizia con un trattino
            switch (argv[1][1]) { // Controlla il carattere dopo il trattino
                case 'a':
                    stallo = 1; // solo rilevamento dello stallo 
                    break;
                case 'b':
                    evita_stallo_ma_non_stavation = 1; // evita lo stallo ma non rileva starvation
                    break;
                case 'c':
                    evita_stallo_e_evita_stavation = 1; // evita lo stallo e rileva starvation
                    break;
                default:
                    printf("Opzione non riconosciuta: %s\n", argv[1]);
                    break;
            }
        }

}

    // Esempio di utilizzo delle opzioni
    if (stallo) {
        printf("Opzione 'rileva stallo' abilitata.\n");
    }
    if (evita_stallo_ma_non_stavation) {
        printf("Opzione 'evita lo stallo ma non rileva starvation' abilitata.\n");
    }
    if (evita_stallo_e_evita_stavation) {
        printf("Opzione 'evita lo stallo e rileva starvation' abilitata.\n");
    }
	
        printf("Lavoro in corso...\n");
        sleep(3); // Simula l'attesa di qualche secondo
    }

    // Termina l'applicazione in modo pulito
    clean_exit();

    return 0; // Non raggiunto poiché clean_exit() termina il programma
}