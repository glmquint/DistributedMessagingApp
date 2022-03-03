#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include "util/IOMultiplex.h"
#include "util/cmd.h"

#define STDIN_BUF_LEN 128
#define CMDLIST_LEN 8

#define DEBUG_ON

#ifdef DEBUG_ON
# define DEBUG_PRINT(x) printf("[DEBUG]: "); printf x; printf("\n"); fflush(stdout)
#else
# define DEBUG_PRINT(x) do {} while (0)
#endif

#define SCREEN_PRINT(x) printf("\r"); printf x; printf("\n>> "); fflush(stdout);

struct Device_s {
    bool is_logged_in;
    int port;
    Cmd available_cmds[CMDLIST_LEN];
} Device = {false, -1,{
    {"signup", {"username", "password"}, 2, "crea un account sul server", false, false},
    {"in", {"srv_port", "username", "password"}, 3, "richiede al server la connessione al servizio", false, false},
    {"hanging", {""}, 0, "riceve la lista degli utenti che hanno inviato messaggi mentre si era offline", true, false},
    {"show", {"username"}, 1, "riceve dal server i messaggi pendenti inviati da <username> mentre si era offline", true, false},
    {"chat", {"username"}, 1, "avvia una chat con l'utente <username>", true, false},
    {"share", {"file_name"}, 1, "invia il file <file_name> (nella current directory) al device su cui è connesso l'utente o gli utenti con cui si sta chattando", true, false},
    {"out", {""}, 0, "richiede la disconnessione dal network", true, false},
    {"esc", {""}, 0, "chiude l'applicazione", false, true},
}};

void Device_init(int argv, char *argc[])
{
    if (argv < 2) {
        printf("Utilizzo: %s <porta>", argc[0]);
        exit(0);
    }
    Device.port = atoi(argc[0]);
    Device.is_logged_in = false;
}

void Device_esc()
{
    printf("Arrivederci\n");
    exit(0);
}

void Device_in(int srv_port, char* username, char* password)
{
    DEBUG_PRINT(("richiesta di login sul server localhost:%d con credenziali ( %s : %s )", srv_port, username, password));
}

void Device_handleSTDIN(char* buffer)
{
    char tmp[20], username[32], password[32];
    int srv_port;

    sscanf(buffer, "%s", tmp);
    if(!strcmp("esc", tmp))
        Device_esc();
    else if (!strcmp("in", tmp)) {
        if (sscanf(buffer, "%s %d %s %s", tmp, &srv_port, username, password) == 4) {
            Device_in(srv_port, username, password);
        } else {
            SCREEN_PRINT(("formato non valido per il comando %s %d %s %s", tmp, srv_port, username, password));
        }
    } else {
        SCREEN_PRINT(("comando non valido: %s", tmp));
    }
    Cmd_showMenu(Device.available_cmds, CMDLIST_LEN, Device.is_logged_in);
}

void Device_handleUDP(int sd)
{
    printf("Device_handleUDP(%d)", sd);
}

void Device_handleTCP(char* cmd, int sd)
{
    printf("Device_handleTCP(%s, %d)", cmd, sd);
}

int main(int argv, char *argc[]) 
{
    
    Device_init(argv, argc);
    Cmd_showMenu(Device.available_cmds, CMDLIST_LEN, false);
    IOMultiplex(Device.port, false, Device_handleSTDIN, Device_handleUDP, Device_handleTCP);

    return 1;
}
