#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include "util/IOMultiplex.h"
#include "util/cmd.h"
#include "util/network.h"
#include "util/time.h"

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
    char username[32];
    int port;
    int srv_port;
    Cmd available_cmds[CMDLIST_LEN];
} Device = {false, "", -1, 4242, {
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
    Device.port = atoi(argc[1]);
    Device.is_logged_in = false;
}

void Device_esc()
{
    int sd = net_initTCP(Device.srv_port);
    net_sendTCP(sd, "LGOUT", Device.username);
    close(sd);
    FD_CLR(sd, &iom.master);
    printf("Arrivederci\n");
    exit(0);
}

void Device_in(int srv_port, char* username, char* password)
{
    char *tmp, cmd[6];
    char credentials[70];
    DEBUG_PRINT(("richiesta di login sul server localhost:%d con credenziali ( %s : %s )", srv_port, username, password));
    int sd = net_initTCP(srv_port);
    if (sd != -1) {
        sprintf(credentials, "%s %s %d", username, password, Device.port);
        net_sendTCP(sd, "LOGIN", credentials);
        DEBUG_PRINT(("inviata richiesta di login, ora in attesa"));
        net_receiveTCP(sd, cmd, &tmp);
        DEBUG_PRINT(("ricevuta risposta dal server"));
        if (strlen(cmd) != 0) {
            if (!strcmp("OK-OK", cmd)) {
                SCREEN_PRINT(("login avvenuta con successo!"));
                Device.is_logged_in = true;
                sscanf(username, "%s", Device.username);
            } else if (!strcmp("UKNWN", cmd)) {
                SCREEN_PRINT(("login rifiutata: utente sconosciuto"));
            } else {
                SCREEN_PRINT(("errore durante la login"));
            }
        } else {
            DEBUG_PRINT(("risposta vuota da parte del server durante la login"));
        }
        if (tmp)
            free(tmp);
        close(sd);
        FD_CLR(sd, &iom.master);
    }
}

void Device_signup(char* username, char* password)
{
    char *tmp, cmd[6];
    char credentials[70];
    int sd = net_initTCP(Device.srv_port); // presupponiamo che il server si trovi a questa porta!!
    if (sd != -1) {
        sprintf(credentials, "%s %s %d", username, password, Device.port);
        net_sendTCP(sd, "SIGUP", credentials);
        DEBUG_PRINT(("inviata richiesta di signup, ora in attesa"));
        net_receiveTCP(sd, cmd, &tmp);
        DEBUG_PRINT(("ricevuta risposta dal server"));
        if (strlen(cmd) != 0) {
            if (!strcmp("OK-OK", cmd)) {
                SCREEN_PRINT(("signup avvenuta con successo!"));
                Device.is_logged_in = true;
                sscanf(username, "%s", Device.username);
            } else if (!strcmp("KNOWN", cmd)) {
                SCREEN_PRINT(("signup rifiutata: utente già registrato. (Usare il comando 'in' per collegarsi)"));
            } else {
                SCREEN_PRINT(("errore durante la signup"));
            }
        } else {
            DEBUG_PRINT(("risposta vuota da parte del server durante la signup"));
        }
        if (tmp)
            free(tmp);
        close(sd);
        FD_CLR(sd, &iom.master);
    }
}

void Device_hanging()
{
}

void Device_show(char* username)
{
}

void Device_chat(char* username)
{
}

void Device_share(char* file_name)
{
}

void Device_out()
{
}

void Device_handleSTDIN(char* buffer)
{
    char tmp[20], username[32], password[32], file_name[64];
    int srv_port;

    sscanf(buffer, "%s", tmp);
    if(!strcmp("esc", tmp))
        Device_esc();
    else if (!strcmp("in", tmp) && !Device.is_logged_in) {
        if (sscanf(buffer, "%s %d %s %s", tmp, &srv_port, username, password) == 4) {
            Device_in(srv_port, username, password);
        } else {
            SCREEN_PRINT(("formato non valido per il comando %s %d %s %s", tmp, srv_port, username, password));
        }
    } else if (!strcmp("signup", tmp) && !Device.is_logged_in) {
        if (sscanf(buffer, "%s %s %s", tmp, username, password) == 3) {
            Device_signup(username, password);
        } else {
            SCREEN_PRINT(("formato non valido per il comando %s %s %s", tmp, username, password));
        }
    } else if (!strcmp("hanging", tmp) && Device.is_logged_in) {
        Device_hanging();
    } else if (!strcmp("show", tmp) && Device.is_logged_in) {
        if (sscanf(buffer, "%s %s", tmp, username) == 2) {
            Device_show(username);
        } else {
            SCREEN_PRINT(("formato non valido per il comando %s %s", tmp, username));
        }
    } else if (!strcmp("chat", tmp) && Device.is_logged_in) {
        if (sscanf(buffer, "%s %s", tmp, username) == 2) {
            Device_chat(username);
        } else {
            SCREEN_PRINT(("formato non valido per il comando %s %s", tmp, username));
        }
    } else if (!strcmp("share", tmp) && Device.is_logged_in) {
        if (sscanf(buffer, "%s %s", tmp, file_name) == 2) {
            Device_share(file_name);
        } else {
            SCREEN_PRINT(("formato non valido per il comando %s %s", tmp, file_name));
        }
    } else if (!strcmp("out", tmp) && Device.is_logged_in) {
        Device_out();
    } else {
        SCREEN_PRINT(("comando non valido: %s", tmp));
    }
    Cmd_showMenu(Device.available_cmds, CMDLIST_LEN, Device.is_logged_in);
}

void Device_handleUDP(int sd)
{
    printf("Device_handleUDP(%d)", sd);
}

//void Device_handleTCP(char* cmd, int sd)
void Device_handleTCP(int sd)
{
    printf("Device_handleTCP(%d)", sd);
}

int main(int argv, char *argc[]) 
{
    
    Device_init(argv, argc);
    Cmd_showMenu(Device.available_cmds, CMDLIST_LEN, false);
    IOMultiplex(Device.port, false, Device_handleSTDIN, Device_handleUDP, Device_handleTCP);

    return 1;
}
