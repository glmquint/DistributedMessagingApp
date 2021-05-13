#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include "vector.h"
#include "network.h"

#define DEBUG_ON

#ifdef DEBUG_ON
# define DEBUG_PRINT(x) printf x
#else
# define DEBUG_PRINT(x) do {} while (0)
#endif

struct Peer{
    int port;
    cvector neighbors;
    vector pending_requests;
}peer;

void initialize(char* port)
{
    sscanf(port, "%d", &peer.port);
    DEBUG_PRINT(("sono peer: %d\n", peer.port));
    cvector_init(&(peer.neighbors));
    vector_init(&(peer.pending_requests));
}

void boot(char* ds_addr, int ds_port)
{
}

void menu()
{
    printf("\nhelp --> mostra i dettagli sui comandi accettati\n\n");
    printf("start DS_addr DS_port --> richiede al DS,in ascolto all’indirizzo DS_addr:DS_port, la connessione al network. Il DS registra il peer e gli invia la lista di neighbor. Se il peer è il primo a connettersi, la lista sarà vuota. Il DS avrà cura di integrare il peer nel network a mano a mano che altri peer invocheranno la star\n\n");
    printf("add type quantity --> aggiunge al register della data corrente l’evento type con quantità quantity. Questo comando provoca la memorizzazione di una nuova entry nel peer\n\n");
    printf("get aggr type period --> effettua una richiesta di elaborazione per ottenere il dato aggregato aggr sui dati relativi a un lasso temporale d’interesse period sulle entry di tipo type. Considerando tali dati su scala giornaliera. Le aggregazioni aggr calcolabili sono il totale e la variazione. Il parametro period è espresso come ‘dd1:mm1:yyyy1-dd2:mm2:yyyy2’, dove con 1 e 2 si indicano, rispettivamente, la data più remota e più recente del lasso temporale d’interesse. Una delle due date può valere ‘*’. Se è la prima, non esiste una data che fa da lower bound. Viceversa, non c’è l’upper bound e il calcolo va fatto fino alla data più recente (con register chiusi). Il parametro period puòmancare, in quel caso il calcolo si fa su tutti i dati. \n\n");
    printf("stop --> il peer richiede una disconnessione dal network. Il comando stop provoca l’invio ai neighbordi tutte le entry registrate dall’avvio. Il peer contatta poi il DS per comunicare la volontà di disconnettersi. Il DS aggiorna i registri di prossimità rimuovendo il peer. Se la rimozione del peer causa l’isolamento di una un’interaparte del network, il DS modifica i registri di prossimitàdi alcuni peer, affinché questo non avvenga\n\n");
}

void handleCmd(char* cmd)
{
    char ds_addr[20], type[20], aggr[20], period[50], tmp[20];
    int ds_port, quantity;

    DEBUG_PRINT(("handling: %s\n", cmd));
    sscanf(cmd, "%s", tmp);
    printf("\n");
    if (strcmp("help", tmp) == 0)
    {
        menu();
    }
    else if (strcmp("start", tmp) == 0)
    {
        if (sscanf(cmd, "%s %s %d", tmp, ds_addr, &ds_port) == 3)
        {
            DEBUG_PRINT(("server boot: %s %d\n\n", ds_addr, ds_port));
            boot(ds_addr, ds_port);
        }
        else
        {
            printf("formato errato per il comando start\n\n");
        }
    }
    else if (strcmp("add", tmp) == 0)
    {
        if (sscanf(cmd, "%s %s", tmp, type) == 2)
        {
            if (strcmp(type, "tampone") == 0)
            {
                if (sscanf(cmd, "%s %s %d", tmp, type, &quantity) == 3)
                {
                    DEBUG_PRINT(("add tampone: %s %d\n\n", type, quantity));
                }
                else
                    printf("formato non valido per il comando add tampone\n\n");
            }
            else if (strcmp(type, "nuovo") == 0)
            {
                if (sscanf(cmd, "%s %s %*s %d", tmp, type, &quantity) == 3)
                {
                    DEBUG_PRINT(("add nuovo: %s %d\n\n", type, quantity));
                }
                else
                    printf("formato non valido per il comando add nuovo\n\n");
            }
            else
                printf("Tipo non valido per il comando add\n\n");
        }
        else
            printf("formato non valido per il comando add\n\n");
    }
    else if (strcmp("get", tmp) == 0)
    {
        if (sscanf(cmd, "%s %s %s", tmp, aggr, type) == 3)
        {
            if (strcmp(type, "tampone") == 0)
            {
                if (sscanf(cmd, "%s %s %s %s", tmp, aggr, type, period) == 4)
                {
                    DEBUG_PRINT(("get tampone: %s %s %s\n\n", aggr, type, period));
                }
                else if (sscanf(cmd, "%s %s %s %s", tmp, aggr, type, period) == 3)
                {
                    strcpy(period, " ");
                    printf("get tampone 2: %s %s %s\n\n", aggr, type, period);
                }
                else
                    DEBUG_PRINT(("formato non valido per il comando get tampone\n\n"));
            }
            else if (strcmp(type, "nuovo") == 0)
            {
                if (sscanf(cmd, "%s %s %s %*s %s", tmp, aggr, type, period) == 4)
                {
                    DEBUG_PRINT(("get nuovo: %s %s %s\n\n", aggr, type, period));
                }
                else if (sscanf(cmd, "%s %s %s %*s %s", tmp, aggr, type, period) == 3)
                {
                    strcpy(period, " ");
                    DEBUG_PRINT(("get nuovo 2: %s %s %s\n\n", aggr, type, period));
                }
                else
                    printf("formato non valido per il comando get nuovo\n\n");
            }
            else
                printf("Tipo non valido per il comando get\n\n");
        }
        else
            printf("formato non valido per il comando get\n\n");
    }
    else if (strcmp("stop", tmp) == 0)
    {
        DEBUG_PRINT(("stop!!\n\n"));
    }
    else
    {
        printf("comando non valido\n\n");
    }
}

// will never be called if use_udp is set to false in IOMultiplexer
void emptyFunc(int sd, char* cmd)
{
    return;
}

void handleReq(int sd, char* cmd)
{
    #ifdef DEBUG_ON
    DEBUG_PRINT(("handling req from %d: %s\n", sd, cmd));
    sleep(5);
    DEBUG_PRINT(("done with request\n"));
    #endif
}

void handleTimeout()
{
    return;
    /*
    time_t mytime = time(NULL);
    char * time_str = ctime(&mytime);
    time_str[strlen(time_str)-1] = '\0';
    printf("\rCurrent Time : %s\t> ", time_str);
    fflush(stdout);
    */
}

int main(int argc, char *argv[])
{
    initialize(argv[1]);
    menu();
    fflush(stdout);
    IOMultiplex(peer.port, false, handleCmd, emptyFunc, handleReq, handleTimeout);
}
