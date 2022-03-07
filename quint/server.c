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

typedef struct UserEntry_s UserEntry;
struct UserEntry_s {
    char user_dest[32];
    char password[32];
    int port;
    time_t timestamp_login;
    time_t timestamp_logout;
    UserEntry* next;
};

struct Server_s {
    int port;
    Cmd available_cmds[CMDLIST_LEN];
    UserEntry* user_register_head;
    UserEntry* user_register_tail;
    // CredentialEntry* credentials_head;
    // CredentialEntry* credentials_tail;
} Server = {4242,{
    {"help", {""}, 0, "mostra i dettagli dei comandi", false, true},
    {"list", {""}, 0, "mostra un elenco degli utenti connessi", false, true},
    {"esc", {""}, 0, "chiude il server", false, true}
    }, NULL, NULL};

void Server_init(int argv, char *argc[])
{
    if (argv > 1)
        Server.port = atoi(argc[1]);
    FILE* fp;
    char* line = NULL;
    size_t len = 0;
    ssize_t read;
    char shadow_file[] = "shadow"; // contiene le credenziali di alcuni utenti già registrati

    fp = fopen(shadow_file, "r");
    if (fp == NULL) {
        DEBUG_PRINT(("Impossibile aprire file %s", shadow_file));
    } else {
        while ((read = getline(&line, &len, fp)) != -1) {
            UserEntry* this_user = malloc(sizeof(UserEntry));
            if (sscanf(line, "%s %s", this_user->user_dest, this_user->password) != 2) {
                DEBUG_PRINT(("errore nel parsing di un record nel file %s", shadow_file));
            } else {
                this_user->next = NULL;
                if (Server.user_register_head == NULL) {
                    Server.user_register_head = this_user;
                    Server.user_register_tail = this_user;
                } else {
                    Server.user_register_tail->next = this_user;
                    Server.user_register_tail = this_user;
                }
                DEBUG_PRINT(("credenziali caricate: %s %s", this_user->user_dest, this_user->password));
            }
        }
    }
    fclose(fp);
    if (line)
        free(line);
}

void Server_esc()
{
    printf("Arrivederci\n");
    exit(0);
}

void Server_list()
{
}

void Server_handleSTDIN(char* buffer)
{
    char tmp[20];
    sscanf(buffer, "%s", tmp);
    if(!strcmp("esc", tmp))
        Server_esc();
    else if(!strcmp("help", tmp)) {
        Cmd_showMenu(Server.available_cmds, CMDLIST_LEN, true);
        return;
    } else if(!strcmp("list", tmp))
        Server_list();
    else
        SCREEN_PRINT(("comando non valido: %s", tmp));
    Cmd_showMenu(Server.available_cmds, CMDLIST_LEN, true);
}

void Server_handleUDP(int sd)
{
}

bool Server_checkCredentials(char* username, char* password)
{
    for (UserEntry* elem = Server.user_register_head; elem!=NULL; elem = elem->next) {
        if (!strcmp(username, elem->user_dest) && !strcmp(password, elem->password))
            return true;
    }
    return false;
    // return !strcmp(username, "pippo") && !strcmp(password, "P!pp0");
}

bool Server_signupCredentials(char* username, char* password)
{
    for (UserEntry* elem = Server.user_register_head; elem!=NULL; elem = elem->next) {
        if (!strcmp(username, elem->user_dest))
            return false;
    }
    UserEntry* this_user = malloc(sizeof(UserEntry));
    this_user->next = NULL;
    sscanf(username, "%s", this_user->user_dest);
    sscanf(password, "%s", this_user->password);
    if (Server.user_register_head == NULL) {
        Server.user_register_head = this_user;
        Server.user_register_tail = this_user;
    } else {
        Server.user_register_tail->next = this_user;
        Server.user_register_tail = this_user;
    }
    DEBUG_PRINT(("credenziali caricate: %s %s", this_user->user_dest, this_user->password));
    return true;

    // return strcmp(username, "pippo"); // && strcmp(password, "P!pp0");
}

void Server_handleTCP(int sd)
{
    char *tmp, cmd[6];
    char username[32], password[32];
    // DEBUG_PRINT(("ricevuto messaggio TCP su socket: %d", sd));
    net_receiveTCP(sd, cmd, &tmp);
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
    } else if (!strcmp("SIGUP", cmd)) {
        if (sscanf(tmp, "%s %s", username, password) == 2){
            DEBUG_PRINT(("ricevuta richiesta di signup da parte di ( %s : %s )", username, password));
            if (Server_signupCredentials(username, password)) {
                net_sendTCP(sd, "OK-OK", "");
                DEBUG_PRINT(("richiesta di signup accettata"));
            } else {
                net_sendTCP(sd, "KNOWN", "");
                DEBUG_PRINT(("rifiutata richiesta di signup da utente già registrato"));
            }
        } else {
            net_sendTCP(sd, "ERROR", "");
            DEBUG_PRINT(("rifiutata richiesta di signup non valida"));
        }
    } else if (!strcmp("LGOUT", cmd)) {
        if (tmp != NULL && sscanf(tmp, "%s", username) == 1) {
            for (UserEntry* elem = Server.user_register_head; elem != NULL; elem = elem->next) {
                if (!strcmp(elem->user_dest, username)) {
                    elem->timestamp_logout = getTimestamp();
                    DEBUG_PRINT(("utente %s disconnesso", username));
                    break;
                }
            }
        } else {
            DEBUG_PRINT(("richiesta di disconnessione anonima. Nessun effetto"));
        }
    } else {
        DEBUG_PRINT(("ricevuto comando remoto non valido: %s", cmd));
    }
    if (tmp)
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
