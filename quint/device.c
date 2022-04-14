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
#define CMDLIST_LEN 9
#define USERNAME_LEN 32
#define REQ_LEN 6

#define DEBUG_ON


// funzioni di utility per mostrare a schermo informazioni di debug
#ifdef DEBUG_ON
# define DEBUG_PRINT(x) printf("\r[DEBUG]: "); printf x; printf("\n"); fflush(stdout) // è necessario un flush dello stream in quanto SCREEN_PRINT usa '\r'
#else
# define DEBUG_PRINT(x) do {} while (0)
#endif


typedef struct Username_s Username;
struct Username_s {
    char username[USERNAME_LEN];
    Username* next;
};

typedef struct Msg_s Msg;
struct Msg_s {
    char sender[USERNAME_LEN];
    Username* receivers;
    time_t send_ts;
    time_t read_ts;
    Msg* next;
};

// TODO: refactor to util file
typedef struct UserContact_s UserContact;
struct UserContact_s {
    char username[USERNAME_LEN];
    int port; // benché i messaggi vengano recapitati dal server, port permette l'invio P2P di file
    bool is_in_chat; // se l'utente è tra i destinatari con cui il device sta chattando
    Msg* chat_head; // lista dei messaggi
    Msg* chat_tail;
    UserContact* next;
};

UserContact* new_UserContact() {
    UserContact* contact = malloc(sizeof(UserContact));
    memset(contact->username, '\0', USERNAME_LEN);
    strcpy(contact->username, "UNKNWOWN");
    contact->port = -1;
    contact->is_in_chat = false;
    contact->chat_head = NULL;
    contact->chat_tail = NULL;
    contact->next = NULL;
    return contact;
}

struct Device_s {
    bool is_logged_in;
    bool is_chatting;
    char username[USERNAME_LEN];
    int port;
    int srv_port;
    char* joined_chat_receivers;
    Cmd available_cmds[CMDLIST_LEN]; // lista dei comandi invacabili dal menu
    UserContact* contacts_head; // lista dei contatti nella rubrica dell'utente loggato
    UserContact* contacts_tail;
} Device = {false, false, "", -1, 4242, NULL, { // singleton per ogni device
    {"help", {""}, 0, "mostra i dettagli dei comandi",  false, true},
    {"signup", {"username", "password"}, 2, "crea un account sul server", false, false},
    {"in", {"srv_port", "username", "password"}, 3, "richiede al server la connessione al servizio", false, false},
    {"hanging", {""}, 0, "riceve la lista degli utenti che hanno inviato messaggi mentre si era offline", true, false},
    {"show", {"username"}, 1, "riceve dal server i messaggi pendenti inviati da <username> mentre si era offline", true, false},
    {"chat", {"username"}, 1, "avvia una chat con l'utente <username>", true, false},
    {"share", {"file_name"}, 1, "invia il file <file_name> (nella current directory) al device su cui è connesso l'utente o gli utenti con cui si sta chattando", true, false},
    {"out", {""}, 0, "richiede la disconnessione dal network", true, false},
    {"esc", {""}, 0, "chiude l'applicazione", false, true},
},NULL, NULL};

void prompt()
{
    if (Device.is_chatting)
        printf("\nChatting with [%s]: ", Device.joined_chat_receivers);
        //TODO: actually show current chat receivers
    else
        printf("\n>> ");
}
#define SCREEN_PRINT(x) printf("\r"); printf x; prompt(); fflush(stdout);

void Device_init(int argv, char *argc[])
{
    if (argv < 2) {
        printf("Utilizzo: %s <porta>", argc[0]); // è necessario specificare la porta in uso dal device
        exit(0);
    }
    Device.port = atoi(argc[1]);
    Device.is_logged_in = false;
}

void Device_loadContacts()
{
    FILE* fp; // file descriptor della rubrica dell'utente (se presente nel current path) nella forma ".<username>-contacts"
    char contacts_file[USERNAME_LEN + 11]; // filename della rubrica (len(username) + len(".-contacts") + len('\0'))
    char* line = NULL;
    size_t len = 0;
    ssize_t read;
    char this_user[USERNAME_LEN];

    sprintf(contacts_file, ".%s-contacts", Device.username);
    fp = fopen(contacts_file, "r");
    if (fp != NULL) {
        DEBUG_PRINT(("file rubrica trovato: %s", contacts_file));
        while ((read = getline(&line, &len, fp)) != -1) {
            if (sscanf(line, "%s", this_user) != 1) {
                DEBUG_PRINT(("errore nel parsing di un contatto nel file %s", contacts_file));
            } else {
                UserContact* this_contact = new_UserContact();
                sprintf(this_contact->username, "%s", this_user);
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

// se nel current path è presente un file di disconnessione per questo utente (nel formato ".<username>-disconnect")
// significa che l'ultimo evento di disconnessione non è stato comunicato al server.
// Questa informazione deve essere notificata ad ogni successivo tentativo di login, 
// finché non sarà possibile rimuovere il file in sicurezza
//
// [Al successivo login dell'utente, si tenta di cumunicare in ritardo questa informazione al server,
// finché questo non lo riceverà e verrà automaticamente eliminato il file di disconnessione]
// FIXME: better comments ffs
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
            char user_ts[64]; // 64 = len(USERNAME_LEN) + margine per un long int in decimale
            sprintf(user_ts, "%s %ld", username, disconnect_ts);
            int sd = net_initTCP(Device.srv_port);
            if (sd != -1) {
                // TODO: try refactoring with Device_out()
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
            DEBUG_PRINT(("impossibile aprire file di disconnessione pendente %s", disconnect_file));
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
    char *tmp, cmd[REQ_LEN];
    char credentials[70];
    DEBUG_PRINT(("richiesta di login sul server localhost:%d con credenziali ( %s : %s )", srv_port, username, password));
    int sd = net_initTCP(srv_port);
    if (sd != -1) {
        sprintf(credentials, "%s %s %d", username, password, Device.port);
        Device_updateCachedLogout(username); // se è presente un logout non notificato, inviarlo al server durante la nuova connessione
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
                SCREEN_PRINT(("login rifiutata: credenziali errate"));
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
    char *tmp, cmd[REQ_LEN];
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
    //TODO
    DEBUG_PRINT(("showing users with pending messages: ..."));
}

void Device_show(char* username)
{
    //TODO
    DEBUG_PRINT(("showing pending messages from %s", username));
}

void Device_chat(char* username)
{
    UserContact *elem;
    for (elem = Device.contacts_head; elem != NULL; elem = elem->next) {
        if (!strcmp(elem->username, username)) {
            Device.is_chatting = true;
            Device.joined_chat_receivers = realloc(Device.joined_chat_receivers, strlen(username));
            strcpy(Device.joined_chat_receivers, username);
            elem->is_in_chat = true;
            SCREEN_PRINT(("chat iniziata con %s", username));
            return;
        }
    }
    SCREEN_PRINT(("impossibile chattare con %s: utente non trovato nella rubrica", username));
}

void Device_addToChat(char* username)
{
    UserContact *elem;
    bool found = false;
    char* sep = ", ";
    void* tmp;
    size_t len, lensep = strlen(sep), 
        sz = 0; //stored size
    bool first = true;

    for (elem = Device.contacts_head; elem != NULL; elem = elem->next) {
        if (!strcmp(elem->username, username)) {
            elem->is_in_chat = true;
            SCREEN_PRINT(("aggiunto %s alla chat", username));
            found = true;
        }
        if (elem->is_in_chat) {
            len = strlen(elem->username);
            tmp = realloc(Device.joined_chat_receivers, sz + len + (first ? 0 : lensep) + 1);
            if (!tmp) {
                perror ("realloc-tmp: ");
                exit(1);
            }
            Device.joined_chat_receivers = tmp;
            if (!first) {
                strcpy(Device.joined_chat_receivers + sz, sep);
                sz += lensep;
            }
            strcpy (Device.joined_chat_receivers + sz, elem->username);
            first = false;
            sz += len;
        }
    }
    if (!found) {
        SCREEN_PRINT(("impossibile aggiungere %s alla chat: utente non trovato nella rubrica", username));
    }
}

void Device_showOnlineContacts()
{
    //TODO: only print users in contacts that are online asking server
    // DEBUG_PRINT(("user2\nuser3\n"));
    char ans[REQ_LEN];
    UserContact* elem;
    char* buffer;
    int sd;
    int total = 0, count = 0, unregistered = 0, errors = 0;
    printf("Status contatti ([-]: in attesa, [+]: online, [X]: disconnesso, [?] non registrato)\n");
    for (elem = Device.contacts_head; elem != NULL; elem = elem->next) {
        sd = net_initTCP(Device.srv_port);
        printf(" [-] %s", elem->username);
        net_sendTCP(sd, "ISONL", elem->username);
        net_receiveTCP(sd, ans, &buffer);
        if (!strcmp(ans, "ONLIN")){ // user is online
            printf("\r [+] %s\n", elem->username);
            sscanf(buffer, "%d", &elem->port);
            count++;
        } else if (!strcmp(ans, "DSCNT")) { // user is disconnected
            printf("\r [X] %s\n", elem->username);
        } else if (!strcmp(ans, "UNKWN")) { // unknown username
            printf("\r [?] %s\n", elem->username);
            unregistered++;
        } else {
            DEBUG_PRINT(("ricevuta risposta non valida da parte del server"));
            errors++;
        }
        total++;
    }
    SCREEN_PRINT(("%d utenti online su %d in rubrica (%d utenti non registrati, %d errori)", count, total, unregistered, errors));
    close(sd);
    FD_CLR(sd, &iom.master);
}

void Device_quitChat()
{
    UserContact *elem;
    for (elem = Device.contacts_head; elem != NULL; elem = elem->next)
        elem->is_in_chat = false;
    Device.is_chatting = false;
}
void Device_share(char* file_name)
{
    if (!Device.is_chatting) {
        SCREEN_PRINT(("prima di condividere un file è necessario entrare in una chat tramite il comando: chat <username>"));
    } else {
        DEBUG_PRINT(("sharing %s", file_name));
    }
}

void Device_send(char* message)
{
    //TODO:
    int sd, newlen;
    char* payload = NULL; //malloc(0);
    SCREEN_PRINT(("invio messaggio: %s", message));
    sd = net_initTCP(Device.srv_port);
    newlen = strlen(Device.username) + 
                        strlen(Device.joined_chat_receivers) + 
                        strlen(message) + 2;
    DEBUG_PRINT(("newlen = %d (%ld + %ld + %ld + 2)", newlen, strlen(Device.username), strlen(Device.joined_chat_receivers), strlen(message)));
    DEBUG_PRINT(("username = %s", Device.username));
    DEBUG_PRINT(("joined_chat_receivers = %s", Device.joined_chat_receivers));
    DEBUG_PRINT(("message = %s", message));
    payload = realloc(payload, newlen);
    strcpy(payload, Device.username);
    payload = strcat(strcat(strcat(strcat(payload, "\n"), 
                                        Device.joined_chat_receivers), "\n"), 
                                        message);
    net_sendTCP(sd, "|MSG|", payload);
    free(payload);
    // close(sd);
    // FD_CLR(sd, &iom.master);
}

void Device_handleSTDIN(char* buffer)
{
    char* tmp, username[USERNAME_LEN], password[USERNAME_LEN], file_name[64];
    int srv_port;
    tmp = NULL;
    tmp = realloc(tmp, strlen(buffer));
    sscanf(buffer, "%s", tmp);
    if (!Device.is_chatting) { // main menu
        if(!strcmp("esc", tmp))
            Device_esc();
        else if (!strcmp("in", tmp) && !Device.is_logged_in) {
            if (sscanf(buffer, "%s %d %s %s", tmp, &srv_port, username, password) == 4) {
                Device_in(srv_port, username, password);
            } else {
                SCREEN_PRINT(("formato non valido per il comando %s %d %s %s (specificare porta, username e password)", tmp, srv_port, username, password));
            }
        } else if (!strcmp("signup", tmp) && !Device.is_logged_in) {
            if (sscanf(buffer, "%s %s %s", tmp, username, password) == 3) {
                Device_signup(username, password);
            } else {
                SCREEN_PRINT(("formato non valido per il comando %s %s %s (specificare username e password)", tmp, username, password));
            }
        } else if (!strcmp("hanging", tmp) && Device.is_logged_in) {
            Device_hanging();
        } else if (!strcmp("show", tmp) && Device.is_logged_in) {
            if (sscanf(buffer, "%s %s", tmp, username) == 2) {
                Device_show(username);
            } else {
                SCREEN_PRINT(("formato non valido per il comando %s %s (specificare username)", tmp, username));
            }
        } else if (!strcmp("chat", tmp) && Device.is_logged_in) {
            if (sscanf(buffer, "%s %s", tmp, username) == 2) {
                Device_chat(username);
            } else {
                SCREEN_PRINT(("formato non valido per il comando %s %s (specificare username)", tmp, username));
            }
        } else if (!strcmp("share", tmp) && Device.is_logged_in) {
            if (sscanf(buffer, "%s %s", tmp, file_name) == 2) {
                Device_share(file_name);
            } else {
                SCREEN_PRINT(("formato non valido per il comando %s %s (specificare filename)", tmp, file_name));
            }
        } else if (!strcmp("out", tmp) && Device.is_logged_in) {
            Device_out();
        } else if (!strcmp("help", tmp)) {
            Cmd_showMenu(Device.available_cmds, CMDLIST_LEN, Device.is_logged_in);
        } else {
            SCREEN_PRINT(("comando non valido: %s", tmp));
        }
    } else { // user is chatting
        if(!strcmp("\\q", tmp)) {
            Device_quitChat();
        } else if (!strcmp("\\u", tmp)) {
            Device_showOnlineContacts();
        } else if (!strcmp("\\a", tmp)) {
            if (sscanf(buffer, "%s %s", tmp, username) == 2) {
                Device_addToChat(username);
            } else {
                SCREEN_PRINT(("formato non valido per il comando %s %s (specificare username)", tmp, username));
            }
        } else if (!strcmp("share", tmp) && Device.is_logged_in) {
            if (sscanf(buffer, "%s %s", tmp, file_name) == 2) {
                Device_share(file_name);
            } else {
                SCREEN_PRINT(("formato non valido per il comando %s %s (speicificare filename)", tmp, file_name));
            }
        } else {
            Device_send(buffer);
        }
    }
    SCREEN_PRINT(("                                      ")); // clean line necessary with '\r'
    free(tmp);
}

void Device_handleUDP(int sd)
{
    net_answerHeartBeat(sd, Device.port);
}

//void Device_handleTCP(char* cmd, int sd)
void Device_handleTCP(int sd)
{
    DEBUG_PRINT(("Device_handleTCP(%d)", sd));
}

int main(int argv, char *argc[]) 
{
    Device_init(argv, argc);
    Cmd_showMenu(Device.available_cmds, CMDLIST_LEN, false);
    IOMultiplex(Device.port, 
                true, 
                Device_handleSTDIN, 
                Device_handleUDP, 
                Device_handleTCP,
                0,
                NULL);

    return 1;
}
