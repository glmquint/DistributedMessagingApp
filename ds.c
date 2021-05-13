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

struct PeerRecord {
    int port;
    cvector neighbors;
};

struct PeerRegister {
    vector peers;
} peerRegister;

void menu()
{
    DEBUG_PRINT(("questo è il menu!\n"));
}

void handleCmd(char* cmd)
{
    DEBUG_PRINT(("handling: %s\n", cmd));
    menu();
}

void handleBootReq(int sd, char* cmd)
{
    DEBUG_PRINT(("handling boot from %d: %s\n", sd, cmd));
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
    int ds_port;
    sscanf(argv[1], "%d", &ds_port);
    DEBUG_PRINT(("sono ds: %d\n", ds_port));
    menu();
    fflush(stdout);
    IOMultiplex(ds_port, true, handleCmd, handleBootReq, handleReq, handleTimeout);
}
