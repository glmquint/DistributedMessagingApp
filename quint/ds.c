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

#define DEBUG_ON

#ifdef DEBUG_ON
# define DEBUG_PRINT(x) printf x
#else
# define DEBUG_PRINT(x) do {} while (0)
#endif


#define STDIN 0
#define BOOT_MSG 6  //"10000\0"
#define BOOT_RESP 12 //"10001 10002\0"
#define REQ_LEN 6 //RSTOP\0 REQVI\0

fd_set master;

struct DiscoveryServer {
    int port;
}DiscoveryServer;

struct PeerRecord {
    int port;
    cvector neighbors;
};

struct PeerRegister {
    vector peers;
} peerRegister;



void Ds_initialize(char* port)
{
    sscanf(port, "%d", &DiscoveryServer.port);
    DEBUG_PRINT(("sono DiscoveryServer: %d\n", DiscoveryServer.port));
    vector_init(&(peerRegister.peers));
}


//funzione che imposta la stringa RegistroPeer.boot_vicini con i vicini del peer passato come parametro
//l'implementazione della funzione stabilisce la topologie della rete
void Ds_restituisciVicini(int porta)
{
    /*
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
    */
}

//funzione che stampa nel dettaglio le operazioni del Ds
void Ds_help()
{
    printf("help --> mostra i dettagli sui comandi accettati\n\n");
    printf("showpeers --> mostra l’elenco dei peer connessi al network (IP e porta)\n\n");
    printf("showneighbor <peer> --> mostra i neighbor di un peer specificato come parametro opzionale. Se non c’è il parametro, il comando mostra i neighbor di ogni peer\n\n");
    printf("esc --> termina  il  DS. La terminazione del DS causa la terminazione di tutti i peer. Opzionalmente, prima di chiudersi, i peerpossono salvare le loro informazioni su un file, ricaricato nel momento in cui un peer torna a far parte del network (esegue il boot).\n\n");
}


//FIXUP: refactor with showneighbors(int, bool)
//funzione che mostra la lista di tutti i peer connessi
void Ds_showpeers()
{
    printf("Questi sono i peers della rete:\n");
    for (int i = 0; i < VECTOR_TOTAL((peerRegister.peers)); i++) {
        struct PeerRecord* peer_r = VECTOR_GET((peerRegister.peers), struct PeerRecord*, i);
        printf("[%d]: %d\n", i, peer_r->port);
    }

}

//funzione che mosta la lista dei vicini del peer passato come parametro, se il parametro è < 0 viene mostrata la lista dei vicini di tutti i peer connessi
void Ds_showneighbor(int peer)
{
    for (int i = 0; i < VECTOR_TOTAL((peerRegister.peers)); i++) {
        struct PeerRecord* peer_r = VECTOR_GET((peerRegister.peers), struct PeerRecord*, i);
        if (peer < 0 || peer == peer_r->port) {
            printf("\nshowing peer: %d\nHis neighbors are:\n", peer_r->port);
            for (int j = 0; j < CVECTOR_TOTAL((peer_r->neighbors)); j++) {
                printf("[%d] %d\n", j, CVECTOR_GET((peer_r->neighbors), int, j));
            }
        }
    }
}

//funzione che invia a tutti i peer della rete il messagio DSTOP e che fa terminare il Ds
void Ds_esc()
{
    /*
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
    */
    DEBUG_PRINT(("Goodbye!!\n"));
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
            Ds_showneighbor(-1);    
    }
    else if (strcmp("esc", tmp) == 0)
    {
        Ds_esc();
    }
    else
    {
        printf("Opzione non valida\n\n");
    }
    printf("\n>> ");
    fflush(stdout);
}

//FIXUP: delete this as we shouldn't need it. It's all handled by void Ds_peerRegistration(int sd)
//funzione che aggiunge un peer alla lista circolare dei peer ordinata per numero di porta
/*void Ds_aggiungiPeer(int porta)
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
    
}*/

//FIXUP: delete this as we shouldn't need it. It's all handled by void Ds_peerRegistration(int sd)
//funzione che rimuove un peer dalla lista dei peer
/*
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
 
}   */

//FIXUP: test
void sendNeighborsUpdate(int peer, cvector new_neighbors)
{
    int ret, sd, len;
    uint16_t lmsg;
    struct sockaddr_in peer_addr;
    char buffer[1024];
    char nei_list[1024];
    memset(buffer, 0, sizeof(buffer));

    if (peer <= 0)
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

    // text-protocol serialization of CVECTOR
    memset(nei_list, 0, sizeof(nei_list));
    for (int i = 0; i < CVECTOR_TOTAL(new_neighbors); i++) {
        char* nei_port = malloc(6);
        sprintf(nei_port, "%d", (int) CVECTOR_GET((new_neighbors), int, i));
        strcat(strcat(nei_list, ":"), nei_port);
        free(nei_port);
    }

    memset(buffer, 0, sizeof(buffer));
    sprintf(buffer, "%d %s", CVECTOR_TOTAL(new_neighbors), nei_list); 

    len = strlen(buffer);
    lmsg = htons(len);
    ret = send(sd, (void*) &lmsg, sizeof(uint16_t), 0);
    if (ret < 0)
    {
        perror("Errore nell'invio lunghezza messaggio: ");
        exit(1);
    }
    ret = send(sd, (void*) &buffer, len, 0);
    if (ret < 0)
    {
        perror("Errore nell'invio del messaggio: ");
        exit(1);
    }

    close(sd);

/*
    strcpy(buffer, RegistroPeer.boot_vicini);

    printf("Comunico al peer %d i suoi nuovi vicini\n\n", peer);
    ret = send(sd, (void *)buffer, BOOT_RESP, 0);
    if (ret < 0)
    {
        perror("Errore in fase di invio: ");
        exit(1);
    }
    */
}



//funzione che gestisce il boot di un peer
void Ds_peerRegistration(int sd)
{
    int ret, new_peer_port, peers_num, len;
    uint16_t lmsg;
    socklen_t addrlen;
    char buffer[1024];
    struct sockaddr_in connecting_addr;

    struct PeerRecord* newPeer;
    char answer[1024];
    char neighbors_list[1000];
    memset(answer, 0, sizeof(answer));
    memset(neighbors_list, 0, sizeof(neighbors_list));
    
    addrlen = sizeof(connecting_addr);
    ret = recvfrom(sd, buffer, BOOT_MSG, 0, (struct sockaddr *)&connecting_addr, &addrlen);
    if (ret < 0)
    {
        perror("Errore nella ricezione: ");
        exit(1);
    }
    sscanf(buffer, "%d", &new_peer_port);
    printf("Gestisco il boot del peer %d\n", new_peer_port);

    peers_num = VECTOR_TOTAL((peerRegister.peers));

    newPeer = (struct PeerRecord*) malloc(sizeof(struct PeerRecord));
    newPeer->port = new_peer_port;
    cvector_init(&(newPeer->neighbors));
    if (peers_num == 0) {
        strcpy(answer, "0 :)"); // the string part is useless
        DEBUG_PRINT(("no peers yet, you are the first! sending: %s\n", answer));
    } else {
        // zukunfty algoritm
        int new_connections = 0;
        int i = 1;
        while (peers_num % i == 0) {
            char* nei_port = malloc(6);
            struct PeerRecord* neighbor =  VECTOR_GET((peerRegister.peers), struct PeerRecord*, peers_num - i);
            DEBUG_PRINT(("new neighbor to add: %d\n", neighbor->port));
            CVECTOR_ADD((newPeer->neighbors), neighbor->port);

            sprintf(nei_port, "%d", neighbor->port);
            strcat(strcat(neighbors_list, ":"), nei_port);
            free(nei_port);
            DEBUG_PRINT(("now neighbors_list is: %s\n", neighbors_list));

            // and vice versa
            DEBUG_PRINT(("and %d should update to include: %d\n", neighbor->port, new_peer_port));
            CVECTOR_ADD((neighbor->neighbors), new_peer_port);
            sendNeighborsUpdate(neighbor->port, neighbor->neighbors);

            new_connections++;
            i *= 2;
        }
        sprintf(answer, "%d %s", new_connections, neighbors_list); 
        DEBUG_PRINT(("done with new neighbors\n\n")); //FIXUP: better log (probably on sPeer_serverBoot)
    }
    VECTOR_ADD((peerRegister.peers), newPeer);
    len = strlen(answer);
    lmsg = htons(len);
    ret = sendto(sd, (void*) &lmsg, sizeof(uint16_t), 0, (struct sockaddr *)&connecting_addr, sizeof(connecting_addr));
    if (ret < 0)
    {
        perror("Errore nell'invio lunghezza messaggio: ");
        exit(1);
    }
    ret = sendto(sd, (void*) answer, len, 0, (struct sockaddr *)&connecting_addr, sizeof(connecting_addr));
    if (ret < 0)
    {
        perror("Errore nell'invio del messaggio: ");
        exit(1);
    }
    /*
    Ds_aggiungiPeer(new_peer_port);
    Ds_restituisciVicini(new_peer_port);
    strcpy(buffer, RegistroPeer.boot_vicini);

    ret = sendto(sd, buffer, BOOT_RESP, 0, (struct sockaddr *)&connecting_addr, sizeof(connecting_addr));
    if (ret < 0)
    {
        perror("Errore nell'invio: ");
        exit(1);
    }

    sscanf(buffer, "%d %d", &new_peer_port1, &new_peer_port2);
    Ds_comunicaNuoviVicini(new_peer_port1);
    Ds_comunicaNuoviVicini(new_peer_port2);
    */
}

//funzione che gestisce la disconnessione da parte di un peer
void Ds_handleDisconnectReq(int sd)
{
    int ret, peer;
    uint16_t lmsg;
    struct PeerRecord* last_peer;

    ret = recv(sd, (void *)&lmsg, sizeof(uint16_t), 0);
    if (ret < 0)
    {
        perror("Errore in fase di ricezione: ");
        exit(1);
    }

    peer = ntohs(lmsg);
    printf("Gestisco la disconnessione del peer %d\n", peer);

    DEBUG_PRINT(("This is a description of the traitor:\n"));
    Ds_showneighbor(peer);
    last_peer = (struct PeerRecord*) VECTOR_GET((peerRegister.peers), void*, -1);
    DEBUG_PRINT(("He is gonna be replaced by:\n"));
    Ds_showneighbor(last_peer->port);

    //FIXUP: actually change peerRegister with new neighborhood, whith last_peer shift, as per the zukunfty algoritm

    /*
    Ds_restituisciVicini(peer);
    sscanf(RegistroPeer.boot_vicini, "%d %d", &peer1, &peer2);
    Ds_rimuoviPeer(peer);
    Ds_comunicaNuoviVicini(peer1);
    Ds_comunicaNuoviVicini(peer2);
    */

    close(sd);
    FD_CLR(sd, &master);
}

void DS_handleTCP(char* buffer, int sd)
{
    if (strcmp(buffer, "RSTOP") == 0) //messaggio di disconnessione da parte di un peer
        Ds_handleDisconnectReq(sd);
    else
        printf("Ricevuto messaggio non valido\n");
}


//main del Ds
int main(int argc, char *argv[])
{
    Ds_initialize(argv[1]);
    Ds_help();
    printf("\n>> ");
    fflush(stdout);
    IOMultiplex(DiscoveryServer.port, &iom, true, Ds_menu, Ds_peerRegistration, DS_handleTCP);
    return 0;
}