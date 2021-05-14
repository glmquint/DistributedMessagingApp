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

struct DiscoveryServer {
    int port;
}Ds;

struct PeerRecord {
    int port;
    cvector neighbors;
};

struct PeerRegister {
    vector peers;
} peerRegister;

void initialize(char* port)
{
    sscanf(port, "%d", &Ds.port);
    DEBUG_PRINT(("sono DiscoveryServer: %d\n", Ds.port));
    vector_init(&(peerRegister.peers));
}

void showNeighbors(int peer)
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

void menu()
{
    DEBUG_PRINT(("questo è il menu!\n"));
    printf("******************** DS COVID STARTED ********************\n");
    printf("Digita un comando:\n\n");
    printf("1) help --> mostra i dettagli dei comandi\n");
    printf("2) showpeers --> mostra un elenco dei peer connessi\n");
    printf("3) showneighbor <peer> --> mostra i neighbor di un peer\n");
    printf("4) esc --> chiude il DS\n\n");
}

void handleCmd(char* cmd)
{
    char tmp[1024];
    int peer;

    DEBUG_PRINT(("handling cmd: %s\n", cmd));
    sscanf(cmd, "%s", tmp);
    printf("\n");
    if (strcmp("help", tmp) == 0)
    {
        menu();
    }
    else if (strcmp("showpeers", tmp) == 0)
    {
        DEBUG_PRINT(("show me the peers!!\n\n"));
    }
    else if (strcmp("showneighbor", tmp) == 0)
    {
        if(sscanf(cmd, "%s %d", tmp, &peer) == 2) {
            DEBUG_PRINT(("show me neighbors of %d\n\n", peer));
            showNeighbors(peer);
        } else {
            DEBUG_PRINT(("show me all the neighbors\n\n"));
            showNeighbors(-1);
        }
    }
    else if (strcmp("esc", tmp) == 0)
    {
        DEBUG_PRINT(("esc!!\n\n"));
    }
    else
    {
        printf("comando non valido\n\n");
    }
    printf("\n>> ");
    fflush(stdout);
}

void handleBootReq(int sd, char* cmd, char* answer)
{
    int new_peer_port, peers_num;
    struct PeerRecord* newPeer;
    char neighbors_list[1000];
    memset(neighbors_list, 0, sizeof(neighbors_list));
    DEBUG_PRINT(("handling boot from %d: %s\n", sd, cmd));
    new_peer_port = atoi(cmd);
    peers_num = VECTOR_TOTAL((peerRegister.peers));

    newPeer = (struct PeerRecord*) malloc(sizeof(struct PeerRecord));
    newPeer->port = new_peer_port;
    cvector_init(&(newPeer->neighbors));
    if (peers_num == 0) {
        strcpy(answer, "0 :P"); // the string part is useless
        DEBUG_PRINT(("no peers yet, you are the first! sending: %s\n", answer));
    } else {
        // zukunfty algoritm
        int new_connections = 0;
        int i = 1;
        while (peers_num % i == 0) {
            char* nei_port = malloc(4);
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
        DEBUG_PRINT(("done with new neighbors\n\n"));
    }
    VECTOR_ADD((peerRegister.peers), newPeer);
        
}

void handleReq(int sd, char* cmd)
{
    DEBUG_PRINT(("handling req from %d: %s\n", sd, cmd));
    //sleep(5); // simulating latency
    DEBUG_PRINT(("done with request\n"));
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
    printf("\n>> ");
    fflush(stdout);
    IOMultiplex(Ds.port, true, handleCmd, handleBootReq, handleReq, handleTimeout);
}
