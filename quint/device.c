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

typedef struct UserContact_s UserContact;
struct UserContact_s {
    char username[32];
    int port;
    UserContact* next;
};

struct Device_s {
    bool is_logged_in;
    char username[32];
    int port;
    int srv_port;
    Cmd available_cmds[CMDLIST_LEN];
    UserContact* contacts_head;
    UserContact* contacts_tail;
} Device = {false, "", -1, 4242, {
    {"signup", {"username", "password"}, 2, "crea un account sul server", false, false},
    {"in", {"srv_port", "username", "password"}, 3, "richiede al server la connessione al servizio", false, false},
    {"hanging", {""}, 0, "riceve la lista degli utenti che hanno inviato messaggi mentre si era offline", true, false},
    {"show", {"username"}, 1, "riceve dal server i messaggi pendenti inviati da <username> mentre si era offline", true, false},
    {"chat", {"username"}, 1, "avvia una chat con l'utente <username>", true, false},
    {"share", {"file_name"}, 1, "invia il file <file_name> (nella current directory) al device su cui è connesso l'utente o gli utenti con cui si sta chattando", true, false},
    {"out", {""}, 0, "richiede la disconnessione dal network", true, false},
    {"esc", {""}, 0, "chiude l'applicazione", false, true},
},NULL, NULL};

void Device_init(int argv, char *argc[])
{
    if (argv < 2) {
        printf("Utilizzo: %s <porta>", argc[0]);
        exit(0);
    }
    Device.port = atoi(argc[1]);
    Device.is_logged_in = false;
}

void Device_loadContacts()
{
    FILE* fp;
    char contacts_file[50];
    char* line = NULL;
    size_t len = 0;
    ssize_t read;
    char this_user[32];

    sprintf(contacts_file, ".%s-contacts", Device.username);
    fp = fopen(contacts_file, "r");
    if (fp != NULL) {
        DEBUG_PRINT(("file rubrica trovato: %s", contacts_file));
        while ((read = getline(&line, &len, fp)) != -1) {
            if (sscanf(line, "%s", this_user) != 1) {
                DEBUG_PRINT(("errore nel parsing di un contatto nel file %s", contacts_file));
            } else {
                UserContact* this_contact = malloc(sizeof(UserContact));
                sprintf(this_contact->username, "%s", this_user);
                this_contact->port = -1;
                this_contact->next = NULL;
                if (Device.contacts_head == NULL) {
                    Device.contacts_head = this_contact;
                    Device.contacts_tail = this_contact;
                } else {
                    Device.contacts_tail->next = this_contact;
                    Device.contacts_tail = this_contact;
                }
            }
        }
    }

    /*
    DEBUG_PRINT(("ribrica:"));
    for (UserContact* elem = Device.contacts_head; elem != NULL; elem = elem->next) {
        DEBUG_PRINT(("%s - %d", elem->username, elem->port));
    }
    */
}

void Device_updateCachedLogout(char* username)
{
    FILE* fp;
    char disconnect_file[50];
    sprintf(disconnect_file, ".%s-disconnect", username);
    fp = fopen(disconnect_file, "r");
    if (fp != NULL) {
        DEBUG_PRINT(("file di disconnessione pendente trovato: %s", disconnect_file));
        time_t disconnect_ts;
        if (fscanf(fp, "%ld", &disconnect_ts) == 1) {
            char user_ts[64];
            sprintf(user_ts, "%s %ld", "pippo", disconnect_ts);
            int sd = net_initTCP(Device.srv_port);
            if (sd != -1) {
                net_sendTCP(sd, "LGOUT", user_ts);
                DEBUG_PRINT(("server aggiornato su l'ultima disconnessione. Eliminazione del file: %s ...", disconnect_file));
                if (remove(disconnect_file)) {
                    DEBUG_PRINT(("impossibile eliminare il file %s", disconnect_file));
                }
                close(sd);
                FD_CLR(sd, &iom.master);
            } else {
                DEBUG_PRINT(("impossibile aggiornare il server su l'ultima disconnessione. Nuovo tentativo al prossimo avvio dell'applicazione"));
            }
        } else {
            DEBUG_PRINT(("impossibile leggere timestamp dal file %s", disconnect_file));
        }
    }
}

void Device_out()
{
    int sd = net_initTCP(Device.srv_port);
    if (sd != -1) {
        char user_ts[64];
        sprintf(user_ts, "%s %d", Device.username, 0);
        net_sendTCP(sd, "LGOUT", user_ts);
        close(sd);
        FD_CLR(sd, &iom.master);
        DEBUG_PRINT(("disconnessione avvennuta con successo"));
    } else { 
        // se non è possibile avvisare il server della disconnessione
        // salvare l'istante di disconnessione e comunicarlo alla prossima riconenssione
        FILE* fp;
        char disconnect_file[50];
        sprintf(disconnect_file, ".%s-disconnect", Device.username);
        fp = fopen(disconnect_file, "w");
        if (fp == NULL) {
            DEBUG_PRINT(("impossibile aprire file %s", disconnect_file));
        } else {
            fprintf(fp, "%ld", getTimestamp());
            DEBUG_PRINT(("timestamp di disconnessione pendente salvato in %s", disconnect_file));
        }
    }
    Device.is_logged_in = false;
}

void Device_esc()
{
    if (Device.is_logged_in) {
        Device_out();
    }
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
        Device_updateCachedLogout(username);
        net_sendTCP(sd, "LOGIN", credentials);
        DEBUG_PRINT(("inviata richiesta di login, ora in attesa"));
        net_receiveTCP(sd, cmd, &tmp);
        DEBUG_PRINT(("ricevuta risposta dal server"));
        if (strlen(cmd) != 0) {
            if (!strcmp("OK-OK", cmd)) {
                SCREEN_PRINT(("login avvenuta con successo!"));
                Device.is_logged_in = true;
                sscanf(username, "%s", Device.username);
                Device_loadContacts();
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
                Device_loadContacts();
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
