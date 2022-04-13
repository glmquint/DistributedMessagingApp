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
#include "util/time.h"

#define CMDLIST_LEN 3
#define MAX_CHANCES 3
#define REQ_LEN 6

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
    int chances;
    UserEntry* next;
};

struct Server_s {
    int port;
    Cmd available_cmds[CMDLIST_LEN];
    UserEntry* user_register_head;
    UserEntry* user_register_tail;
    char* shadow_file;
    // CredentialEntry* credentials_head;
    // CredentialEntry* credentials_tail;
} Server = {4242,{
    {"help", {""}, 0, "mostra i dettagli dei comandi", false, true},
    {"list", {""}, 0, "mostra un elenco degli utenti connessi", false, true},
    {"esc", {""}, 0, "chiude il server", false, true}
    }, NULL, NULL, "shadow"};

void Server_init(int argv, char *argc[])
{
    if (argv > 1)
        Server.port = atoi(argc[1]);
}

void Server_loadUserEntry()
{
    FILE* fp;
    char* line = NULL;
    size_t len = 0;
    ssize_t read;

    fp = fopen(Server.shadow_file, "r");
    if (fp == NULL) {
        DEBUG_PRINT(("Impossibile aprire file %s", Server.shadow_file));
    } else {
        while ((read = getline(&line, &len, fp)) != -1) {
            UserEntry* this_user = malloc(sizeof(UserEntry));
            if (sscanf(line, "%s %s %d %ld %ld %d", this_user->user_dest, 
                                                this_user->password, 
                                                &this_user->port,
                                                &this_user->timestamp_login, 
                                                &this_user->timestamp_logout,
                                                &this_user->chances) != 6) {
                DEBUG_PRINT(("errore nel parsing di un record nel file %s", Server.shadow_file));
            } else {
                this_user->next = NULL;
                if (Server.user_register_head == NULL) {
                    Server.user_register_head = this_user;
                    Server.user_register_tail = this_user;
                } else {
                    Server.user_register_tail->next = this_user;
                    Server.user_register_tail = this_user;
                }
                // DEBUG_PRINT(("credenziali caricate: %s %s", this_user->user_dest, this_user->password));
            }
        }
    fclose(fp);
    if (line)
        free(line);
    }
}

void Server_saveUserEntry()
{
    FILE* fp;
    fp = fopen(Server.shadow_file, "w");
    if (fp == NULL) {
        DEBUG_PRINT(("impossibile aprire file: %s", Server.shadow_file));
        return;
    }
    int count = 0;
    UserEntry* elem;
    // for (elem = Server.user_register_head; elem != NULL; elem = elem->next) {
    while(Server.user_register_head) {
        elem = Server.user_register_head;
        fprintf(fp, "%s %s %d %ld %ld %d\n", elem->user_dest, 
                                        elem->password, 
                                        elem->port,
                                        elem->timestamp_login, 
                                        elem->timestamp_logout,
                                        elem->chances);
        count++;
        Server.user_register_head = elem->next;
        free(elem);
    }
    // DEBUG_PRINT(("%d utenti salvati in %s", count, Server.shadow_file));
    fclose(fp);
}

void Server_esc()
{
    // Server_loadUserEntry();
    // Server_saveUserEntry();
    printf("Arrivederci\n");
    exit(0);
}

void Server_list()
{
    int total_users = 0;
    int logged_in_users = 0;
    UserEntry* elem;
    Server_loadUserEntry();
    for (elem = Server.user_register_head; elem != NULL; elem = elem->next) {
        total_users++;
        if (elem->timestamp_login > elem->timestamp_logout) {
            logged_in_users++;
            SCREEN_PRINT(("%s*%s*%d", elem->user_dest, getDateTime(elem->timestamp_login), elem->port));
            //SCREEN_PRINT(("%s*%ld*%d", elem->user_dest, elem->timestamp_login, elem->port));
        }
    }
    SCREEN_PRINT(("%d utenti connessi su %d totali", logged_in_users, total_users));
    Server_saveUserEntry();
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
    } else if(!strcmp("list", tmp)) {
        Server_list();
    } else {
        SCREEN_PRINT(("comando non valido: %s", tmp));
    }
    Cmd_showMenu(Server.available_cmds, CMDLIST_LEN, true);
}

void Server_handleUDP(int sd)
{
    UserEntry* elem;
    int remote_port = net_receiveHeartBeat(sd);
    if (remote_port == -1)
        return;
    Server_loadUserEntry();
    for (elem = Server.user_register_head; elem != NULL; elem = elem->next) {
        if (elem->port == remote_port) {
            elem->chances = MAX_CHANCES;
            break;
        }
    }
    Server_saveUserEntry();
}

void Server_onTimeout()
{
    UserEntry* elem;
    Server_loadUserEntry();
    for (elem = Server.user_register_head; elem != NULL; elem = elem->next) {
        if (elem->timestamp_login > elem->timestamp_logout) {
            if (elem->chances > 0) {
                net_askHearthBeat(elem->port, Server.port);
                elem->chances--;
                // DEBUG_PRINT(("%s now only has %d chances left", elem->user_dest, elem->chances));
            } else { // no chances left
                // TODO: make it disconnected
                elem->timestamp_logout = getTimestamp();
                DEBUG_PRINT(("%s disconnected", elem->user_dest));
            }
        }
    }

    Server_saveUserEntry();
}

bool Server_checkCredentials(char* username, char* password, int dev_port)
{
    UserEntry* elem;
    Server_loadUserEntry();
    for (elem = Server.user_register_head; elem!=NULL; elem = elem->next) {
        if (!strcmp(username, elem->user_dest) && !strcmp(password, elem->password)) {
            elem->timestamp_login = getTimestamp();
            elem->port = dev_port;
            elem->chances = MAX_CHANCES;
            Server_saveUserEntry();
            return true;
        }
    }
    return false;
    // return !strcmp(username, "pippo") && !strcmp(password, "P!pp0");
}

bool Server_signupCredentials(char* username, char* password, int dev_port)
{
    UserEntry* elem;
    Server_loadUserEntry();
    for ( elem = Server.user_register_head; elem!=NULL; elem = elem->next) {
        if (!strcmp(username, elem->user_dest))
            return false;
    }
    UserEntry* this_user = malloc(sizeof(UserEntry));
    this_user->next = NULL;
    this_user->timestamp_login = getTimestamp();
    this_user->port = dev_port;
    this_user->chances = MAX_CHANCES;
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
    Server_saveUserEntry();
    return true;

    // return strcmp(username, "pippo"); // && strcmp(password, "P!pp0");
}

void Server_handleTCP(int sd)
{
    char *tmp, cmd[REQ_LEN];
    char username[32], password[32];
    char port_str[6];
    int dev_port;
    time_t logout_ts;
    UserEntry* elem;
    bool found;
    // DEBUG_PRINT(("ricevuto messaggio TCP su socket: %d", sd));
    net_receiveTCP(sd, cmd, &tmp);
    // DEBUG_PRINT(("comando ricevuto: %s", cmd));
    if (!strcmp("LOGIN", cmd)) {
        //DEBUG_PRINT(("corpo di signin: %s", tmp));
        if (sscanf(tmp, "%s %s %d", username, password, &dev_port) == 3){
            DEBUG_PRINT(("ricevuta richiesta di login da parte di ( %s : %s : %d )", username, password, dev_port));
            if (Server_checkCredentials(username, password, dev_port)) {
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
        if (sscanf(tmp, "%s %s %d", username, password, &dev_port) == 3){
            DEBUG_PRINT(("ricevuta richiesta di signup da parte di ( %s : %s )", username, password));
            if (Server_signupCredentials(username, password, dev_port)) {
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
        if (tmp != NULL && sscanf(tmp, "%s %ld", username, &logout_ts) == 2) {
            Server_loadUserEntry();
            for (elem = Server.user_register_head; elem != NULL; elem = elem->next) {
                if (!strcmp(elem->user_dest, username)) {
                    if (logout_ts == 0)
                        elem->timestamp_logout = getTimestamp();
                    else
                        elem->timestamp_logout = logout_ts;
                    DEBUG_PRINT(("utente %s disconnesso", username));
                    Server_saveUserEntry();
                    break;
                }
            }
        } else {
            DEBUG_PRINT(("richiesta di disconnessione anonima. Nessun effetto"));
        }
    } else if (!strcmp("ISONL", cmd)) {
        if (sscanf(tmp, "%s", username) == 1) {
            Server_loadUserEntry();
            found = false;
            for (elem = Server.user_register_head; elem != NULL; elem = elem->next) {
                if (!strcmp(elem->user_dest, username)) {
                    if (elem->timestamp_login > elem->timestamp_logout) {
                        sprintf(port_str, "%d", elem->port);
                        net_sendTCP(sd, "ONLIN", port_str);
                    } else {
                        net_sendTCP(sd, "DSCNT", "");
                    }
                    found = true;
                    break;
                }
            }
            if (!found)
                net_sendTCP(sd, "UNKWN", "");
        } else {
            DEBUG_PRINT(("ricevuta richiesta di conenttività ma nessun username trasmesso"));
        }
    } else {
        DEBUG_PRINT(("ricevuto comando remoto non valido: %s", cmd));
    }
    if (tmp)
        free(tmp);
    close(sd);
    FD_CLR(sd, &iom.master);
}

int main(int argv, char *argc[])
{
    Server_init(argv, argc);
    Cmd_showMenu(Server.available_cmds, CMDLIST_LEN, true);
    IOMultiplex(Server.port, 
                true, 
                Server_handleSTDIN, 
                Server_handleUDP, 
                Server_handleTCP, 
                1, 
                Server_onTimeout);
    return 1;
}
