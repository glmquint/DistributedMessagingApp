#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include "util/IOMultiplex.h"
#include "util/cmd.h"
#include "util/network.h"

#define CMDLIST_LEN 3

#define DEBUG_ON

#ifdef DEBUG_ON
# define DEBUG_PRINT(x) printf("[DEBUG]: "); printf x; printf("\n"); fflush(stdout)
#else
# define DEBUG_PRINT(x) do {} while (0)
#endif

#define SCREEN_PRINT(x) printf("\r"); printf x; printf("\n>> "); fflush(stdout);

struct Server_s {
    int port;
    Cmd available_cmds[CMDLIST_LEN];
} Server = {4242,{
    {"help", {""}, 0, "mostra i dettagli dei comandi", false, true},
    {"list", {""}, 0, "mostra un elenco degli utenti connessi", false, true},
    {"esc", {""}, 0, "chiude il server", false, true}
}};

void Server_init(int argv, char *argc[])
{
    if (argv > 1)
        Server.port = atoi(argc[1]);
}

void Server_esc()
{
    printf("Arrivederci\n");
    exit(0);
}

void Server_handleSTDIN(char* buffer)
{
    char tmp[20];
    sscanf(buffer, "%s", tmp);
    if(!strcmp("esc", tmp))
        Server_esc();
    else if(!strcmp("help", tmp)){
        Cmd_showMenu(Server.available_cmds, CMDLIST_LEN, true);
        return;
    } else
        SCREEN_PRINT(("comando non valido: %s", tmp));
    Cmd_showMenu(Server.available_cmds, CMDLIST_LEN, true);
}

void Server_handleUDP(int sd)
{
}

bool Server_checkCredentials(char* username, char* password)
{
    return !strcmp(username, "pippo") && !strcmp(password, "P!pp0");
}

//void Server_handleTCP(char* cmd, int sd)
void Server_handleTCP(int sd)
{
    char *tmp, cmd[6];
    char username[32], password[32];
    // DEBUG_PRINT(("ricevuto messaggio TCP su socket: %d", sd));
    net_receiveTCP(sd, cmd, &tmp);
    //printf("  [debug] tmp=%x\n", tmp);
    if (!strcmp("LOGIN", cmd)) {
        //DEBUG_PRINT(("corpo di signin: %s", tmp));
        if (sscanf(tmp, "%s %s", username, password) == 2){
            DEBUG_PRINT(("ricevuta richiesta di login da parte di ( %s : %s )", username, password));
            if (Server_checkCredentials(username, password)) {
                net_sendTCP(sd, "OK-OK", "");
                DEBUG_PRINT(("richiesta di login accettata"));
            } else {
                net_sendTCP(sd, "UKNWN", "");
                DEBUG_PRINT(("rifiutata richiesta di login da utente sconosciuto"));
            }
        } else {
            net_sendTCP(sd, "ERROR", "");
            DEBUG_PRINT(("rifiutata richiesta di login non valida"));
        }
    }
    free(tmp);
    close(sd);
    FD_CLR(sd, &iom.master);
}

int main(int argv, char *argc[]){
    Server_init(argv, argc);
    Cmd_showMenu(Server.available_cmds, CMDLIST_LEN, true);
    IOMultiplex(Server.port, true, Server_handleSTDIN, Server_handleUDP, Server_handleTCP);
    return 1;
}
