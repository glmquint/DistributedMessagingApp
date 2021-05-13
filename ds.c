#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include "vector.h"
#include "network.h"

#define DEBUG_ON

void menu()
{
    printf("questo è il menu!\n");
}

void handleCmd(char* cmd)
{
    #ifdef DEBUG_ON
    printf("handling: %s\n", cmd);
    #endif
    menu();
}

void handleBootReq(int sd, char* cmd)
{
    #ifdef DEBUG_ON
    printf("handling boot from %d: %s\n", sd, cmd);
    #endif
}

void handleReq(int sd, char* cmd)
{
    #ifdef DEBUG_ON
    printf("handling req from %d: %s\n", sd, cmd);
    //sleep(5); // simulating latency
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
    int ds_port;
    sscanf(argv[1], "%d", &ds_port);
    printf("sono ds: %d\n", ds_port);
    menu();
    fflush(stdout);
    IOMultiplex(ds_port, true, handleCmd, handleBootReq, handleReq, handleTimeout);
}
