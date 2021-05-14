#include "Include.h"

int main(int argc, char *argv[])
{
    sscanf(argv[1], "%d", &IOController.porta);
    FileManager_caricaRegistro();
    FileManager_caricaArchivioAggregazioni();

    printf("******************** PEER COVID STARTED ********************\n");
    printf("Digita un comando:\n\n");
    printf("1) help --> mostra i dettagli dei comandi\n");
    printf("2) start DS_addr DS_port --> richiede al DS, in ascolto all'indirizzo DS_addr:DS_port, la connessione al network\n");
    printf("3) add type quantity --> aggiunge al register della data corrente l'evento type con quantità quantity\n");
    printf("4) get aggr type period --> effettua una richiesta di elaborazione per ottenere il dato aggregato aggr sui dati relativi a un lasso temporale d'interesse period sulle entry di tipo type\n");
    printf("5) stop --> il peer richiede una disconnessione dal network\n\n");
    IOController_gestisciIO();
    return 0;
}