#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <stdbool.h>
#include "IOMultiplex.h"
#include "vector.h"
#include "network.h"
// #include "Include.h"

#define DEBUG_ON

#ifdef DEBUG_ON
# define DEBUG_PRINT(x) printf x
#else
# define DEBUG_PRINT(x) do {} while (0)
#endif

#define SCREEN_PRINT(x) printf("\r"); printf x; printf("\n>> "); fflush(stdout);

#define BOOT_MSG 6            /*"10000\0"*/
#define BOOT_RESP 1024          /*can't say for sure, but this should be safe*/
#define TIMEOUT 1              /*tempo che il peer aspetta per ricevere la risposta al boot dal server*/
                                    // se si usa usleep(), indica i microsecondi

struct Period
{
    struct tm first_date;
    struct tm last_date;
};

struct Entry
{
    int year;
    int month;
    int day;
    int nuovi_casi;
    int nuovi_tamponi;

    struct Entry* next;
};

struct sumAggregation
{
    char question[40]; // len("get sum tampone 11:11:2020-22:22:2020") + 1 == 38
    int sum_answer;

    struct sumAggregation* next;
};

struct varAggregation
{
    char question[40]; // len("get sum tampone 11:11:2020-22:22:2020") + 1 == 38
    cvector var_answer;

    struct varAggregation* next;
};

struct Peer
{
    int port;
    vector flood_requests; // (str)id: initiator_peer.port (+) request_timestamp
    cvector neighbors;
    int ds_port;
    char ds_ip[20];

    struct Entry* Register;
    struct Entry* currentEntry;
    struct sumAggregation* sumAggregationList;
    struct varAggregation* varAggregationList;

} Peer;

void Register_init()
{
    time_t rawtime;
    struct tm* timeinfo;
    // FileManager_caricaRegistro();
    void Register_load(); // FIXUP: implement
    void Register_save(); // FIXUP: implement 
    Peer.Register = NULL;


    Peer.currentEntry = malloc(sizeof(struct Entry));
    time (&rawtime);
    timeinfo = localtime(&rawtime);
    Peer.currentEntry->day = timeinfo->tm_mday;
    Peer.currentEntry->month = timeinfo->tm_mon;
    Peer.currentEntry->year = timeinfo->tm_year;
    Peer.currentEntry->nuovi_casi = 0;
    Peer.currentEntry->nuovi_tamponi = 0;
    Peer.currentEntry->next = Peer.Register; // inserimento in testa

    // FileManager_caricaArchivioAggregazioni();
    Peer.sumAggregationList = NULL;
    Peer.varAggregationList = NULL;
}

void Register_updateCurrentEntry(char type[20], int quantity)
{
    if (strcmp(type, "nuovo") == 0)
        Peer.currentEntry->nuovi_casi += quantity;
    else if (strcmp(type, "tampone") == 0)
        Peer.currentEntry->nuovi_tamponi += quantity;
    else
        DEBUG_PRINT(("type %s errato", type));
}

void Register_load() 
{
    SCREEN_PRINT(("loading from ./register%d", Peer.port));
    SCREEN_PRINT(("loading from ./aggregations%d", Peer.port));
}

void Register_save() 
{
    SCREEN_PRINT(("saving into ./register%d", Peer.port));
    SCREEN_PRINT(("saving into ./aggregations%d", Peer.port));
}


void Peer_init(char* port)
{
    sscanf(port, "%d", &Peer.port);
    DEBUG_PRINT(("sono peer: %d\n", Peer.port));
    cvector_init(&(Peer.neighbors));
    vector_init(&(Peer.flood_requests));

    Register_init();

}

//funzione che stampa nel dettaglio le operazioni del peer
void Peer_help()
{
    SCREEN_PRINT(("\nhelp --> mostra i dettagli sui comandi accettati\n\n"));
    SCREEN_PRINT(("start DS_addr DS_port --> richiede al DS,in ascolto all’indirizzo DS_addr:DS_port, la connessione al network. Il DS registra il peer e gli invia la lista di neighbor. Se il peer è il primo a connettersi, la lista sarà vuota. Il DS avrà cura di integrare il peer nel network a mano a mano che altri peer invocheranno la star\n\n"));
    SCREEN_PRINT(("add type quantity --> aggiunge al register della data corrente l’evento type con quantità quantity. Questo comando provoca la memorizzazione di una nuova entry nel peer\n\n"));
    SCREEN_PRINT(("get aggr type period --> effettua una richiesta di elaborazione per ottenere il dato aggregato aggr sui dati relativi a un lasso temporale d’interesse period sulle entry di tipo type. Considerando tali dati su scala giornaliera. Le aggregazioni aggr calcolabili sono il totale e la variazione. Il parametro period è espresso come ‘dd1:mm1:yyyy1-dd2:mm2:yyyy2’, dove con 1 e 2 si indicano, rispettivamente, la data più remota e più recente del lasso temporale d’interesse. Una delle due date può valere ‘*’. Se è la prima, non esiste una data che fa da lower bound. Viceversa, non c’è l’upper bound e il calcolo va fatto fino alla data più recente (con register chiusi). Il parametro period puòmancare, in quel caso il calcolo si fa su tutti i dati. \n\n"));
    SCREEN_PRINT(("stop --> il peer richiede una disconnessione dal network. Il comando stop provoca l’invio ai neighbordi tutte le entry registrate dall’avvio. Il peer contatta poi il DS per comunicare la volontà di disconnettersi. Il DS aggiorna i registri di prossimità rimuovendo il peer. Se la rimozione del peer causa l’isolamento di una un’interaparte del network, il DS modifica i registri di prossimitàdi alcuni peer, affinché questo non avvenga\n\n"));
}

// funzione di utility per la fase di sviluppo. Da attivare tramite il simbolo DEBUG_ON
void Peer_showNeighbors()
{
    #ifdef DEBUG_ON
    SCREEN_PRINT(("\nshowing peer: %d\nHis neighbors are:\n", Peer.port));
    for (int i = 0; i < CVECTOR_TOTAL((Peer.neighbors)); i++) {
        SCREEN_PRINT(("[%d] %d\n", i, CVECTOR_GET((Peer.neighbors), int, i)));
    }
    #else
    SCREEN_PRINT(("Opzione non valida (Are you developer?)\n\n"));
    #endif
}

//funzione che si occupa di effettura il boot del Peer inviando un messaggio UDP al Ds
void Peer_serverBoot(char *ds_addr, int ds_port)
{
    //faccio il boot
    int ret, sd, num, len, i = 0;
    uint16_t lmsg;
    socklen_t addrlen;
    struct sockaddr_in srv_addr;
    char buffer[BOOT_RESP];
    char neighbors_list[1024];
    char msg[BOOT_MSG], tmp[10];
    char* nuovo_vicino;
    const char delim[2] = ":";

    struct timeval tv;
    fd_set read_fd_boot;

    sprintf(tmp, "%d", Peer.port);
    strcpy(msg, tmp);

    memset(neighbors_list, 0, sizeof(neighbors_list));
    memset(buffer, 0, sizeof(buffer));

    strcpy(Peer.ds_ip, ds_addr);
    Peer.ds_port = ds_port;

    sd = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, 0);

    memset(&srv_addr, 0, sizeof(srv_addr));
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(ds_port);
    inet_pton(AF_INET, ds_addr, &srv_addr.sin_addr);

    addrlen = sizeof(srv_addr);

    // il timeout di boot viene verificato tramite una select
    do
    {
        FD_ZERO(&read_fd_boot);
        FD_SET(sd, &read_fd_boot);

        tv.tv_sec = TIMEOUT;
        tv.tv_usec = 0;
        SCREEN_PRINT(("Invio il messaggio di boot\n"));
        ret = sendto(sd, msg, BOOT_MSG, 0, (struct sockaddr *)&srv_addr, sizeof(srv_addr));
        if (ret < 0) {
            perror("Errore nell'invio: ");
            exit(1);
        }

        ret = select(sd + 1, &read_fd_boot, NULL, NULL, &tv);
        DEBUG_PRINT(("select returned: %d\n", ret));

    } while (ret <= 0); // è presente un solo socket, quindi non è necessario verificare anche FD_ISSET(sd, &read_fds)
    ret = recvfrom(sd, (void*) &lmsg, sizeof(uint16_t), 0, (struct sockaddr *)&srv_addr, &addrlen);

    SCREEN_PRINT(("Richiesta di boot accettata\n"));
    len = ntohs(lmsg);
    DEBUG_PRINT(("len is: %d\n", len));
    ret = recvfrom(sd, (void*)buffer, len, 0, (struct sockaddr *)&srv_addr, &addrlen);
    if (ret < 0 || ret < len) {
        perror("Errore in fase di ricezione lunghezza messaggio: ");
        DEBUG_PRINT(("ret: %d", ret));
        exit(1);
    }
    DEBUG_PRINT(("buffer recieved: %s\n", buffer));
    sscanf(buffer, "%d %s", &num, neighbors_list);

    SCREEN_PRINT(("Ricevuti i %d nuovi vicini: %s\n", num, neighbors_list));

    // split della stringa dei nuovi vicini tramite delimitatore ':'
    nuovo_vicino = strtok(neighbors_list, delim);
    while (nuovo_vicino != NULL && i < num) {
        // DEBUG_PRINT(("%s\n", nuovo_vicino));
        CVECTOR_ADD((Peer.neighbors), atoi(nuovo_vicino));
        nuovo_vicino = strtok(NULL, delim);
        i++;
    }
    if (i < num)
        DEBUG_PRINT(("ricevuti meno vicini di quanto dichiarato: %d invece che %d\n", i, num));
    close(sd);
}

void Peer_networkDisconnect()
{
    int sd, ret;
    uint16_t lmsg;
    struct sockaddr_in ds_addr;
    char buffer[1024];

    sd = socket(AF_INET, SOCK_STREAM, 0);

    memset(&ds_addr, 0, sizeof(ds_addr));
    ds_addr.sin_family = AF_INET;
    ds_addr.sin_port = htons(Peer.ds_port);
    inet_pton(AF_INET, Peer.ds_ip, &ds_addr.sin_addr);

    SCREEN_PRINT(("Invio al Ds la richiesta di disconnessione\n"));
    ret = connect(sd, (struct sockaddr *)&ds_addr, sizeof(ds_addr));
    if (ret < 0) {
        perror("Errore in fase di connessione: ");
        exit(-1);
    }

    strcpy(buffer, "RSTOP");

    ret = send(sd, (void *)buffer, REQ_LEN, 0);
    if (ret < REQ_LEN) {
        perror("Errore in fase di invio: ");
        exit(-1);
    }

    lmsg = htons(Peer.port);
    ret = send(sd, (void *)&lmsg, sizeof(uint16_t), 0);
    if (ret < sizeof(uint16_t)) {
        perror("Errore in fase di invio: ");
        exit(-1);
    }
    ret = recv(sd, (void*)buffer, 4, 0); // ACK . Meglio attendere che il ds abbia correttamente modificato la struttura dati di vicinanza
    if (ret < 4) {
        perror("Errore in fase di ricezione: ");
        exit(1);
    }
    close(sd);

}

//funzione che gestisce la ricezione di un messaggio di stop da parte di un neighbor peer
void Peer_handleNeighborDisconnect(int sd)
{
    SCREEN_PRINT(("Ho ricevuto una richiesta di stop da parte di un vicino\n"));
//     RegistroGenerale_ricevoRegistriRichiesti(sd);                          //ricevo i registri dal peer che si sta disconettendo
//     RegistroGenerale_modificoIlRegistroGeneraleInBaseAiRegistriRicevuti(); //me li salvo nel RegistroGenerale

    close(sd);
    FD_CLR(sd, &iom.master);
}

//funzione che gestisce il comando di stop
void Peer_stop()
{
    int sd, ret, cont = 0;
    struct sockaddr_in peer_addr;
//     struct RegistroGiornaliero *pp;
    char buffer[1024];


    if (CVECTOR_TOTAL((Peer.neighbors)) > 0)
    {
        int vicino = CVECTOR_GET((Peer.neighbors), int, 0); // il primo vicino è sicuramente presente e sarà colui che riceverà il mio registro generale
        sd = socket(AF_INET, SOCK_STREAM, 0);

        memset(&peer_addr, 0, sizeof(peer_addr));
        peer_addr.sin_family = AF_INET;
        peer_addr.sin_port = htons(vicino);
        inet_pton(AF_INET, "127.0.0.1", &peer_addr.sin_addr);

        SCREEN_PRINT(("Invio al neighbor Peer %d i miei registri\n", vicino));
        ret = connect(sd, (struct sockaddr *)&peer_addr, sizeof(peer_addr));
        if (ret < 0)
        {
            perror("Errore in fase di connessione: ");
            exit(-1);
        }

        strcpy(buffer, "VSTOP");

        ret = send(sd, (void *)buffer, REQ_LEN, 0);
        if (ret < 0)
        {
            perror("Errore in fase di invio: ");
            exit(-1);
        }
//         //visto che devo inviare tutto il mio RegistroGenerale ad un mio vicino usando la funzione invioRegistriRichiesti devo inizializzare
        //correttamente la lista dei registri_richiesti e il numero di registri_richiesti
//         RegistroGenerale.lista_registri_richiesti = RegistroGenerale.lista_registri;
//         pp = RegistroGenerale.lista_registri;
        /*
        while (pp != 0)
        {
            cont++;
            // pp = pp->next_registro;
        }
        */
//         RegistroGenerale.numero_registri_richiesti = cont;
//         RegistroGenerale_invioRegistriRichiesti(sd);
        //recv(sd, buffer, 1, 0); // ACK da parte del Ds per potersi disconnettere
                                // necessario affinché la struttura dati di vicinanza tenuta dal DS
                                // venga aggiornata correttamente
        //DEBUG_PRINT(("ho finalmente ricevuto l'ack\n"));
        close(sd);
    }

    Peer_networkDisconnect();
//     // FileManager_salvaArchivioAggregazioni();
    // FileManager_salvaRegistro(0);
    //Let the OS free all used memory...
    exit(0);
}

// controlla se il formato del periodo è corretto
// Nel caso sia presente il simbolo *, questo viene trasformato in date valide,
// ossia l'epoch, oppure l'ultimo registro chiuso (che è quello di ieri)
struct Period* Register_formatPeriod(char period[50])
{
    SCREEN_PRINT(("check del periodo inserito"));
    DEBUG_PRINT(("periodo inserito: %s\n", period));
    char* date_delim = "-";
    char* dmy_delim = ":";
    char buffer[50];
    char *first_date, *last_date, *dmy, tmp[50];
    time_t now;
    struct tm  *yesterday;  
    struct Period* p = malloc(sizeof(struct Period));
    memset(p, 0, sizeof(struct Period));


    now = time(NULL);
    yesterday = localtime(&now);
    yesterday->tm_mday--;
    mktime(yesterday); /* normalizza data */


    strcpy(buffer, period);

    if (strcmp(buffer, " ") == 0)
        return NULL;

    first_date = strtok(buffer, date_delim);
    DEBUG_PRINT(("first date is: %s\n", first_date));
    last_date = strtok(NULL, date_delim);
    DEBUG_PRINT(("last date is: %s\n", last_date));

    // prima data
    strcpy(tmp, first_date);

    if(strcmp(tmp, "*") == 0) {
        // la prima data accettabile è 1:1:1970
        DEBUG_PRINT(("first date is epoch\n"));
        if (strptime("01:01:1970", "%d:%m:%Y",&(p->first_date)) == NULL)
            return NULL;
    } else {
        if (strptime(tmp, "%d:%m:%Y",&(p->first_date)) == NULL)
            return NULL;
        if (difftime(mktime(yesterday), mktime(&(p->first_date))) < 0.0) {// è stata impostata una data successiva a l'ultimo registro chiuso (ieri)
            DEBUG_PRINT(("la prima data deve essere antecedente a ieri: %f\n", difftime(mktime(&(p->first_date)), mktime(yesterday))));
            return NULL;
        }
    }
    DEBUG_PRINT(("p->first_date is: %s", asctime(&(p->first_date))));

    // ultima data
    strcpy(tmp, last_date);

    if(strcmp(tmp, "*") == 0) {
        // l'ultima data deve essere ieri (ossia l'ultimo registro chiuso)
        DEBUG_PRINT(("last_date is yesterday\n"));
        p->last_date.tm_mday = yesterday->tm_mday;
        p->last_date.tm_mon = yesterday->tm_mon;
        p->last_date.tm_year = yesterday->tm_year;
        p->last_date.tm_wday = yesterday->tm_wday;
        p->last_date.tm_yday = yesterday->tm_yday;
        p->last_date.tm_isdst = yesterday->tm_isdst;
    } else {
        if (strptime(tmp, "%d:%m:%Y",&(p->last_date)) == NULL)
            return NULL;
        if (difftime(mktime(yesterday), mktime(&(p->last_date))) < 0.0) {// è stata impostata una data successiva a l'ultimo registro chiuso (ieri)
            DEBUG_PRINT(("too far in the future, cowboy\n"));
            // p->last_date.tm_mday = yesterday->tm_mday;
            // p->last_date.tm_mon = yesterday->tm_mon;
            // p->last_date.tm_year = yesterday->tm_year;
            return NULL;
        }
    }
    DEBUG_PRINT(("p->last_date is: %s", asctime(&(p->last_date))));

    if (difftime(mktime(&(p->last_date)), mktime(&(p->first_date))) <= 0.0) {// le date devono essere diverse e la prima antecedente alla sconda
        DEBUG_PRINT(("keep it in order, please\n"));
        return NULL;
    }
    SCREEN_PRINT(("formato periodo ora valido"));
    return p;
}

// cerca nella lista delle aggregazioni se è già stato calcolato il risultato
bool Register_queryAggregation (char aggr[20], char type[20], char str_period[50])
{
    if (strcmp(aggr, "sum") == 0) {
        struct sumAggregation* elem = Peer.sumAggregationList;
        while (elem != NULL)
        {
            if (strcmp(elem->question, str_period) == 0) {
                SCREEN_PRINT(("aggregazione trovata tra quelle già calcolate:\nrisultato:n\n\t%d", elem->sum_answer));
                return (true);
            }
            elem = elem->next;
        }
        return (false);
        
    } else if (strcmp(aggr, "var") == 0) {
        struct varAggregation* elem = Peer.varAggregationList;
        while (elem != NULL)
        {
            if (strcmp(elem->question, str_period) == 0) {
                SCREEN_PRINT(("aggregazione trovata tra quelle già calcolate:\nrisultato:\n"));
                for (int i = 0; i < CVECTOR_TOTAL(elem->var_answer); i++) {
                    SCREEN_PRINT(("\t[%d] %d\n", i, CVECTOR_GET(elem->var_answer, int, i)));
                }
                return (true);
            }
            elem = elem->next;
        }
        return (false);
    }

    SCREEN_PRINT(("aggregazione non valida [ sum | var ]"));
    return (true); // è necessario ritornare true per non eseguire queryNeighbors e Flood
}

// invia una richiesta a tutti i vicini per sapere se hanno già calcolato il risultato
bool Peer_queryNeighbors (char aggr[20], char type[20], char str_period[50])
{
    SCREEN_PRINT(("Inoltro la richiesta di aggregazione ai miei vicini"));
    for (int i = 0; i < CVECTOR_TOTAL(Peer.neighbors); i++)
    {
        char *tmp;
        int nei_port = CVECTOR_GET(Peer.neighbors, int, i);
        int sd = net_initTCP(nei_port);
        // DEBUG_PRINT(("chiedo a %d se ha l'aggregazione\n"));
        strcat(strcat(strcat(strcat(strcpy(tmp, aggr), " "), type), " "), str_period);
        net_sendTCP(sd, "REQAG", strcat(tmp));
        net_receiveTCP(sd, tmp);
        if (strlen(tmp) != 0) {
            SCREEN_PRINT(("Il vicino %d ha l'aggregazione pronta\nrisultato:\n\t%s", tmp));
            return (true);
        }
        free(tmp);
    }
    return (false);
    
}
void Peer_flood (struct Period* period, cvector* relevant_peers)
{
    int fdmax = -1; 
    int remaining_neighbors = CVECTOR_TOTAL(sPeer.vicini);
    fd_set read_fds;
    FD_ZERO(&read_fds);

    for (int i = 0; i < remaining_neighbors; i++)
    {
        int sd = net_initTCP(CVECTOR_GET(sPeer.vicini, int, i));
        FD_SET(sd, &read_fds);
        if (sd > fdmax)
            fdmax = sd;
    }
    while (remaining_neighbors > 0)
    {
        if (select(fdmax + 1, &read_fds, NULL, NULL, NULL) > 0) {
            for (int i = 0; i < fdmax + 1; i++)
            {
                if (FD_ISSET(i, &read_fds)) {
                    char* buffer;
                    net_receiveTCP(i, buffer);
                    strcat(answer, buffer);
                    remaining_neighbors--;
                    free(buffer);
                }
            }
            
        }
    }
}
void Peer_queryPeer (int relevant_peer_port, struct Period* period)
{
    // ...
}
char* Peer_calculateAggregation (char aggr[20], char type[20], struct Period* period)
{
    // ...
}

void Peer_performAggregation(char aggr[20], char type[20], char str_period[50])
{
    DEBUG_PRINT(("richiesta aggregazione: %s, tipo: %s, periodo: %s\n", aggr, type, str_period));
    struct Period* period = Register_formatPeriod(str_period);
    if (period != NULL) {
        if (!Register_queryAggregation(aggr, type, str_period)) { // se l'aggregazione è già stata calcolata, viene riportato il risultato
            if (!Peer_queryNeighbors(aggr, type, str_period)) { // se un vicino possiede l'aggregazione, questa viene comunicata, il peer la salva e ne riporta il risultato
                char req_id[64];
                sprintf(req_id, "%d-%ld", Peer.port, time(NULL));
                VECTOR_ADD(Peer.flood_requests, req_id);
                CVECTOR_INIT(relevant_peers);
                Peer_flood(period, &relevant_peers); // è necessario fare un flood della rete. Vengono salvari i peer che hanno almeno una entry di interesse
                for (int i = 0; i < CVECTOR_TOTAL(relevant_peers); i++) {
                    Peer_queryPeer(CVECTOR_GET(relevant_peers, int, i), period);
                }
                SCREEN_PRINT(("risultato: %s", Peer_calculateAggregation(aggr, type, period)));
            }
        }
    } else {
        SCREEN_PRINT(("periodo non valido\n"));
    }
}



void Peer_menu(char* buffer)
{
    char ds_addr[20], type[20], aggr[20], period[50], tmp[20];
    int ds_port, quantity;

    sscanf(buffer, "%s", tmp);
    SCREEN_PRINT(("\n"));
    if (strcmp("help", tmp) == 0) {
        Peer_help();
    } else if (strcmp("start", tmp) == 0) {
        if (sscanf(buffer, "%s %s %d", tmp, ds_addr, &ds_port) == 3) {
            Peer_serverBoot(ds_addr, ds_port);
        } else
            SCREEN_PRINT(("Formato non valido per start [ start 127.0.0.1 4242 ]\n\n"));
    } else if (strcmp("add", tmp) == 0) {
        if (sscanf(buffer, "%s %s", tmp, type) == 2) {
            if (strcmp(type, "tampone") == 0){
                if (sscanf(buffer, "%s %s %d", tmp, type, &quantity) == 3) {
                    Register_updateCurrentEntry(type, quantity); // RegistroGiornaliero_aggiungiEntry(type, quantity);
                } else
                    SCREEN_PRINT(("Formato non valido per add [ add tampone 42 ]\n\n"));
            }
            else if (strcmp(type, "nuovo") == 0) {
                if (sscanf(buffer, "%s %s %*s %d", tmp, type, &quantity) == 3) {
                    Register_updateCurrentEntry(type, quantity); // RegistroGiornaliero_aggiungiEntry(type, quantity);
                } else
                    SCREEN_PRINT(("Formato non valido per add [ add nuovo 42 ]\n\n"));
            } else
                SCREEN_PRINT(("Formato non valido per add [ add (tampone | nuovo) 42 ]\n\n"));
        } else
            SCREEN_PRINT(("Formato non valido per add [ add (tampone | nuovo) 42 ]\n\n"));
    } else if (strcmp("get", tmp) == 0) {
        if (sscanf(buffer, "%s %s %s", tmp, aggr, type) == 3) {
            if (strcmp(type, "tampone") == 0)  {
                if (sscanf(buffer, "%s %s %s %s", tmp, aggr, type, period) == 4) {
                    Peer_performAggregation(aggr, type, period); // ArchivioAggregazioni_gestioneAggregazione(aggr, type, period);
                }
                else if (sscanf(buffer, "%s %s %s %s", tmp, aggr, type, period) == 3) {
                    strcpy(period, " ");
                    Peer_performAggregation(aggr, type, period); // ArchivioAggregazioni_gestioneAggregazione(aggr, type, period);
                } else
                    SCREEN_PRINT(("Formato non valido per get [get (sum | var) (nuovo | tampone) (dd1:mm1:yy1 | *)-(dd2:mm2:yy2 | *) ]\n\n"));
            } else if (strcmp(type, "nuovo") == 0)  {
                if (sscanf(buffer, "%s %s %s %*s %s", tmp, aggr, type, period) == 4) {
                    Peer_performAggregation(aggr, type, period); // ArchivioAggregazioni_gestioneAggregazione(aggr, type, period);
                }
                else if (sscanf(buffer, "%s %s %s %*s %s", tmp, aggr, type, period) == 3) {
                    strcpy(period, " ");
                    Peer_performAggregation(aggr, type, period); // ArchivioAggregazioni_gestioneAggregazione(aggr, type, period);
                } else
                    SCREEN_PRINT(("Formato non valido per get [get (sum | var) (nuovo | tampone) (dd1:mm1:yy1 | *)-(dd2:mm2:yy2 | *) ]\n\n"));
            } else
                SCREEN_PRINT(("Tipo non valido\n\n"));
        } else
            SCREEN_PRINT(("Formato non valido per get [get (sum | var) (nuovo | tampone) (dd1:mm1:yy1 | *)-(dd2:mm2:yy2 | *) ]\n\n"));
    } else if (strcmp("stop", tmp) == 0) {
        Peer_stop();
    } else if (strcmp("showneighbors", tmp) == 0) {
        Peer_showNeighbors();
    } else {
        SCREEN_PRINT(("Comando non valida. Usare help per una lista dei comandi disponibili\n\n"));
    }

}

void Peer_handleAggrReq(int sd)
{
    char buffer[1024];
    // ricevo l'aggregazione richiesta
    // net_receiveTCP(sd, buffer);
    recv(sd, buffer, 1024, 0);
    DEBUG_PRINT(("handling aggr req: %s\n", buffer));
    // verifico se posseggo l'aggregazione
    // rispondo al richiedente
    // free(buffer);
    close(sd);
}

void Peer_handleEntryReq(int sd)
{
    char* buffer;
    // ricevo l'aggregazione richiesta
    recv(sd, buffer, 1024, 0);
    DEBUG_PRINT(("handling entry req: %s\n", buffer));
    // verifico se posseggo l'aggregazione
    // rispondo al richiedente
    free(buffer);
    close(sd);
}

void Peer_handleFlood(int sd)
{
    char* buffer;
    // ricevo l'aggregazione richiesta
    recv(sd, buffer, 1024, 0);
    DEBUG_PRINT(("handling flood req: %s\n", buffer));
    // verifico se posseggo l'aggregazione
    // rispondo al richiedente
    free(buffer);
    close(sd);
}

void Peer_handleNeighborStop(int sd)
{
    char* buffer;
    // ricevo l'aggregazione richiesta
    recv(sd, buffer, 1024, 0);
    DEBUG_PRINT(("handling nei stop signal: %s\n", buffer));
    // verifico se posseggo l'aggregazione
    // rispondo al richiedente
    free(buffer);
    close(sd);
}

//funzione che gestisce l'arrivo di una comunicazione di nuovi vicini da parte del Ds
// questi verranno a sostituire i precedenti, in quanto viene sempre inviata l'intera lista aggiornata
void Peer_handleNeighborsUpdate(int sd)
{
    int ret, num, len, i = 0;
    uint16_t lmsg;
    const char delim[2] = ":";
    char* token;
    char buffer[1024];
    char neighbors[1024];
    memset(buffer, 0, sizeof(buffer));
    memset(neighbors, 0, sizeof(neighbors));

    ret = recv(sd, (void *)&lmsg, sizeof(uint16_t), 0);
    if (ret < 0)
    {
        perror("Errore in fase di ricezione lunghezza messaggio: ");
        exit(1);
    }
    len = ntohs(lmsg);
    DEBUG_PRINT(("devo ricevere %d bytes\n", len));
    ret = recv(sd, (void*)buffer, len, 0);
    if (ret < 0)
    {
        perror("Errore in fase di ricezione messaggio: ");
        exit(1);
    }
    sscanf(buffer, "%d %s", &num, neighbors);

    SCREEN_PRINT(("Mi sono stati comunicati i %d nuovi vicini: %s\n\n", num, neighbors));
    // il vecchio vettore verrà completamente sostituito
    CVECTOR_FREE((Peer.neighbors));
    cvector_init(&(Peer.neighbors));

    token = strtok(neighbors, delim);
    while (token != NULL && i < num) {
        if(atoi(token) != Peer.port) { // un peer non deve avere se stesso come vicino
            DEBUG_PRINT(("I add (%d) because i'm (%d) and i'm different\n", atoi(token), Peer.port));
            CVECTOR_ADD((Peer.neighbors), atoi(token));
        }
        token = strtok(NULL, delim);
    }

    close(sd);
    FD_CLR(sd, &iom.master);
}

//funzione che gestisce l'arrivo di un messaggio di stop da parte del Ds
void Peer_handleDsStop()
{
    SCREEN_PRINT(("Mi arrivato un messaggio di stop da parte del Ds\n"));
//     // FileManager_salvaArchivioAggregazioni();
    // FileManager_salvaRegistro(1);
    exit(0);
}

// gestione di un comando accettato su un socket TCP
void Peer_handleTCP(char* buffer, int sd)
{
    DEBUG_PRINT(("handling command: %s\n", buffer));
    if (strcmp(buffer, "REQAG") == 0) //messaggio di richiesta di aggregazione 
        Peer_handleAggrReq(sd);
    else if (strcmp(buffer, "REQRE") == 0) //messaggio di richiesta di uno o più entry
        Peer_handleEntryReq(sd);
    else if (strcmp(buffer, "FLOOD") == 0) //messaggio di inoltro di una FLOOD_FOR_ENTRIES
        Peer_handleFlood(sd);
    else if (strcmp(buffer, "VSTOP") == 0) //segnale di stop da parte di un neighbor
        Peer_handleNeighborStop(sd);
    else if (strcmp(buffer, "UPDVI") == 0) //messaggio di comunicazione di nuovi vicini da parte del DS
        Peer_handleNeighborsUpdate(sd);
    else if (strcmp(buffer, "DSTOP") == 0) //messaggio di stop da parte del DS
        Peer_handleDsStop();
    else
        DEBUG_PRINT(("Ricevuto messaggio non valido\n"));

    close(sd);
    FD_CLR(sd, &iom.master);
}

// non verrà mai chiamata se use_udp è posto a false in IOMultiplex,
// i peer infatti non sono mai passivamente in ascolto su un socket udp
void emptyFunc(int boot)
{
    /* NOTREACHED */
    return;
}


int main(int argc, char *argv[])
{
    Peer_init(argv[1]);

    Peer_help();
    #ifdef DEBUG_ON
    Peer_serverBoot("127.0.0.1", 4242); // just to save time
    #endif
    IOMultiplex(Peer.port, &iom, false, Peer_menu, emptyFunc, Peer_handleTCP);    
    return (0);
}
