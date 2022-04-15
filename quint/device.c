#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/stat.h>
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
    char contacts_file[USERNAME_LEN + 12]; // filename della rubrica (strlen(username) + strlen(".//contacts") + strlen('\0'))
    char* line = NULL;
    size_t len = 0;
    ssize_t read;
    char this_user[USERNAME_LEN];

    sprintf(contacts_file, "./%s/contacts", Device.username);
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
    sprintf(disconnect_file, "./%s/disconnect", username);
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
                net_sendTCP(sd, "LGOUT", user_ts, strlen(user_ts));
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
        net_sendTCP(sd, "LGOUT", user_ts, strlen(user_ts));
        close(sd);
        FD_CLR(sd, &iom.master);
        DEBUG_PRINT(("disconnessione avvennuta con successo"));
    } else { 
        // se non è possibile avvisare il server della disconnessione
        // salvare l'istante di disconnessione e comunicarlo alla prossima riconenssione
        FILE* fp;
        char disconnect_file[50];
        sprintf(disconnect_file, "./%s/disconnect", Device.username);
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
    void *tmp;
    char cmd[REQ_LEN];
    char credentials[70];
    DEBUG_PRINT(("richiesta di login sul server localhost:%d con credenziali ( %s : %s )", srv_port, username, password));
    int sd = net_initTCP(srv_port);
    if (sd != -1) {
        sprintf(credentials, "%s %s %d", username, password, Device.port);
        Device_updateCachedLogout(username); // se è presente un logout non notificato, inviarlo al server durante la nuova connessione
        net_sendTCP(sd, "LOGIN", credentials, strlen(credentials));
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
    void *tmp;
    char cmd[REQ_LEN];
    char credentials[70];
    int sd = net_initTCP(Device.srv_port); // presupponiamo che il server si trovi a questa porta!!
    if (sd != -1) {
        sprintf(credentials, "%s %s %d", username, password, Device.port);
        net_sendTCP(sd, "SIGUP", credentials, strlen(credentials));
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

bool Device_resolvePort(UserContact* contact)
{
    char ans[REQ_LEN];
    int sd;
    void *buffer;
    bool ret;
    sd = net_initTCP(Device.srv_port);
    if(sd == -1){
        perror("impossibile contattare il server: ");
        return false;
    }
    net_sendTCP(sd, "ISONL", contact->username, strlen(contact->username));
    net_receiveTCP(sd, ans, &buffer);
    if (!strcmp(ans, "ONLIN")){ // utente è online
        sscanf(buffer, "%d", &contact->port);
        DEBUG_PRINT(("│ └%s è online e si trova alla porta %d", contact->username, contact->port));
        ret = true;
    } else if (!strcmp(ans, "DSCNT")) { // utente è disconnesso
        contact->port = -1;
        DEBUG_PRINT(("│ └%s è disconnesso. Impossibile recapitare il file", contact->username));
        ret = false;
    } else if (!strcmp(ans, "UNKWN")) { // utente sconosciuto
        contact->port = -1;
        DEBUG_PRINT(("│ └%s non registrato con il server", contact->username));
        ret = false;
    } else {
        contact->port = -1;
        DEBUG_PRINT(("ricevuta risposta non valida da parte del server"));
        ret = false;
    }
    close(sd);
    return ret;
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
        // FIXME: only add users that are online
        if (!strcmp(elem->username, username)) {
            found = true;
            if (elem->port == -1 && !Device_resolvePort(elem)){
                DEBUG_PRINT(("utente disconnesso. Impossibile aggiungerlo alla chat"));
                return;
            }
            elem->is_in_chat = true;
            SCREEN_PRINT(("aggiunto %s alla chat", username));
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
    void* buffer;
    int sd;
    int total = 0, count = 0, unregistered = 0, errors = 0;
    printf("Status contatti ([-]: in attesa, [+]: online, [X]: disconnesso, [?] non registrato)\n");
    for (elem = Device.contacts_head; elem != NULL; elem = elem->next) {
        sd = net_initTCP(Device.srv_port);
        printf(" [-] %s", elem->username);
        net_sendTCP(sd, "ISONL", elem->username, strlen(elem->username));
        net_receiveTCP(sd, ans, &buffer);
        if (!strcmp(ans, "ONLIN")){ // user is online
            printf("\r [+] %s\n", elem->username);
            sscanf(buffer, "%d", &elem->port);
            count++;
        } else if (!strcmp(ans, "DSCNT")) { // user is disconnected
            printf("\r [X] %s\n", elem->username);
            elem->port = -1;
        } else if (!strcmp(ans, "UNKWN")) { // unknown username
            printf("\r [?] %s\n", elem->username);
            elem->port = -1;
            unregistered++;
        } else {
            DEBUG_PRINT(("ricevuta risposta non valida da parte del server"));
            errors++;
        }
        total++;
    }
    SCREEN_PRINT(("%d utenti online su %d in rubrica (%d utenti non registrati, %d errori)", count, total, unregistered, errors));
    close(sd);
    // FD_CLR(sd, &iom.master);
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
    char *dest, *joined_chat_receivers, buffer[1024];
    int sd, len, pid;//, num_dest, i; 
    long int size, sended;
    FILE *fp;
    UserContact *elem;
    // bool found;
    if (!Device.is_chatting) { // utente loggato ma non sta chattando con nessuno (è nel menu principale)
        SCREEN_PRINT(("prima di condividere un file è necessario entrare in una chat tramite il comando: chat <username>"));
    } else { // utente è in una chat con un certo numero di partecipanti
        
        joined_chat_receivers = NULL;
        joined_chat_receivers = realloc(joined_chat_receivers, strlen(Device.joined_chat_receivers));
        strcpy(joined_chat_receivers, Device.joined_chat_receivers);
        dest = strtok(joined_chat_receivers, ", ");
        // num_dest = 0;
        while(dest) { // per ogni destinatario con cui si sta chattando
            // found = false;
            DEBUG_PRINT(("├condivisione con: %s", dest));
            for (elem = Device.contacts_head; elem != NULL; elem = elem->next) { // cerca contatto in rubrica
                if (!strcmp(elem->username, dest)){
                    // found = true;
                    if (elem->port == -1) { // non si conosce la porta del destinatario
                                            // chiediamola al server, se la conosce
                        DEBUG_PRINT(("| non conosco la porta, chiedo al server"));
                        if (!Device_resolvePort(elem))
                            break;
                    } else { // si conosce la porta del destinatario
                        DEBUG_PRINT(("|└%s si trova alla porta %d", elem->username, elem->port));
                    }

                    DEBUG_PRINT(("├─effettuo l'invio del file adesso"));
                    // l'invio viene effettuato da un altro processo, in modo che
                    // i files di grandi dimensioni non risultino bloccanti sullo stdin
                    pid = fork();
                    if (pid == -1) {
                        perror("Errore durante la fork: ");
                        break;
                    }
                    if (pid == 0) {
                        // non stiamo controllando che il destinatario sia proprio dest
                        // ma solo che qualcuno risponda su questa porta.
                        // Se la porta salvata non viene aggiornata e su quella stessa porta
                        // si connette un altro utente, riceverà lui il file
                        sd = net_initTCP(elem->port);
                        if (sd == -1) {
                            if (!Device_resolvePort(elem))
                                break;
                            //close(sd); //FIXME: non ha senso chiudere sd se è -1
                            sd = net_initTCP(elem->port);
                        }
                        // DEBUG_PRINT(("├─effettuo l'invio del file adesso"));
                        /**/
                        fp = fopen(file_name, "rb");
                        if (!fp){
                            perror("impossibile aprire il file: ");
                            return;
                        }
                        fseek(fp, 0, SEEK_END);
                        size = ftell(fp);
                        fseek(fp, 0, SEEK_SET);
                        DEBUG_PRINT(("condivisione file: %s (dimensione: %ld)", file_name, size));
                        /**/
                        net_sendTCP(sd, "SHARE", file_name, strlen(file_name));
                        memset(buffer, '\0', sizeof(buffer));
                        sended = 0;
                        while((len = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
                            net_sendTCP(sd, "FILE:", buffer, len);
                            sended+=len;
                            //printf("\r");
                            //for(i = 0; i< num_dest; i++)
                                //printf("\t\t\t\t");
                            printf("\r> %s < %.2lf%% (%ld / %ld)", elem->username, (double)sended/size*100, sended, size);
                            fflush(stdout);
                            //DEBUG_PRINT(("chunk sended"));
                        }
                        if (sended != size)
                            perror("il file non è stato inviato correttamente: ");
                        net_sendTCP(sd, "|EOF|", "", 0);
                        SCREEN_PRINT(("\n"));
                        close(sd);
                        fclose(fp); /**/
                        exit(0);
                        //break;
                    }
                }
            } // for (elem in contacts)
            dest = strtok(NULL, ", ");
            // num_dest++;
        } // while(dest)
        DEBUG_PRINT(("┴"));
        //fclose(fp);
    }
}

void Device_send(char* message)
{
    //TODO:
    int sd, newlen, pid;
    char* payload, *joined_chat_receivers; //malloc(0);
    char* dest;
    char cmd[REQ_LEN];
    UserContact* elem;
    payload = NULL;
    SCREEN_PRINT(("invio messaggio: %s", message));

    newlen = strlen(Device.username) + 
                        strlen(Device.joined_chat_receivers) + 
                        strlen(message) + 2;
    // DEBUG_PRINT(("newlen = %d (%ld + %ld + %ld + 2)", newlen, strlen(Device.username), strlen(Device.joined_chat_receivers), strlen(message)));
    DEBUG_PRINT(("username = %s", Device.username));
    DEBUG_PRINT(("joined_chat_receivers = %s", Device.joined_chat_receivers));
    DEBUG_PRINT(("message = %s", message));
    payload = realloc(payload, newlen);
    strcpy(payload, Device.username);
    payload = strcat(strcat(strcat(strcat(payload, "\n"), 
                                        Device.joined_chat_receivers), "\n"), 
                                        message);

    sd = net_initTCP(Device.srv_port);
    if (sd == -1) { // impossibile contattare il server
        DEBUG_PRINT(("impossibile contattare il server. Tentativo di invio diretto del messaggio"));
        joined_chat_receivers = NULL;
        joined_chat_receivers = realloc(joined_chat_receivers, strlen(Device.joined_chat_receivers));
        strcpy(joined_chat_receivers, Device.joined_chat_receivers);
        dest = strtok(joined_chat_receivers, ", ");
        //FIXME: refactor with send file
        while(dest) { // per ogni destinatario con cui si sta chattando
            // found = false;
            DEBUG_PRINT(("├invio messaggio a: %s", dest));
            for (elem = Device.contacts_head; elem != NULL; elem = elem->next) { // cerca contatto in rubrica
                if (!strcmp(elem->username, dest)){
                    if (elem->port == -1) { // non si conosce la porta del destinatario
                                            // chiediamola al server, se la conosce
                        DEBUG_PRINT(("| non conosco la porta, impossibile recapitare il messaggio"));
                        break;
                    } else { // si conosce la porta del destinatario
                        DEBUG_PRINT(("|└%s si trova alla porta %d", elem->username, elem->port));
                    }

                    /*
                    // l'invio viene effettuato da un altro processo, in modo che
                    // i messaggi di grandi dimensioni non risultino bloccanti sullo stdin
                    pid = fork();
                    if (pid == -1) {
                        perror("Errore durante la fork: ");
                        break;
                    }
                    if (pid == 0) {
                        // non stiamo controllando che il destinatario sia proprio dest
                        // ma solo che qualcuno risponda su questa porta.
                        // Se la porta salvata non viene aggiornata e su quella stessa porta
                        // si connette un altro utente, riceverà lui il file
                        */
                    sd = net_initTCP(elem->port);
                    if (sd == -1) {
                        DEBUG_PRINT(("utente %s è disconnesso. Impossibile recapitare il messaggio", dest));
                        elem->port = -1; //salviamo l'informazione di utente disconnesso
                        //exit(0);
                        break;
                    }
                    DEBUG_PRINT(("├─effettuo l'invio del messaggio adesso sul sd: %d", sd));
                    /**/
                    net_sendTCP(sd, "|MSG|", payload, strlen(payload));
                    net_receiveTCP(sd, cmd, (void*)payload);
                    if(!strcmp(cmd, "OK-OK")){
                        DEBUG_PRINT(("messaggio recapitato correttamente"));
                    }else{
                        DEBUG_PRINT(("errore nella risposta al messaggio"));
                    }
                    close(sd);
                        // exit(0);
                    /*}*/
                }
            } // for (elem in contacts)
            dest = strtok(NULL, ", ");
            // num_dest++;
        } // while(dest)
        DEBUG_PRINT(("┴"));
    } else {
        net_sendTCP(sd, "|MSG|", payload, strlen(payload));
    }
    free(payload);
    close(sd); 
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
            if (sscanf(buffer, "%s %[^\t\n]", tmp, file_name) == 2) {
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
            if (sscanf(buffer, "%s %[^\t\n]", tmp, file_name) == 2) {
                DEBUG_PRINT(("request to share: %s", file_name));
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

void Device_handleTCP(int sd)
{
    void *tmp;
    char cmd[REQ_LEN];
    FILE* fp;
    char file_name[128];
    int len;
    // DEBUG_PRINT(("Device_handleTCP(%d)", sd));
    net_receiveTCP(sd, cmd, &tmp);
    DEBUG_PRINT(("ricevuto cmd = %s\npayload = %s", cmd, (char*)tmp));
    if (!strcmp(cmd, "SHARE")){
        sprintf(file_name, "./%s/copy-%s", Device.username, (char*)tmp);
        DEBUG_PRINT(("saving to %s", file_name));
        fp = fopen(file_name, "wb");
        if (!fp) {
            perror("errore nella creazione del file");
            return;
        }
        do {
            free(tmp);
            len = net_receiveTCP(sd, cmd, &tmp);
            if (strcmp(cmd, "FILE:"))
                break;
            fwrite(tmp, 1, len, fp);
            //DEBUG_PRINT(("content of size(%ld): %s", strlen(tmp), tmp));
        } while(strcmp(cmd, "|EOF|"));
        fclose(fp);
    } else if (!strcmp(cmd, "|MSG|")){
        DEBUG_PRINT(("ricevuto messaggio: %s", (char*)tmp));
        net_sendTCP(sd, "OK-OK", "", 0);
    }
    free(tmp);
    close(sd);
    FD_ZERO(&iom.master);
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
