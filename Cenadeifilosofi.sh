#!/bin/bash

# quando si avvia il file cenadeifilosofis.sh va specificato il flag 
# "a" per avviare il rilevamento dello stallo
# "b" per evitare lo stallo ma non rilevare starvation
# "c" per evitare lo stallo e rilevare starvation
clear
echo -e "\n\n\033[92mCompilazione Parent\033[0m\n"



# Verifica se è stato fornito almeno un argomento
# if [ $# -eq 0 ]; then
#     echo "Usage: $0 <flags>"
#     exit 1
# fi

# Concatena gli argomenti forniti in una variabile
FLAGS="$@"

# Esegui il comando make run con i flag specificati
make run FLAGS="$FLAGS"

make clean

echo -e "\n\n\033[92mAvvio Parent\033[0m\n"

./Parent

echo -e "\n\033[92mTerminazione Parent..\033[0m\n\n"


make aClean