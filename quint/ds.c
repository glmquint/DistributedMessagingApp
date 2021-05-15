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
#include "vector.h"
#include "IOMultiplex.h"

#define STDIN 0
#define BOOT_MSG 6  //"10000\0"
#define BOOT_RESP 12 //"10001 10002\0"
#define REQ_LEN 6 //RSTOP\0 REQVI\0

fd_set master;

// FIXUP: change structures to include vector and cvector
//
// DS should have a vector PeerRegister of PeerRecord,
// each one with its own int port and cvector of neighbors
struct Peer
{
    int porta;
    struct Peer *next_peer, *prev_peer;
};

struct RegistroPeer
{
    struct Peer *lista_peer;
    int numero_peer;
    char boot_vicini[BOOT_RESP];
} RegistroPeer;

struct DiscoveryServer
{
    int portaserver;
} DiscoveryServer;

//funzione che imposta la stringa RegistroPeer.boot_vicini con i vicini del peer passato come parametro
//l'implementazione della funzione stabilisce la topologie della rete
void Ds_restituisciVicini(int porta)
{
    struct Peer *pp, *prevp;
    int porta1, porta2;

    printf("Stabilisco i vicini del peer %d\n", porta);
    prevp = 0;
    pp = RegistroPeer.lista_peer;
    while (pp != 0)
    {
        if (pp->porta == porta) //trovo il peer cercato
            break;
        prevp = pp;
        pp = pp->next_peer;
        if(prevp->porta >= pp->porta){
            printf("Peer non registrato\n");
            return;
        }
    }

    porta1 = pp->prev_peer->porta;
    if(porta1 == pp->porta) porta1 = 0;

    porta2 = pp->next_peer->porta;
    if(porta2 == pp->porta) porta2 = 0;

    if(porta1 == porta2) porta2 = 0;
    
    //una volta trovate le 2 porte più vicine le restituisco come stringa
    printf("Vicini impostati: %d %d\n\n", porta1, porta2);
    sprintf(RegistroPeer.boot_vicini, "%d %d", porta1, porta2);
}

//funzione che stampa nel dettaglio le operazioni del Ds
void Ds_help()
{
    printf("help --> mostra i dettagli sui comandi accettati\n\n");
    printf("showpeers --> mostra l’elenco dei peer connessi al network (IP e porta)\n\n");
    printf("showneighbor <peer> --> mostra i neighbor di un peer specificato come parametro opzionale. Se non c’è il parametro, il comando mostra i neighbor di ogni peer\n\n");
    printf("esc --> termina  il  DS. La terminazione del DS causa la terminazione di tutti i peer. Opzionalmente, prima di chiudersi, i peerpossono salvare le loro informazioni su un file, ricaricato nel momento in cui un peer torna a far parte del network (esegue il boot).\n\n");
}

//funzione che mostra la lista di tutti i peer connessi
void Ds_showpeers()
{
    struct Peer *pp;

    pp = RegistroPeer.lista_peer;
    printf("Questi sono i peer del network: \n\n");
    while (pp != 0)
    {
        printf("%d\n", pp->porta);
        pp = pp->next_peer;
        if (pp->porta == RegistroPeer.lista_peer->porta)
            break;
    }
    printf("\n");
}

//funzione che mosta la lista dei vicini del peer passato come parametro, se il parametro è 0 viene mostrata la lista dei vicini di tutti i peer connessi
void Ds_showneighbor(int peer)
{
    char buffer[12];
    if (peer)
    {
        Ds_restituisciVicini(peer); //questa funzione scrive in RegistroPeer.boot_vicini i vicini del peer passato come parametro
        strcpy(buffer, RegistroPeer.boot_vicini);
        printf("Questi sono i vicini del peer: %s\n\n", buffer);
    }
    else
    {
        //mi scorro tutta la lista dei peer e per ogni peer chiamo la funzione che me ne restituisce i vicini
        struct Peer *pp;

        pp = RegistroPeer.lista_peer;
        printf("Questa è la lista dei peer e dei loro vicini: \n\n");
        while (pp != 0)
        {
            Ds_restituisciVicini(pp->porta);
            strcpy(buffer, RegistroPeer.boot_vicini);
            printf("%d: %s\n", pp->porta, buffer);
            pp = pp->next_peer;
            if (pp->porta == RegistroPeer.lista_peer->porta)
                break;
        }
        printf("\n");
    }
}

//funzione che invia a tutti i peer della rete il messagio DSTOP e che fa terminare il Ds
void Ds_esc()
{
    int sd, ret, i = 0;
    struct sockaddr_in peer_addr;
    struct Peer *pp;
    char buffer[1024];

    strcpy(buffer, "DSTOP");

    
    pp = RegistroPeer.lista_peer;
    for (i = 0; i < RegistroPeer.numero_peer; i++)
    {
        sd = socket(AF_INET, SOCK_STREAM, 0);
        memset(&peer_addr, 0, sizeof(peer_addr));
        peer_addr.sin_family = AF_INET;
        peer_addr.sin_port = htons(pp->porta);
        inet_pton(AF_INET, "127.0.0.1", &peer_addr.sin_addr);

        printf("Invio al peer %d il messaggio DSTOP\n", pp->porta);
        ret = connect(sd, (struct sockaddr*)&peer_addr, sizeof(peer_addr));
        if(ret < 0){
            perror("Errore in fase di connessione:");
            exit(1);
        }

        ret = send(sd, (void*)buffer, REQ_LEN, 0);
        if(ret < 0){
            perror("Errore in fase di invio:");
            exit(1);
        }

        pp = pp->next_peer;
        close(sd);
    } 
    exit(0);
}

//funzione che stabilisce le funzioni da chiamare in base all'input dell'utente
void Ds_menu()
{
    char buffer[20], tmp[20];
    int peer;

    fgets(buffer, 100, stdin);
    sscanf(buffer, "%s", tmp);
    printf("\n");
    if (strcmp("help", tmp) == 0)
    {
        Ds_help();
    }
    else if (strcmp("showpeers", tmp) == 0)
    {
        Ds_showpeers();
    }
    else if (strcmp("showneighbor", tmp) == 0)
    {
        if(sscanf(buffer, "%s %d", tmp, &peer) == 2)
            Ds_showneighbor(peer);
        else
            Ds_showneighbor(0);    
    }
    else if (strcmp("esc", tmp) == 0)
    {
        Ds_esc();
    }
    else
    {
        printf("Opzione non valida\n\n");
    }
}

//funzione che aggiunge un peer alla lista circolare dei peer ordinata per numero di porta
void Ds_aggiungiPeer(int porta)
{
    struct Peer *new_peer = malloc(sizeof(struct Peer));
    new_peer->porta = porta;

    struct Peer *pp, *prevp;
    pp = RegistroPeer.lista_peer;
    prevp = 0;

    while (pp != 0 && (pp->porta < new_peer->porta))
    {
        prevp = pp;
        pp = pp->next_peer;
        if (prevp->porta >= pp->porta)
            break;
    }

    if (prevp)
    {
        if (prevp->porta == porta)
        {
            printf("Peer %d non aggiunto alla lista perchè già presente\n", porta);
        }
        prevp->next_peer = new_peer;
        new_peer->prev_peer = prevp;
    }
    else
        RegistroPeer.lista_peer = new_peer;

    new_peer->next_peer = pp;
    if(pp)
        pp->prev_peer = new_peer;

    if (new_peer->next_peer == 0)
        new_peer->next_peer = RegistroPeer.lista_peer;

    prevp = 0;
    pp = RegistroPeer.lista_peer;
    while(pp){
        prevp = pp;
        pp = pp->next_peer;
        if (prevp->porta >= pp->porta)
            break;
    }
    RegistroPeer.lista_peer->prev_peer = prevp;
    prevp->next_peer = RegistroPeer.lista_peer;
    RegistroPeer.numero_peer++;
    printf("Peer %d aggiunto correttamente alla lista dei peer connessi\n", porta);
}

//funzione che rimuove un peer dalla lista dei peer
void Ds_rimuoviPeer(int porta)
{
    struct Peer *pp, *prevp;

    printf("Rimuovo il peer %d dalla lista dei peer\n", porta);
    prevp = 0;
    pp = RegistroPeer.lista_peer;

    while (pp != 0)
    {
        if (pp->porta == porta)
            break;
        prevp = pp;
        pp = pp->next_peer;
    }

    if (prevp == 0)
    {
        if(pp->porta == pp->next_peer->porta) RegistroPeer.lista_peer = 0;
        else RegistroPeer.lista_peer = pp->next_peer;
    }     
    else
    {
        prevp->next_peer = pp->next_peer;
        pp->next_peer->prev_peer = prevp;
    }
    prevp = 0;
    pp = RegistroPeer.lista_peer;
    while(pp){
        prevp = pp;
        pp = pp->next_peer;
        if (prevp->porta >= pp->porta)
            break;
    }
    if(RegistroPeer.lista_peer) RegistroPeer.lista_peer->prev_peer = prevp;
    if(prevp) prevp->next_peer = RegistroPeer.lista_peer;

    RegistroPeer.numero_peer--;
}

//funzione che comunica al peer passato come parametro i suoi nuovi vicini
void Ds_comunicaNuoviVicini(int peer)
{
    int ret, sd;
    struct sockaddr_in peer_addr;
    char buffer[1024];

    if (peer == 0)
        return;

    sd = socket(AF_INET, SOCK_STREAM, 0);

    memset(&peer_addr, 0, sizeof(peer_addr));
    peer_addr.sin_family = AF_INET;
    peer_addr.sin_port = htons(peer);
    inet_pton(AF_INET, "127.0.0.1", &peer_addr.sin_addr);

    ret = connect(sd, (struct sockaddr *)&peer_addr, sizeof(peer_addr));
    if (ret < 0)
    {
        perror("Errore in fase di connessione: ");
        exit(-1);
    }

    strcpy(buffer, "UPDVI");

    ret = send(sd, (void *)buffer, REQ_LEN, 0);
    if (ret < 0)
    {
        perror("Errore in fase di invio: ");
        exit(1);
    }

    Ds_restituisciVicini(peer);
    strcpy(buffer, RegistroPeer.boot_vicini);

    printf("Comunico al peer %d i suoi nuovi vicini\n\n", peer);
    ret = send(sd, (void *)buffer, BOOT_RESP, 0);
    if (ret < 0)
    {
        perror("Errore in fase di invio: ");
        exit(1);
    }
}

//funzione che gestisce il boot di un peer
void Ds_peerRegistration(int sd)
{
    int ret, porta, porta1, porta2;
    socklen_t addrlen;
    char buffer[1024];
    struct sockaddr_in connecting_addr;

    addrlen = sizeof(connecting_addr);
    ret = recvfrom(sd, buffer, BOOT_MSG, 0, (struct sockaddr *)&connecting_addr, &addrlen);
    if (ret < 0)
    {
        perror("Errore nella ricezione: ");
        exit(1);
    }
    sscanf(buffer, "%d", &porta);
    printf("Gestisco il boot del peer %d\n", porta);
    Ds_aggiungiPeer(porta);
    Ds_restituisciVicini(porta);
    strcpy(buffer, RegistroPeer.boot_vicini);

    ret = sendto(sd, buffer, BOOT_RESP, 0, (struct sockaddr *)&connecting_addr, sizeof(connecting_addr));
    if (ret < 0)
    {
        perror("Errore nell'invio: ");
        exit(1);
    }

    sscanf(buffer, "%d %d", &porta1, &porta2);
    Ds_comunicaNuoviVicini(porta1);
    Ds_comunicaNuoviVicini(porta2);
}

//funzione che gestisce la disconnessione da parte di un peer
void Ds_gestioneDisconnessione(int sd)
{
    int ret, peer, peer1, peer2;
    uint16_t lmsg;

    ret = recv(sd, (void *)&lmsg, sizeof(uint16_t), 0);
    if (ret < 0)
    {
        perror("Errore in fase di ricezione: ");
        exit(1);
    }

    peer = ntohs(lmsg);
    printf("Gestisco la disconnessione del peer %d\n", peer);
    Ds_restituisciVicini(peer);
    sscanf(RegistroPeer.boot_vicini, "%d %d", &peer1, &peer2);
    Ds_rimuoviPeer(peer);
    Ds_comunicaNuoviVicini(peer1);
    Ds_comunicaNuoviVicini(peer2);

    close(sd);
    FD_CLR(sd, &master);
}

void DS_handleTCP(char* buffer, int sd)
{
    if (strcmp(buffer, "RSTOP") == 0) //messaggio di disconnessione da parte di un peer
        Ds_gestioneDisconnessione(sd);
    else
        printf("Ricevuto messaggio non valido\n");
}

void DS_handleUDP(int boot)
{
    Ds_peerRegistration(boot);
}

void DS_handleSTDIN()
{
    Ds_menu();
}



//main del Ds
int main(int argc, char *argv[])
{
    sscanf(argv[1], "%d", &DiscoveryServer.portaserver);
    printf("******************** DS COVID STARTED ********************\n");
    printf("Digita un comando:\n\n");
    printf("1) help --> mostra i dettagli dei comandi\n");
    printf("2) showpeers --> mostra un elenco dei peer connessi\n");
    printf("3) showneighbor <peer> --> mostra i neighbor di un peer\n");
    printf("4) esc --> chiude il DS\n\n");
    IOMultiplex(DiscoveryServer.portaserver, &iom, true, DS_handleSTDIN, DS_handleUDP, DS_handleTCP);
    return 0;
}