#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include "vector.h"
#include "network.h"

#define DEBUG_ON

struct Peer{
    int port;
    cvector neighbors;
    vector pending_requests;
}peer;

void initialize(char* port)
{
    sscanf(port, "%d", &peer.port);
    printf("sono peer: %d\n", peer.port);
}

void menu()
{
    printf("questo è il menu del peer!\n");
}

void handleCmd(char* cmd)
{
    #ifdef DEBUG_ON
    printf("handling: %s\n", cmd);
    #endif
    menu();
}

// will never be called if use_udp is set to false in IOMultiplexer
void emptyFunc(int sd, char* cmd)
{
    return;
}

void handleReq(int sd, char* cmd)
{
    #ifdef DEBUG_ON
    printf("handling req from %d: %s\n", sd, cmd);
    sleep(5);
    printf("done with request\n");
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
