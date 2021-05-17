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
// #include "vector.h"
#include "Include.h"

#define TIMEOUT 100000              /*tempo che il peer aspetta per ricevere la risposta al boot dal server*/
                                    // se si usa usleep(), indica i microsecondi
#define DEBUG_ON

#ifdef DEBUG_ON
# define DEBUG_PRINT(x) printf x
#else
# define DEBUG_PRINT(x) do {} while (0)
#endif

void sPeer_initialize(char* port)
{
    sscanf(port, "%d", &sPeer.port);
    DEBUG_PRINT(("sono peer: %d\n", sPeer.port));
    cvector_init(&(sPeer.vicini));
    vector_init(&(sPeer.flood_requests));

    FileManager_caricaRegistro();
    FileManager_caricaArchivioAggregazioni();
}

//funzione che stampa nel dettaglio le operazioni del peer
void sPeer_help()
{
    printf("\nhelp --> mostra i dettagli sui comandi accettati\n\n");
    printf("start DS_addr DS_port --> richiede al DS,in ascolto all’indirizzo DS_addr:DS_port, la connessione al network. Il DS registra il peer e gli invia la lista di neighbor. Se il peer è il primo a connettersi, la lista sarà vuota. Il DS avrà cura di integrare il peer nel network a mano a mano che altri peer invocheranno la star\n\n");
    printf("add type quantity --> aggiunge al register della data corrente l’evento type con quantità quantity. Questo comando provoca la memorizzazione di una nuova entry nel peer\n\n");
    printf("get aggr type period --> effettua una richiesta di elaborazione per ottenere il dato aggregato aggr sui dati relativi a un lasso temporale d’interesse period sulle entry di tipo type. Considerando tali dati su scala giornaliera. Le aggregazioni aggr calcolabili sono il totale e la variazione. Il parametro period è espresso come ‘dd1:mm1:yyyy1-dd2:mm2:yyyy2’, dove con 1 e 2 si indicano, rispettivamente, la data più remota e più recente del lasso temporale d’interesse. Una delle due date può valere ‘*’. Se è la prima, non esiste una data che fa da lower bound. Viceversa, non c’è l’upper bound e il calcolo va fatto fino alla data più recente (con register chiusi). Il parametro period puòmancare, in quel caso il calcolo si fa su tutti i dati. \n\n");
    printf("stop --> il peer richiede una disconnessione dal network. Il comando stop provoca l’invio ai neighbordi tutte le entry registrate dall’avvio. Il peer contatta poi il DS per comunicare la volontà di disconnettersi. Il DS aggiorna i registri di prossimità rimuovendo il peer. Se la rimozione del peer causa l’isolamento di una un’interaparte del network, il DS modifica i registri di prossimitàdi alcuni peer, affinché questo non avvenga\n\n");
}

void sPeer_showNeighbors()
{
    #ifdef DEBUG_ON
    printf("\nshowing peer: %d\nHis neighbors are:\n", sPeer.port);
    for (int i = 0; i < CVECTOR_TOTAL((sPeer.vicini)); i++) {
        printf("[%d] %d\n", i, CVECTOR_GET((sPeer.vicini), int, i));
    }
    #else
    printf("Opzione non valida (Are you developer?)\n\n");
    #endif
}

//funzione che si occupa di effettura il boot del Peer inviando un messaggio UDP al Ds
void sPeer_serverBoot(char *ds_addr, int ds_port)
{
    //faccio il boot
    int ret, sd, num, len;
    uint16_t lmsg;
    socklen_t addrlen;
    struct sockaddr_in srv_addr;
    char buffer[BOOT_RESP];
    char neighbors_list[1024];
    char msg[BOOT_MSG], tmp[10];
    char* token;
    const char delim[2] = ":";

    sprintf(tmp, "%d", sPeer.port);
    strcpy(msg, tmp);

    memset(neighbors_list, 0, sizeof(neighbors_list));
    memset(buffer, 0, sizeof(buffer));

    strcpy(sPeer.ds_ip, ds_addr);
    sPeer.ds_port = ds_port;

    sd = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, 0);

    memset(&srv_addr, 0, sizeof(srv_addr));
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(ds_port);
    inet_pton(AF_INET, ds_addr, &srv_addr.sin_addr);

    addrlen = sizeof(srv_addr);

    do
    {
        printf("Invio il messaggio di boot\n");
        ret = sendto(sd, msg, BOOT_MSG, 0, (struct sockaddr *)&srv_addr, sizeof(srv_addr));
        if (ret < 0) {
            perror("Errore nell'invio: ");
            exit(1);
        }
        usleep(TIMEOUT); // FIXUP: maybe use a select with timeout, not to block on this 
        ret = recvfrom(sd, (void*) &lmsg, sizeof(uint16_t), 0, (struct sockaddr *)&srv_addr, &addrlen);
        //se dopo aver aspettato il timeout non ricevo riposta riinvio il messaggio
    } while (ret < 0);

    printf("Richiesta di boot accettata\n");
    len = ntohs(lmsg);
    DEBUG_PRINT(("len is: %d\n", len));
    ret = recvfrom(sd, (void*)buffer, len, 0, (struct sockaddr *)&srv_addr, &addrlen);
    if (ret < 0 || ret < len)
    {
        perror("Errore in fase di ricezione lunghezza messaggio: ");
        DEBUG_PRINT(("ret: %d", ret));
        exit(1);
    }
    DEBUG_PRINT(("buffer recieved: %s\n", buffer));
    sscanf(buffer, "%d %s", &num, neighbors_list);
    printf("Answer ricevuta: %d %s\n\n", num, neighbors_list);

    //FIXUP: actually parse and add the new neighbor_list in the form `3 :123:234:345

    // first token
    token = strtok(neighbors_list, delim);
    // walk through the other tokens
    while (token != NULL) {
        DEBUG_PRINT(("%s\n", token));
        CVECTOR_ADD((sPeer.vicini), atoi(token));
        token = strtok(NULL, delim);
    }

    close(sd);
}

//funzione che richiede ai vicini se hanno una determinata aggregazione
int sPeer_richiestaAggregazioneNeighbor(char *aggr, char *type, char *period)
{
    int sd, ret, len, i;
    struct sockaddr_in vicini_addr;
    char buffer[1024], intervallo_date[MAX_SIZE_RISULTATO][22];
    uint16_t size_risultato_ricevuto, lmsg;
    int risultato[MAX_SIZE_RISULTATO], size_risultato;

    for (i = 0; i < CVECTOR_TOTAL((sPeer.vicini)); i++)
    {
        /*if (sPeer.vicini[i] == 0) // this doesn't make sense with our structure
            continue;
            */

        sd = socket(AF_INET, SOCK_STREAM, 0);

        memset(&vicini_addr, 0, sizeof(vicini_addr));
        vicini_addr.sin_family = AF_INET;
        vicini_addr.sin_port = htons((int) CVECTOR_GET((sPeer.vicini), int, i));
        inet_pton(AF_INET, "127.0.0.1", &vicini_addr.sin_addr);

        ret = connect(sd, (struct sockaddr *)&vicini_addr, sizeof(vicini_addr));
        if (ret < 0)
        {
            perror("Errore in fase di connessione: ");
            exit(-1);
        }

        strcpy(buffer, "REQAG"); //REQA -> Richiesta Aggregazione

        ret = send(sd, (void *)buffer, REQ_LEN, 0);
        if (ret < 0)
        {
            perror("Errore in fase di invio: ");
            exit(-1);
        }

        strcpy(buffer, aggr);
        strcat(buffer, " ");
        strcat(buffer, type);
        strcat(buffer, " ");
        strcat(buffer, period);

        len = strlen(buffer) + 1;
        lmsg = htons(len);

        ret = send(sd, (void *)&lmsg, sizeof(uint16_t), 0);
        if (ret < 0)
        {
            perror("Errore in fase di invio: ");
            exit(-1);
        }

        ret = send(sd, (void *)buffer, len, 0);
        if (ret < 0)
        {
            perror("Errore in fase di invio: ");
            exit(-1);
        }

        printf("Richiedo al neighbor %d se ha l'aggregazione\n", (int) CVECTOR_GET((sPeer.vicini), int, i));

        ret = recv(sd, (void *)&size_risultato_ricevuto, sizeof(uint16_t), 0);
        if (ret < 0)
        {
            perror("Errore in fase di ricezione: ");
            exit(-1);
        }

        //se il la dimensione del risultato è diversa da zero il neighbor in questione ha l'aggregazione e me la invia
        size_risultato = ntohs(size_risultato_ricevuto);
        if (size_risultato != 0)
        {
            printf("Il neighbor ha l'aggregazione\n");
            for (i = 0; i < size_risultato; i++)
            {
                ret = recv(sd, (void *)&lmsg, sizeof(uint16_t), 0);
                len = ntohs(lmsg);
                ret = recv(sd, (void *)buffer, len, 0);
                if (ret < 0)
                {
                    perror("Errore in fase di ricezione: ");
                    exit(-1);
                }
                sscanf(buffer, "%d", &risultato[i]);
            }
            for (i = 0; i < size_risultato; i++)
            {
                ret = recv(sd, (void *)buffer, 22, 0);
                strcpy(intervallo_date[i], buffer);
            }

            ArchivioAggregazioni_aggiungiAggregazione(aggr, type, period, risultato, intervallo_date, size_risultato);
            close(sd);
            return 1;
        }
        else
        {
            printf("Il neighbor non ha l'aggregazione richiesta\n");
        }
        close(sd);
    }
    return 0;
}

//funzione che gestisce una richiesta di aggregazione in arrivo da un altro peer
void sPeer_gestioneRichiestaAggregazione(int sd)
{
    int ret, len, i = 0;
    uint16_t size_risultato_inviato, lmsg;
    int size_risultato;
    char buffer[1024], tmp[20];
    char aggr[20], type[20], period[20];
    struct Aggregazione *pp;

    printf("Ricevuta una richiesta di aggregazione\n");
    ret = recv(sd, (void *)&lmsg, sizeof(uint16_t), 0);
    if (ret < 0)
    {
        perror("Errore in fase di ricezione: ");
        exit(1);
    }

    len = ntohs(lmsg);
    ret = recv(sd, (void *)buffer, len, 0);
    if (ret < 0)
    {
        perror("Errore in fase di ricezione: ");
        exit(1);
    }

    sscanf(buffer, "%s %s %s", aggr, type, period);
    pp = ArchivioAggregazioni_controlloPresenzaAggregazione(aggr, type, period);
    if (pp)
    {
        printf("Invio l'aggregazione trovata al richiedente\n\n");
        size_risultato = pp->size_risultato;
        size_risultato_inviato = htons(size_risultato);
        ret = send(sd, (void *)&size_risultato_inviato, sizeof(uint16_t), 0);
        if (ret < 0)
        {
            perror("Errore in fase di invio: ");
            exit(1);
        }

        for (i = 0; i < size_risultato; i++)
        {
            sprintf(tmp, "%d", pp->risultato[i]);
            len = strlen(tmp) + 1;
            lmsg = htons(len);
            ret = send(sd, (void *)&lmsg, sizeof(uint16_t), 0);
            ret = send(sd, (void *)tmp, len, 0);
        }

        for (i = 0; i < size_risultato; i++)
        {
            strcpy(buffer, pp->intervallo_date[i]);
            ret = send(sd, (void *)buffer, 22, 0);
            if (ret < 0)
            {
                perror("Errore in fase di invio: ");
                exit(-1);
            }
        }
    }
    else
    {
        printf("Dico al richiedente che non ho trovato l'aggregazione\n\n");
        size_risultato = 0;
        size_risultato = htons(size_risultato);
        ret = send(sd, (void *)&size_risultato, sizeof(uint16_t), 0);
        if (ret < 0)
        {
            perror("Errore in fase di invio: ");
            exit(1);
        }
    }

    close(sd);
    FD_CLR(sd, &iom.master);
}

//funzione che richiede i Registri Necessari ai peer da contattare in base alle Date Necessarie
void sPeer_richiestaRegistri()
{
    int sd, ret, i;
    struct sockaddr_in peer_addr;
    char buffer[1024];

    for (i = 0; i < sPeer.numero_peer_da_contattare; i++)
    {
        sd = socket(AF_INET, SOCK_STREAM, 0);

        memset(&peer_addr, 0, sizeof(peer_addr));
        peer_addr.sin_family = AF_INET;
        peer_addr.sin_port = htons(sPeer.peer_da_contattare[i]);
        inet_pton(AF_INET, "127.0.0.1", &peer_addr.sin_addr);

        printf("Richiedo al peer %d i Registri\n", sPeer.peer_da_contattare[i]);

        ret = connect(sd, (struct sockaddr *)&peer_addr, sizeof(peer_addr));
        if (ret < 0)
        {
            perror("Errore in fase di connessione: ");
            exit(-1);
        }

        strcpy(buffer, "REQRE");

        ret = send(sd, (void *)buffer, REQ_LEN, 0);
        if (ret < 0)
        {
            perror("Errore in fase di invio: ");
            exit(-1);
        }

        RegistroGenerale_invioDateNecessarie(sd);
        RegistroGenerale_ricevoRegistriRichiesti(sd);
        RegistroGenerale_modificoIlRegistroGeneraleInBaseAiRegistriRicevuti();

        close(sd);
    }
    RegistroGenerale_impostoRegistriRichiestiCompleti();
    ArchivioAggregazioni_calcoloAggregazione(ArchivioAggregazioni.aggr, ArchivioAggregazioni.type, ArchivioAggregazioni.period);
    sPeer.richiesta_in_gestione = 0; //una volta ricevute tutte le risposte imposta il flag richiesta in gestione = 0
}

//funzione che invia al server il messaggio di disconnesssione dalla rete da parte di un peer
void sPeer_disconnessioneNetwork()
{
    int sd, ret;
    uint16_t lmsg;
    struct sockaddr_in ds_addr;
    char buffer[1024];

    sd = socket(AF_INET, SOCK_STREAM, 0);

    memset(&ds_addr, 0, sizeof(ds_addr));
    ds_addr.sin_family = AF_INET;
    ds_addr.sin_port = htons(sPeer.ds_port);
    inet_pton(AF_INET, sPeer.ds_ip, &ds_addr.sin_addr);

    printf("Invio al Ds la richiesta di disconnessione\n");
    ret = connect(sd, (struct sockaddr *)&ds_addr, sizeof(ds_addr));
    if (ret < 0)
    {
        perror("Errore in fase di connessione: ");
        exit(-1);
    }

    strcpy(buffer, "RSTOP");

    ret = send(sd, (void *)buffer, REQ_LEN, 0);
    if (ret < REQ_LEN)
    {
        perror("Errore in fase di invio: ");
        exit(-1);
    }

    lmsg = htons(sPeer.port);
    ret = send(sd, (void *)&lmsg, sizeof(uint16_t), 0);
    if (ret < sizeof(uint16_t))
    {
        perror("Errore in fase di invio: ");
        exit(-1);
    }
    ret = recv(sd, (void*)buffer, 4, 0); // ACK
    if (ret < 4)
    {
        perror("Errore in fase di ricezione: ");
        exit(1);
    }

}

//funzione che gestisce la ricezione di un messaggio di stop da parte di un neighbor peer
void sPeer_gestisciStop(int sd)
{
    printf("Ho ricevuto una richiesta di stop da parte di un vicino\n");
    RegistroGenerale_ricevoRegistriRichiesti(sd);                          //ricevo i registri dal peer che si sta disconettendo
    RegistroGenerale_modificoIlRegistroGeneraleInBaseAiRegistriRicevuti(); //me li salvo nel RegistroGenerale

    close(sd);
    FD_CLR(sd, &iom.master);
}

//funzione che gestisce il comando di stop
void sPeer_stop()
{
    int sd, ret, cont = 0;
    struct sockaddr_in peer_addr;
    struct RegistroGiornaliero *pp;
    char buffer[1024];


    if (CVECTOR_TOTAL((sPeer.vicini)) > 0)
    {
        int vicino = CVECTOR_GET((sPeer.vicini), int, 0); // il primo vicino è sicuramente presente e sarà colui che riceverà il mio registro generale
        sd = socket(AF_INET, SOCK_STREAM, 0);

        memset(&peer_addr, 0, sizeof(peer_addr));
        peer_addr.sin_family = AF_INET;
        peer_addr.sin_port = htons(vicino);
        inet_pton(AF_INET, "127.0.0.1", &peer_addr.sin_addr);

        printf("Invio al neighbor Peer %d i miei registri\n", vicino);
        ret = connect(sd, (struct sockaddr *)&peer_addr, sizeof(peer_addr));
        if (ret < 0)
        {
            perror("Errore in fase di connessione: ");
            exit(-1);
        }

        strcpy(buffer, "REQST");

        ret = send(sd, (void *)buffer, REQ_LEN, 0);
        if (ret < 0)
        {
            perror("Errore in fase di invio: ");
            exit(-1);
        }
        //visto che devo inviare tutto il mio RegistroGenerale ad un mio vicino usando la funzione invioRegistriRichiesti devo inizializzare
        //correttamente la lista dei registri_richiesti e il numero di registri_richiesti
        RegistroGenerale.lista_registri_richiesti = RegistroGenerale.lista_registri;
        pp = RegistroGenerale.lista_registri;
        while (pp != 0)
        {
            cont++;
            pp = pp->next_registro;
        }
        RegistroGenerale.numero_registri_richiesti = cont;
        RegistroGenerale_invioRegistriRichiesti(sd);
        //recv(sd, buffer, 1, 0); // ACK da parte del Ds per potersi disconnettere
                                // necessario affinché la struttura dati di vicinanza tenuta dal DS
                                // venga aggiornata correttamente
        //DEBUG_PRINT(("ho finalmente ricevuto l'ack\n"));
        close(sd);
    }

    sPeer_disconnessioneNetwork();
    FileManager_salvaArchivioAggregazioni();
    FileManager_salvaRegistro(0);
    //Let the OS free all used memory...
    exit(0);
}

//funzione che aggiunge il peer passato come parametro alla lista dei peer da contattare
void sPeer_aggiungiPeerDaContattare(int peer)
{
    //prima controllo se il peer è già nella lista se non c'è lo aggiungo
    int trovato, x;

    trovato = 0;
    for (x = 0; x < sPeer.numero_peer_da_contattare; x++)
    {
        if (sPeer.peer_da_contattare[x] == peer)
            trovato = 1;
    }
    if (!trovato)
    {
        printf("Aggiungo il peer %d alla lista dei peer da contattare\n", peer);
        sPeer.peer_da_contattare[sPeer.numero_peer_da_contattare] = peer;
        sPeer.numero_peer_da_contattare++;
    }
    else
    {
        printf("Peer %d non aggiunto alla lista dei peer da contattare perchè già presente\n", peer);
    }
}

//funzione che manda i neighbor la richiesta di Flood
void sPeer_richiestaFlood(int requester)
{
    int sd, ret, len, i;
    struct sockaddr_in vicini_addr;
    struct DataNecessaria *pp;
    uint16_t lmsg;
    char buffer[1024], richiesta[1024], port[10];

    pp = RegistroGenerale.lista_date_necessarie;
    if (pp == 0)
    {
        printf("Non inoltro la richiesta di flood perchè la lista delle date necessarie risulta vuota\n");
        return;
    }

    sprintf(richiesta, "%s", sPeer.richiesta_id);
    strcat(richiesta, " ");
    sprintf(port, "%d", sPeer.port);
    strcat(richiesta, port);

    //in richiesta è presente l'id della richiesta più il peer che sta inoltrando tale richiesta
    //l'id di una richiesta è formata dalla stringa contenente la port del peer che ha fatto la richiesta e il numero sequenziale della richiesta

    for (i = 0; i < CVECTOR_TOTAL((sPeer.vicini)); i++)
    {
        int i_esimo_vicino = CVECTOR_GET((sPeer.vicini), int, i);
        if (requester == i_esimo_vicino || i_esimo_vicino == 0)
            continue; //non inoltro la richiesta a chi l'ha inoltrata a me anche se è un mio vicino
        //i vicini == 0 non sono vicini reali

        sPeer.risposte_mancanti++;

        sd = socket(AF_INET, SOCK_STREAM, 0);

        memset(&vicini_addr, 0, sizeof(vicini_addr));
        vicini_addr.sin_family = AF_INET;
        vicini_addr.sin_port = htons(i_esimo_vicino);
        inet_pton(AF_INET, "127.0.0.1", &vicini_addr.sin_addr);

        printf("Inoltro la richiesta di Flood al peer %d\n", i_esimo_vicino);

        ret = connect(sd, (struct sockaddr *)&vicini_addr, sizeof(vicini_addr));
        if (ret < 0)
        {
            perror("Errore in fase di connessione: ");
            exit(-1);
        }

        strcpy(buffer, "FLOOD");

        ret = send(sd, (void *)buffer, REQ_LEN, 0);
        if (ret < 0)
        {
            perror("Errore in fase di invio: ");
            exit(-1);
        }

        strcpy(buffer, richiesta);

        len = strlen(buffer) + 1;
        lmsg = htons(len);

        ret = send(sd, (void *)&lmsg, sizeof(uint16_t), 0);
        if (ret < 0)
        {
            perror("Errore in fase di invio: ");
            exit(-1);
        }

        ret = send(sd, (void *)buffer, len, 0);
        if (ret < 0)
        {
            perror("Errore in fase di invio: ");
            exit(-1);
        }

        RegistroGenerale_invioDateNecessarie(sd);

        FD_SET(sd, &iom.master);
        if (sd > iom.fdmax)
            iom.fdmax = sd;
    }
}

//funzione che invia il messaggio di risposta a chi aveva inoltrato richiesta di Flood
void sPeer_invioRispostaFlood()
{
    int sd, ret, i;
    uint16_t lmsg, peer;
    char buffer[1024];

    sd = sPeer.sd_flood;
    printf("Invio la risposta indietro a chi mi aveva inoltrato una richiesta di Flood\n\n");
    strcpy(buffer, "FLORE");

    ret = send(sd, (void *)buffer, REQ_LEN, 0);
    if (ret < 0)
    {
        perror("Errore in fase di invio: ");
        exit(1);
    }

    //invio indietro il numero di peer da contattare e la lista dei peer da contattare
    lmsg = htons(sPeer.numero_peer_da_contattare);

    ret = send(sd, (void *)&lmsg, sizeof(uint16_t), 0);
    if (ret < 0)
    {
        perror("Errore in fase di invio: ");
        exit(-1);
    }

    for (i = 0; i < sPeer.numero_peer_da_contattare; i++)
    {
        peer = htons(sPeer.peer_da_contattare[i]);
        ret = send(sd, (void *)&peer, sizeof(uint16_t), 0);
    }

    close(sd);
    FD_CLR(sd, &iom.master);
}

//funzione che gestisce la ricezione di una risposta alla richiesta di Flood
void sPeer_ricezioneRispostaFlood(int sd)
{
    int ret, numero_peer_tmp, i, peer_tmp;
    uint16_t lmsg, peer;
    printf("Ricevo la risposta ad una richiesta di Flood\n");
    ret = recv(sd, (void *)&lmsg, sizeof(uint16_t), 0);
    if (ret < 0)
    {
        perror("Errore in fase di ricezione: ");
        exit(-1);
    }

    numero_peer_tmp = ntohs(lmsg);

    for (i = 0; i < numero_peer_tmp; i++)
    {
        ret = recv(sd, (void *)&peer, sizeof(uint16_t), 0);
        if (ret < 0)
        {
            perror("Errore in fase di ricezione: ");
            exit(1);
        }
        peer_tmp = ntohs(peer);
        sPeer_aggiungiPeerDaContattare(peer_tmp);
    }

    close(sd);
    FD_CLR(sd, &iom.master);
    sPeer.risposte_mancanti--;
    if (sPeer.risposte_mancanti == 0) //se le risposte mancanti == 0 significa che tutti i neighbor ai quali hai inoltrato una richiesta
                                             //di flood ti hanno risposto quindi puoi proseguire con la richiesta dei registri ai vari peer
    {
        sPeer.richiesta_in_gestione = 0;
        printf("Ho ricevuto tutte le risposte che mi servivano\n\n");
        if (sPeer.port == sPeer.requester) //se la mia port è uguale al requester significa che io sono chi ha fatto la richiesta
                                                          //in origine se non è così devo inviare la risposta a chi mi ha inoltrato la richiesta
            sPeer_richiestaRegistri();
        else
            sPeer_invioRispostaFlood();
    }
}

//funzione che inizializza l'invio di una richiesta di Flood
void sPeer_sendFlood()
{
    char buffer[1024], numero_richiesta[10];
    int i;
    //imposto la reichiesta_id
    printf("Inizializzo una richiesta di Flood\n");
    sPeer.requester = sPeer.port;
    sprintf(buffer, "%d", sPeer.port);
    strcat(buffer, " ");
    sprintf(numero_richiesta, "%d", sPeer.richieste_inviate);
    strcat(buffer, numero_richiesta);

    strcpy(sPeer.richiesta_id, buffer);
    sPeer.richieste_inviate++;

    sPeer.risposte_mancanti = 0;
    sPeer.richiesta_in_gestione = 1;
    for (i = 0; i < MAX_PEER_DA_CONTATTARE; i++)
        sPeer.peer_da_contattare[i] = 0;
    sPeer.numero_peer_da_contattare = 0;
    sPeer_richiestaFlood(0);
    //se la richiesta flood non invia a nessun vicino la richiesta le risposte mancanti saranno uguale a 0 e si può procedere con la richiesta
    //dei registri;
    if (sPeer.risposte_mancanti == 0)
    {
        sPeer.richiesta_in_gestione = 0;
        sPeer_richiestaRegistri();
    }

    else
        printf("Attendo risposta\n\n");
}

//funzione che controlla le richieste di Flood in ingresso. Se non c'è una richiesta in gestione la richiesta viene accettata
//e diventa l'attuale richiesta in gestione. Se c'è già una richiesta in gestione la funzione controlla se la richiesta
//che arriva è uguale alla richiesta in gestione, se è così risponde dicendo che non ha peer da inviargli mentre se si tratta di una altra
//richiesta risponde che la richiesta non è stata accettata e che deve essere ritrasmessa.
//la funzione restituisce 1 se la richista è valida altrimenti restituisce 0
int sPeer_verificaRichiestaRicevuta(char richiesta_id[50], int sd)
{
    uint16_t lmsg;
    int ret, i, len, numero_recv;
    char buffer[1024];
    if (strcmp(sPeer.richiesta_id, richiesta_id) == 0) //inizializzo una nuova richiesta di Flood
    {
        //se la richiesta che mi è arrivata è uguale alla richiesta in gestione devo inviare indietro una risposta con numero peer = 0
        //per evitare il deadlock in una rete dove possono essere presenti cicli
        //ricezione fasulla delle date necessarie che mi sta inviando (non posso chiudere subito il socket mentre mi invia le date è da maleducati)
        ret = recv(sd, (void *)&lmsg, sizeof(uint16_t), 0);
        if (ret < 0)
        {
            perror("Errore in fase di ricezione: ");
            exit(1);
        }
        numero_recv = ntohs(lmsg);
        for (i = 0; i < numero_recv; i++)
        {
            ret = recv(sd, (void *)&lmsg, sizeof(uint16_t), 0);
            if (ret < 0)
            {
                perror("Errore in fase di ricezione: ");
                exit(1);
            }
            len = ntohs(lmsg);
            ret = recv(sd, (void *)buffer, len, 0);
            if (ret < 0)
            {
                perror("Errore in fase di ricezione: ");
                exit(1);
            }
        }
        printf("La richiesta è uguale a quella in gestione rispondo che non ho nulla da inviare\n\n");
        strcpy(buffer, "FLORE");
        ret = send(sd, (void *)buffer, REQ_LEN, 0);
        if (ret < 0)
        {
            perror("Errore in fase di invio: ");
            exit(1);
        }
        lmsg = htons(0);
        ret = send(sd, (void *)&lmsg, sizeof(uint16_t), 0);
        if (ret < 0)
        {
            perror("Errore in fase di invio: ");
            exit(-1);
        }

        close(sd);
        FD_CLR(sd, &iom.master);
        return 1;
    }
    else
    {
        printf("La richiesta è valida ed entrerà in gestione\n");
        strcpy(sPeer.richiesta_id, richiesta_id);
        sPeer.richiesta_in_gestione = 1;
        sPeer.risposte_mancanti = 0;
        sPeer.sd_flood = sd;
        sPeer.numero_peer_da_contattare = 0;
        for (i = 0; i < MAX_PEER_DA_CONTATTARE; i++)
            sPeer.peer_da_contattare[i] = 0;

        return 0;
    }
}

//funzione che gestisce l'arrivo di una richiesta di Flood
void sPeer_gestioneFlood(int sd)
{
    int ret, len, requester;
    uint16_t lmsg;
    char buffer[1024], richiesta_id[20], tmp[20];

    ret = recv(sd, (void *)&lmsg, sizeof(uint16_t), 0);
    if (ret < 0)
    {
        perror("Errore in fase di ricezione: ");
        exit(-1);
    }

    len = ntohs(lmsg);
    ret = recv(sd, (void *)buffer, len, 0);
    if (ret < 0)
    {
        perror("Errore in fase di ricezione: ");
        exit(1);
    }

    sscanf(buffer, "%s %s %d", richiesta_id, tmp, &requester);
    strcat(richiesta_id, " ");
    strcat(richiesta_id, tmp);
    printf("Mi è arrivata una richiesta di Flood da %d\n", requester);

    if (sPeer_verificaRichiestaRicevuta(richiesta_id, sd)) //se la richiesta è valida si continua se no ci si ferma
        return;

    sPeer.requester = requester;

    RegistroGenerale_ricevoDateNecessarie(sd);
    ArchivioAggregazioni_verificaPresenzaDati(1); //elimina dalla lista delle date necessarie le date complete contenute nel RegistroGenerale e
    //se trova dei registri utili si inserisce nella lista dei peer da contattare
    sPeer_richiestaFlood(requester); //inoltro la richiesta ai miei vicini, la richiesta inoltrata mancherà delle date che io ho complete
    if (sPeer.risposte_mancanti == 0)
    {
        sPeer.richiesta_in_gestione = 0;
        sPeer_invioRispostaFlood();
    }
}

//funzione che chiama le giuste funzione in base alle operazione richieste dall'utente
void sPeer_menu()
{
    char buffer[1024], ds_addr[20], type[20], aggr[20], period[50], tmp[20];
    int ds_port, quantity;

    fgets(buffer, 100, stdin);
    sscanf(buffer, "%s", tmp);
    printf("\n");
    if (strcmp("help", tmp) == 0)
    {
        sPeer_help();
    }
    else if (strcmp("start", tmp) == 0)
    {
        if (sscanf(buffer, "%s %s %d", tmp, ds_addr, &ds_port) == 3)
        {
            sPeer_serverBoot(ds_addr, ds_port);
        }
        else
        {
            printf("Opzione non valida\n\n");
        }
    }
    else if (strcmp("add", tmp) == 0)
    {
        if (sscanf(buffer, "%s %s", tmp, type) == 2)
        {
            if (strcmp(type, "tampone") == 0)
            {
                if (sscanf(buffer, "%s %s %d", tmp, type, &quantity) == 3)
                {
                    RegistroGiornaliero_aggiungiEntry(type, quantity);
                }
                else
                    printf("Opzione non valida\n\n");
            }
            else if (strcmp(type, "nuovo") == 0)
            {
                if (sscanf(buffer, "%s %s %*s %d", tmp, type, &quantity) == 3)
                {
                    RegistroGiornaliero_aggiungiEntry(type, quantity);
                }
                else
                    printf("Opzione non valida\n\n");
            }
            else
                printf("Tipo non valido\n\n");
        }
        else
            printf("Opzione non valida\n\n");
    }
    else if (strcmp("get", tmp) == 0)
    {
        if (sscanf(buffer, "%s %s %s", tmp, aggr, type) == 3)
        {
            if (strcmp(type, "tampone") == 0)
            {
                if (sscanf(buffer, "%s %s %s %s", tmp, aggr, type, period) == 4)
                {
                    ArchivioAggregazioni_gestioneAggregazione(aggr, type, period);
                }
                else if (sscanf(buffer, "%s %s %s %s", tmp, aggr, type, period) == 3)
                {
                    strcpy(period, " ");
                    ArchivioAggregazioni_gestioneAggregazione(aggr, type, period);
                }
                else
                    printf("Opzione non valida\n\n");
            }
            else if (strcmp(type, "nuovo") == 0)
            {
                if (sscanf(buffer, "%s %s %s %*s %s", tmp, aggr, type, period) == 4)
                {
                    ArchivioAggregazioni_gestioneAggregazione(aggr, type, period);
                }
                else if (sscanf(buffer, "%s %s %s %*s %s", tmp, aggr, type, period) == 3)
                {
                    strcpy(period, " ");
                    ArchivioAggregazioni_gestioneAggregazione(aggr, type, period);
                }
                else
                    printf("Opzione non valida\n\n");
            }
            else
                printf("Tipo non valido\n\n");
        }
        else
            printf("Opzione non valida\n\n");
    }
    else if (strcmp("stop", tmp) == 0)
    {
        sPeer_stop();
    }
    else if (strcmp("showneighbors", tmp) == 0)
    {
        sPeer_showNeighbors();
    }
    else
    {
        printf("Opzione non valida\n\n");
    }

    printf("\n>> ");
    fflush(stdout);
}

//funzone che gestisce l'arrivo di una richiesta di registri
void sPeer_gestioneRichiestaRegistri(int sd)
{
    printf("Mi è arrivata una richiesta di Registri\n");
    RegistroGenerale_ricevoDateNecessarieECreoRegistri(sd);
    RegistroGenerale_modificoRegistriRichiesti();
    RegistroGenerale_invioRegistriRichiesti(sd);
    close(sd);
    FD_CLR(sd, &iom.master);
}

//funzione che gestisce l'arrivo di una comunicazione di nuovi vicini da parte del Ds
// questi verranno a sostituire i precedenti, in quanto viene sempre inviata l'intera lista aggiornata
void sPeer_gestisciNuoviVicini(int sd)
{
    int ret, num, len;
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

    printf("Mi sono stati comunicati i %d nuovi vicini: %s\n\n", num, neighbors);
    // il vecchio vettore verrà completamente sostituito
    CVECTOR_FREE((sPeer.vicini));
    cvector_init(&(sPeer.vicini));

    // first token
    token = strtok(neighbors, delim);
    // walk through the other tokens
    while (token != NULL) {
        if(atoi(token) != sPeer.port) { // un peer non deve avere se stesso come vicino
            DEBUG_PRINT(("I add (%d) because i'm (%d) and i'm different\n", atoi(token), sPeer.port));
            CVECTOR_ADD((sPeer.vicini), atoi(token));
        }
        token = strtok(NULL, delim);
    }

    close(sd);
    FD_CLR(sd, &iom.master);
}

//funzione che gestisce l'arrivo di un messaggio di stop da parte del Ds
void sPeer_dsStop()
{
    printf("Mi arrivato un messaggio di stop da parte del Ds\n");
    FileManager_salvaArchivioAggregazioni();
    FileManager_salvaRegistro(1);
    exit(0);
}

void sPeer_handleTCP(char* buffer, int sd)
{
    if (strcmp(buffer, "REQAG") == 0) //messaggio di richiesta di aggregazione 
        sPeer_gestioneRichiestaAggregazione(sd);
    else if (strcmp(buffer, "REQRE") == 0) //messaggio di richiesta di uno o più registri
        sPeer_gestioneRichiestaRegistri(sd);
    else if (strcmp(buffer, "FLOOD") == 0) //messaggio di inoltro di una FLOOD_FOR_ENTRIES
        sPeer_gestioneFlood(sd);
    else if (strcmp(buffer, "FLORE") == 0) //messaggio di risposta ad un richiesta di FLOOD_FOR_ENTRIES
        sPeer_ricezioneRispostaFlood(sd); // FIXUP: non ce ne dovrebbe essere bisogno se flooding è gestito da altro proc
    else if (strcmp(buffer, "REQST") == 0) //messaggio di stop da parte di un neighbor
        sPeer_gestisciStop(sd);
    else if (strcmp(buffer, "UPDVI") == 0) //messaggio di comunicazione di nuovi vicini da parte del DS
        sPeer_gestisciNuoviVicini(sd);
    else if (strcmp(buffer, "DSTOP") == 0) //messaggio di stop da parte del DS
        sPeer_dsStop();
    else
        printf("Ricevuto messaggio non valido\n");
}

void sPeer_handlSTDIN()
{
    if (sPeer.richiesta_in_gestione == 0)
        sPeer_menu();
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
    sPeer_initialize(argv[1]);


    sPeer_help();
    printf("\n>> ");
    fflush(stdout);
    #ifdef DEBUG_ON
    sPeer_serverBoot("127.0.0.1", 4242); // just to save time
    #endif
    IOMultiplex(sPeer.port, &iom, false, sPeer_menu, emptyFunc, sPeer_handleTCP);    
    return 0;
}
