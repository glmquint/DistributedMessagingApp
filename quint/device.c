#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include "util/IOMultiplex.h"

#define STDIN_BUF_LEN 128
#define CMDLIST_LEN 8

#define DEBUG_OFF

#ifdef DEBUG_ON
# define DEBUG_PRINT(x) printf x; printf("\n"); fflush(stdout)
#else
# define DEBUG_PRINT(x) do {} while (0)
#endif


#define SCREEN_PRINT(x) printf("\r"); printf x; printf("\n>> "); fflush(stdout);

struct Cmd_s {
    char* name;
    char* arguments[20];
    int arg_num;
    char* help;
    bool requires_login;
    bool always_print;
};

typedef struct Cmd_s Cmd;

struct Device {
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

void Device_Init(int argv, char *argc[])
{
    if (argv < 2) {
        printf("Utilizzo: %s <porta>", argc[0]);
        exit(0);
    }
    Device.port = atoi(argc[0]);
    Device.is_logged_in = false;
}

void Device_showMenu()
{
    SCREEN_PRINT(("        \nDigita un comando:\n"));
    for (int i = 0; i < CMDLIST_LEN; i++) {
        if (Device.available_cmds[i].always_print || Device.available_cmds[i].requires_login == Device.is_logged_in) {
            char current_args[50] = "";
            for (int j = 0; j < Device.available_cmds[i].arg_num; j++)
                sprintf(current_args, "%s <%s>", current_args, Device.available_cmds[i].arguments[j]);
            SCREEN_PRINT(("    %s%s --> %s", Device.available_cmds[i].name, current_args, Device.available_cmds[i].help));
        }
    }
}

void Device_esc()
{
    printf("Arrivederci");
    exit(0);
}

void Device_handleSTDIN(char* buffer)
{
    char tmp[20];
    sscanf(buffer, "%s", tmp);
    if(!strcmp("esc", tmp))
        Device_esc();
    else
        SCREEN_PRINT(("comando non valido: %s", tmp));
    Device_showMenu();
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
    
    Device_Init(argv, argc);
    Device_showMenu();
    IOMultiplex(Device.port, false, Device_handleSTDIN, Device_handleUDP, Device_handleTCP);

    return 0;
}
