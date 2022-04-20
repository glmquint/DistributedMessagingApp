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

// ATTENZIONE: per attivare la modalità verbose, cambiare DEBUG_OFF -> DEBUG_ON prima di compilare
#define DEBUG_ON

#define STDIN_BUF_LEN   128
#define CMDLIST_LEN     9
#define USERNAME_LEN    32
#define REQ_LEN         6

#define FORMAT_CONTACTS         "./%s/contacts"
#define FORMAT_DISCONNECT       "./%s/disconnect"
#define FORMAT_CHAT             "./%s/%s"
#define FORMAT_CACHED_CHAT      "./%s/cached-%s"
#define FORMAT_RECV_FILE        "(%s)<-(%s) file condiviso: %s\n"
#define FORMAT_SEND_FILE        "(%s)->(%s) file condiviso: %s\n"
#define FORMAT_SCAN_RECV_MSG    "(%[^)])<-(%[^)]): %[^\n]"
#define FORMAT_PRNT_RECV_MSG    "(%s)<-(%s): %s"
#define FORMAT_SEND_MSG         "(%s)->(%s): %s"
#define FORMAT_COPY_FILE        "./%s/copy-%s"

#define JOINED_CHAT_SEPARATOR ", "

// funzione di utility per mostrare a schermo informazioni di debug
// è necessario un flush dello stream in quanto SCREEN_PRINT usa '\r'
#ifdef DEBUG_ON
# define DEBUG_PRINT(x) printf("\r[DEBUG]: "); printf x; printf("\n"); fflush(stdout) 
#else
# define DEBUG_PRINT(x) do {} while (0)
#endif


typedef struct UserContact_s UserContact;
struct UserContact_s {
    char username[USERNAME_LEN];
    int port; // benché i messaggi vengano recapitati dal server, salvare port permette l'invio P2P di file
    bool is_in_chat; // se l'utente è tra i destinatari con cui il device sta chattando
    UserContact* next;
};

// inizializza i parametri di un nuovo elemento UserContact
UserContact* new_UserContact() {
    UserContact* contact = malloc(sizeof(UserContact));
    memset(contact->username, '\0', USERNAME_LEN);
    strcpy(contact->username, "UNKNWOWN");
    contact->port = -1;
    contact->is_in_chat = false;
    contact->next = NULL;
    return contact;
}

struct Device_s {
    bool is_logged_in; // se l'utente se questo device ha effettuato il login
    bool is_chatting; // se l'utente è all'interno di una chat
    char username[USERNAME_LEN];
    int port;
    int srv_port; // la porta su cui è raggiungibile il server in localhost (default: 4242)
    char* joined_chat_receivers; // insieme di utenti (divisi da JOINED_CHAT_SEPARATOR) destinatari della chat
    Cmd available_cmds[CMDLIST_LEN]; // lista dei comandi invacabili dal menu
    UserContact* contacts_head; // lista dei contatti nella rubrica dell'utente loggato
    UserContact* contacts_tail;
} Device = {
    false,  // is_logged_in
    false,  // is_chatting
    "",     // username
    -1,     // port
    4242,   // srv_port
    NULL,   // joined_chat_receivers
    {       // available_cmds
        // name,    arguments,  arg_num,  help_msg,                             requires_login, always_print
        {"help",   {""},         0, "mostra i dettagli dei comandi",                         false, true},
        {"signup", {"username", 
                    "password"}, 2, "crea un account sul server",                            false, false},
        {"in",     {"srv_port", 
                    "username", 
                    "password"}, 3, "richiede al server la connessione al servizio",         false, false},
        {"hanging",{""},         0, "riceve la lista degli utenti che hanno inviato "\
                                        "messaggi mentre si era offline",                    true, false},
        {"show",   {"username"}, 1, "riceve dal server i messaggi pendenti inviati "\
                                        "da <username> mentre si era offline",               true, false},
        {"chat",   {"username"}, 1, "avvia una chat con l'utente <username>",                true, false},
        {"share",  {"file_name"},1, "invia il file <file_name> (nella current directory) "\
                                        "al device su cui è connesso l'utente o gli utenti "\
                                        "con cui si sta chattando",                          true, false},
        {"out",    {""},         0, "richiede la disconnessione dal network",                true, false},
        {"esc",    {""},         0, "chiude l'applicazione",                                 false, true},
    },
    NULL,   // contacts_head
    NULL    // contacts_tail
};  

// Prompt contestuale che mostra i destinatari se si sta chattando, oppure ">> " nel menu
void prompt()
{
    if (Device.is_chatting)
        printf("\nChatting with [%s]: ", Device.joined_chat_receivers);
    else
        printf("\n>> ");
}
#define SCREEN_PRINT(x) printf("\r"); printf x; prompt(); fflush(stdout);

// Inizializzazione dei parametri (la porta su cui stare in ascolto) del device
void Device_init(int argv, char *argc[])
{
    if (argv < 2) {
        printf("Utilizzo: %s <porta>", argc[0]); // è necessario specificare la porta in uso dal device
        exit(0);
    }
    Device.port = atoi(argc[1]);
}

// Carica da file la rubrica dei contatti con il quale l'utente può chattare
// 
// ATTENZIONE:
// Non è presente alcuna funzionalità di aggiunta o modifica della rubrica
// Pertanto, anche effettuando una signup per un nuovo utente, è necessario aggiungere e popolare MANUALMENTE la rubrica
void Device_loadContacts()
{
    FILE* fp; // file descriptor della rubrica dell'utente (se presente a partire dal current path)
    char contacts_file[USERNAME_LEN + strlen(FORMAT_CONTACTS) + 1], // filename della rubrica 
        *line = NULL, // il file di rubrica contiene un utente per linea
        this_user[USERNAME_LEN];
    size_t len = 0;
    ssize_t read;
    UserContact* elem;

    // eventuale svuotamento della lista dei contatti
    // (affinchè sullo stesso dispositivo possano accedere sequenzialmente
    // più utenti senza la necessità di riavviare il dispositivo)
    while(Device.contacts_head){
        elem = Device.contacts_head;
        Device.contacts_head = elem->next;
        free(elem);
    }

    memset(contacts_file, '\0', sizeof(contacts_file));
    sprintf(contacts_file, FORMAT_CONTACTS, Device.username);
    fp = fopen(contacts_file, "r");
    if (fp != NULL) {
        DEBUG_PRINT(("file rubrica trovato: %s", contacts_file));
        while ((read = getline(&line, &len, fp)) != -1) {
            if (sscanf(line, "%s", this_user) != 1) { // il nome utente non deve contenere spazi
                DEBUG_PRINT(("errore nel parsing di un contatto nel file %s", contacts_file));
            } else {
                UserContact* this_contact = new_UserContact();
                sprintf(this_contact->username, "%s", this_user);
                // inserimento in coda alla lista dei contatti
                if (Device.contacts_head == NULL) {
                    Device.contacts_head = this_contact;
                    Device.contacts_tail = this_contact;
                } else {
                    Device.contacts_tail->next = this_contact;
                    Device.contacts_tail = this_contact;
                }
            }
        }
        fclose(fp);
    }
    /*
    // dump completo della rubrica appena caricata
    DEBUG_PRINT(("ribrica:"));
    for (UserContact* elem = Device.contacts_head; elem != NULL; elem = elem->next) {
        DEBUG_PRINT(("%s - %d", elem->username, elem->port));
    }
    */
}

// Se nel current path è presente un file di disconnessione per questo utente
// significa che l'ultimo evento di disconnessione non è stato comunicato al server.
// Questa informazione deve essere notificata ad ogni successivo tentativo di login, 
// finché non sarà possibile rimuovere il file in sicurezza
//
// [Al successivo login dell'utente, si tenta di cumunicare in ritardo questa informazione al server,
// finché questo non lo riceverà e verrà automaticamente eliminato il file di disconnessione]
void Device_updateCachedLogout(char* username)
{
    FILE* fp;
    char disconnect_file[USERNAME_LEN + strlen(FORMAT_DISCONNECT) + 1];
    time_t disconnect_ts;
    char user_ts[64]; // 64 = len(USERNAME_LEN) + margine per un long int in decimale
    int sd;

    memset(disconnect_file, '\0', sizeof(disconnect_file));
    sprintf(disconnect_file, FORMAT_DISCONNECT, username);
    fp = fopen(disconnect_file, "r");
    if (fp != NULL) {
        DEBUG_PRINT(("file di disconnessione pendente trovato: %s", disconnect_file));
        if (fscanf(fp, "%ld", &disconnect_ts) == 1) {
            sprintf(user_ts, "%s %ld", username, disconnect_ts);
            sd = net_initTCP(Device.srv_port);
            if (sd != -1) {
                net_sendTCP(sd, "LGOUT", user_ts, strlen(user_ts));
                DEBUG_PRINT(("server aggiornato su l'ultima disconnessione. Eliminazione del file: %s ...", disconnect_file));
                if (remove(disconnect_file)) {
                    DEBUG_PRINT(("impossibile eliminare il file %s", disconnect_file));
                }
                close(sd);
            } else {
                DEBUG_PRINT(("impossibile aggiornare il server su l'ultima disconnessione. Nuovo tentativo al prossimo avvio dell'applicazione"));
            }
        } else {
            DEBUG_PRINT(("impossibile leggere timestamp dal file %s", disconnect_file));
        }
        fclose(fp);
    }
}

// Comando di disconnessione dell'utente dal device
//
// Notifica il server o salva localmente l'evento di disconnessione
void Device_out()
{
    FILE* fp; // il file di disconnessione, se presente a partire dal current path
    char disconnect_file[50]; // filename di disconnessione
    char user_ts[64];
    int sd; // socket per la comunicazione col server

    sd = net_initTCP(Device.srv_port);
    if (sd != -1) {
        sprintf(user_ts, "%s %d", Device.username, 0);
        net_sendTCP(sd, "LGOUT", user_ts, strlen(user_ts));
        close(sd);
        DEBUG_PRINT(("disconnessione avvennuta con successo"));
    } else { 
        // se non è possibile avvisare il server della disconnessione
        // salvare l'istante di disconnessione e comunicarlo alla prossima riconenssione
        sprintf(disconnect_file, FORMAT_DISCONNECT, Device.username);
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

// Comando di uscita dall'applicazione
//
// Se l'utente è conensso viene prima eseguita la disconnessione
void Device_esc()
{
    if (Device.is_logged_in) {
        Device_out();
    }
    printf("Arrivederci\n");
    exit(0); // chiusura del main thread
}

// Comando di accesso al profilo utente
//
// L'utente effettua l'autenticazione sul server per accedere al servizio
void Device_in(int srv_port, char* username, char* password)
{
    void *tmp;
    char cmd[REQ_LEN];
    char credentials[70];
    int sd;

    DEBUG_PRINT(("richiesta di login sul server localhost:%d con credenziali ( %s : %s )", srv_port, username, password));
    sd = net_initTCP(srv_port);
    if (sd != -1) {
        Device_updateCachedLogout(username); // se è presente un logout non notificato, inviarlo al server durante la nuova connessione

        sprintf(credentials, "%s %s %d", username, password, Device.port);
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
            if (tmp)
                free(tmp);
        } else {
            DEBUG_PRINT(("risposta vuota da parte del server durante la login"));
        }
        close(sd);
    }
}

// Comando di registrazione di un nuovo profilo utente
//
// L'utente si registra al servizio comunicando al server le sue nuove credenziali
//
// ATTENZIONE:
// vengono create la cartella personale (sul device) e la cartella per i messaggi pendenti (sul server)
// ma NON viene creato e popolato il file di rubrica.
// Senza una modifica manuale di tale file, l'utente non può accedere a nessuna funzionalità di messaggistica
void Device_signup(char* username, char* password)
{
    void *tmp;
    char cmd[REQ_LEN];
    char credentials[70];
    int sd;

    sd = net_initTCP(Device.srv_port);
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
                if(!mkdir(username, 0777)){
                    DEBUG_PRINT(("cartella creata %s", username));
                } else {
                    DEBUG_PRINT(("errore nella creazione della cartella %s", username));
                }
                Device_loadContacts();
            } else if (!strcmp("KNOWN", cmd)) {
                SCREEN_PRINT(("signup rifiutata: utente già registrato. (Usare il comando 'in' per collegarsi)"));
            } else {
                SCREEN_PRINT(("errore durante la signup"));
            }
            if (tmp)
                free(tmp);
        } else {
            DEBUG_PRINT(("risposta vuota da parte del server durante la signup"));
        }
        close(sd);
    }
}

// Comando hanging
//
// L'utente richiede al server di inviargli la lista degli utenti
// che gli hanno scritto mentre era offline.
// Con il comando 'show', avrà poi la possibilità di ricevere
// effettivamente i messaggi che sono stati salvati in cache dal server
void Device_hanging()
{
    int sd;
    void *tmp;
    char cmd[REQ_LEN];
    int count, recv_count;
    bool at_least_one; // almeno un utente nella lista fornita dal server

    sd = net_initTCP(Device.srv_port);
    if(sd == -1){
        perror("impossibile contattare il server: ");
        return;
    }
    count = 0;
    at_least_one = false;
    net_sendTCP(sd, "HANG?", Device.username, strlen(Device.username));
    net_receiveTCP(sd, cmd, &tmp);
    while(!strcmp(cmd, "HUSER")) {
        if (!at_least_one) {
            SCREEN_PRINT(("ci sono dei messaggi non letti da questi utenti:"));
            at_least_one = true;
        }
        SCREEN_PRINT(("-> %s", (char*)tmp));
        free(tmp);
        net_receiveTCP(sd, cmd, &tmp);
        count++;
    } 
    if(strcmp(cmd, "OK-OK")){
        DEBUG_PRINT(("errore nella risposta del server"));
        return;
    }
    if (!at_least_one){
        SCREEN_PRINT(("non ci sono messaggi non letti sul server"));
    }
    sscanf(tmp, "%d", &recv_count);
    if (count == recv_count) { // questo controllo dovrebbe essere inutile con TCP. Rimane comunque un semplice double-check/sanity-check
        DEBUG_PRINT(("ricevuti %s utenti in attesa", (char*)tmp));
    } else {
        DEBUG_PRINT(("ricevuto un numero diverso di utenti rispetto a quanto annunciato (%d invece che %d)", count, recv_count));
    }
    free(tmp);
}

// Comando show
//
// Mostra effettivamente i messaggi che sono stati inviati da un utente mentre si era disconnessi
// Questi messaggi, salvati dal sever, verranno aggiunti alla chat con l'utente che li aveva scritti
// Il comando 'hanging' permette di vedere la lista di utenti con almento un messaggio non letto
// e su cui è possibile effettuare il comando 'show'
void Device_show(char* username)
{
    int sd, len;
    char buffer[2*USERNAME_LEN + 2], // strlen("<user_src> <user_dest>\0")
        cmd[REQ_LEN], 
        chat_file[2*USERNAME_LEN + strlen(FORMAT_CHAT) + 1];
    void* tmp;
    FILE* fp;

    DEBUG_PRINT(("richiesta di accesso ai messaggi non letti inviati da %s", username));
    sd = net_initTCP(Device.srv_port);
    if (sd == -1){
        perror("impossibile contattare il server");
        return;
    }
    memset(buffer, '\0', sizeof(buffer));
    sprintf(buffer, "%s %s", Device.username, username);
    net_sendTCP(sd, "SHOW:", buffer, strlen(buffer));
    len = net_receiveTCP(sd, cmd, &tmp);
    if(!strcmp(cmd, "ERROR")){
        perror("errore nella ricezione di messaggi non letti");
    } else {
        sprintf(chat_file, FORMAT_CHAT, Device.username, username);
        fp = fopen(chat_file, "ab");
        if (fp == NULL){
            perror("impossibile appendere sul file di chat");
            // se non è possibile la scrittura in append, forse il file non esiste
            // Aprendolo con "w" il file viene creato se non esiste
            fp = fopen(chat_file, "wb");
            if (fp == NULL){
                perror("impossibile aprire il file di chat");
                close(sd);
                return;
            }
        }
        while(!strcmp(cmd, "HANGF")){
            fwrite(tmp, 1, len, fp);
            SCREEN_PRINT(("%s", (char*)tmp));
            free(tmp);
            len = net_receiveTCP(sd, cmd, &tmp);
        }
        if(strcmp(cmd, "|EOH|")){
            perror("errore nella ricezione fine messaggi non letti");
        }
        fclose(fp);
    }

    close(sd);
    free(tmp);
}

// Comando di accesso alla chat con un utente in rubrica
//
// Viene mostrata la cronologia dei messaggi inviati ad un utente
// Ossia tutti i messaggi, in ordine cronologico, che hanno quell'utente tra i destinatari
//
// Se sono presenti messaggi iviati ma non ricevuti dall'utente, e quinidi salvati in cache dal server,
// questi verranno mostrati in una sezione separata della chat, 
// in fondo ai messaggi recapitati, separati da un divisore. 
// Altrimenti verrà segnalato che il destinatario ha ricevuto tutti i messaggi inviati
void Device_chat(char* username)
{
    FILE* fp;
    char s;
    UserContact *elem;
    char chat_file[2*USERNAME_LEN + strlen(FORMAT_CACHED_CHAT) + 1]; // max(strlen(FORMAT_CHAT), strlen(FORMAT_CACHED_CHAT)) == strlen(FORMAT_CACHED_CHAT)

    // scorrimento della rubrica
    for (elem = Device.contacts_head; elem != NULL; elem = elem->next) {
        if (!strcmp(elem->username, username)) {
            SCREEN_PRINT(("=== chat iniziata con %s ===", username));
            Device.is_chatting = true;
            Device.joined_chat_receivers = realloc(Device.joined_chat_receivers, strlen(username));
            strcpy(Device.joined_chat_receivers, username);
            elem->is_in_chat = true;
            sprintf(chat_file, FORMAT_CHAT, Device.username, elem->username);
            DEBUG_PRINT(("apertura del file: %s", chat_file));
            fp = fopen(chat_file, "r");
            if (fp == NULL) {
                SCREEN_PRINT(("nessun messaggio in questa chat"));
            } else{
                printf("\r");
                while((s=fgetc(fp))!= EOF){
                    printf("%c", s);
                }
                printf("\n");
                fclose(fp);
            }
            // cached msgs
            SCREEN_PRINT(("=== messaggi non letti da %s ===", username));
            sprintf(chat_file, FORMAT_CACHED_CHAT, Device.username, elem->username);
            DEBUG_PRINT(("apertura del file: %s", chat_file));
            fp = fopen(chat_file, "r");
            if (fp == NULL) {
                SCREEN_PRINT(("nessun messaggio non letto"));
            } else{
                printf("\r");
                while((s=fgetc(fp))!= EOF){
                    printf("%c", s);
                }
                printf("\n");
                fclose(fp);
            }

            return;
        }
    }
    SCREEN_PRINT(("impossibile chattare con %s: utente non trovato nella rubrica", username));
}

// Funzione di utility per recuperare e salvare la porta su cui è connesso un utente a partire dal suo username
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
    if (!strcmp(ans, "ONLIN")){         // utente è online
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
    free(buffer);
    close(sd);
    return ret;
}

// Comando di aggiunta di un utente della rubrica ai destinatari della chat
//
// All'interno di una chat, il comando \a <usernme> aggiunge <username> ai destinatari
void Device_addToChat(char* username)
{
    UserContact *elem;
    bool found, first;
    char* sep = JOINED_CHAT_SEPARATOR;
    void* tmp;
    size_t len, lensep,
        sz = 0; //stored size

    found = false;
    first = true;
    lensep = strlen(sep);

    // Scorrimento della rubrica
    // Al massimo un elemento verrà aggiunto alla chat
    // ma la stringa joined_chat_receivers viene ricostruita da capo
    for (elem = Device.contacts_head; elem != NULL; elem = elem->next) {
        if (!strcmp(elem->username, username)) {
            found = true;
            if (elem->port == -1 && !Device_resolvePort(elem)){
                SCREEN_PRINT(("utente disconnesso. Impossibile aggiungerlo alla chat"));
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

// Comando di listing dei contatti con informazioni sullo stato di conenttività
//
// All'interno di una chat, è possibile usare il comando \u per mostrare tutti 
// gli utenti della rubrica e, per ciascuno, se questo è connesso o meno al servizio
void Device_showOnlineContacts()
{
    char ans[REQ_LEN];
    UserContact* elem;
    void* buffer;
    int sd, total = 0, count = 0, unregistered = 0, errors = 0;
    printf("Status contatti ([-]: in attesa, [+]: online, [X]: disconnesso, [?] non registrato)\n");
    for (elem = Device.contacts_head; elem != NULL; elem = elem->next) {
        total++;
        printf(" [-] %s", elem->username); // questa stringa verrà sovrascritta col \r, non appena si avrà una risposta dal server
        sd = net_initTCP(Device.srv_port);
        // se il server non è raggiungibile, si tenta, se possibile, di stabilire la connettività
        // contattando direttamente il device su cui si ipotizza sia online l'utente della rubrica
        if(sd == -1){
            DEBUG_PRINT(("impossibile connettersi al server. Tentativo di connessione manuale con il device"));
            if(elem->port == -1){
                printf("\r [X] %s\n", elem->username);
                continue;
            }
            sd = net_initTCP(elem->port);
            if (sd == -1){
                printf("\r [X] %s\n", elem->username);
            }else{
                printf("\r [+] %s\n", elem->username);
                // non vogliamo inviare informazioni, ma il device si aspetta una richiesta.
                // Questa verrà ingnorata
                net_sendTCP(sd, "IGNOR", "", 0); 
                count++;
                close(sd);
            }
            continue;
        }
        net_sendTCP(sd, "ISONL", elem->username, strlen(elem->username));
        net_receiveTCP(sd, ans, &buffer);
        if (!strcmp(ans, "ONLIN")){ // utente online
            printf("\r [+] %s\n", elem->username);
            sscanf(buffer, "%d", &elem->port);
            count++;
        } else if (!strcmp(ans, "DSCNT")) { // utente disconnesso
            printf("\r [X] %s\n", elem->username);
            elem->port = -1;
        } else if (!strcmp(ans, "UNKWN")) { // utente sconosciuto
            printf("\r [?] %s\n", elem->username);
            elem->port = -1;
            unregistered++;
        } else {
            DEBUG_PRINT(("ricevuta risposta non valida da parte del server"));
            errors++;
        }
        close(sd);
    }
    SCREEN_PRINT(("%d utenti online su %d in rubrica (%d utenti non registrati, %d errori)", count, total, unregistered, errors));
}

// Comando di uscita dalla chat
//
// Usando il comando \q all'interno di una chat, è possibile uscire
// e tornare al menu principale
void Device_quitChat()
{
    UserContact *elem;
    for (elem = Device.contacts_head; elem != NULL; elem = elem->next)
        elem->is_in_chat = false;
    Device.is_chatting = false;
}

// Comando di condivisione di un file con i destinatari della chat
//
// la condivisione di un file nel current path avviene senza intervento del server
void Device_share(char* file_name)
{
    char *dest, *joined_chat_receivers, buffer[1024];
    int sd, len, pid, newlen;
    long int size, sended;
    FILE *fp;
    char chat_file[2*USERNAME_LEN + strlen(FORMAT_CHAT)+1];
    UserContact *elem;
    char *payload;

    if (!Device.is_chatting) { // utente loggato ma non sta chattando con nessuno (è nel menu principale)
        SCREEN_PRINT(("prima di condividere un file è necessario entrare in una chat tramite il comando: chat <username>"));
    } else { // utente è in una chat con un certo numero di partecipanti
        joined_chat_receivers = NULL;
        joined_chat_receivers = realloc(joined_chat_receivers, strlen(Device.joined_chat_receivers));
        strcpy(joined_chat_receivers, Device.joined_chat_receivers);
        dest = strtok(joined_chat_receivers, JOINED_CHAT_SEPARATOR);
        while(dest) { // per ogni destinatario con cui si sta chattando
            DEBUG_PRINT(("├condivisione con: %s", dest));
            for (elem = Device.contacts_head; elem != NULL; elem = elem->next) { // cerca contatto in rubrica
                if (!strcmp(elem->username, dest)){
                    if (elem->port == -1) { // non si conosce la porta del destinatario
                                            // chiediamola al server, se la conosce
                        DEBUG_PRINT(("| non conosco la porta, chiedo al server"));
                        // anche se non si vuole l'intervento del server,
                        // questo è necessario se non si conosce la porta
                        // su cui il destinatario è in ascolto
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
                    if (pid == 0) { // processo figlio
                        // ATTENZIONE:
                        // non stiamo controllando che il destinatario sia proprio dest
                        // ma solo che qualcuno risponda su questa porta.
                        // Se la porta salvata non viene aggiornata e su quella stessa porta
                        // si connette un altro utente, riceverà lui il file
                        sd = net_initTCP(elem->port);
                        if (sd == -1) {
                            // è già stato effettuato il resolve della porta del destinatario
                            // eppure questo non è raggiungibile: inutile continuare oltre
                            exit(0);
                        }
                        fp = fopen(file_name, "rb");
                        if (fp == NULL){
                            perror("impossibile aprire il file: ");
                            exit(0);
                        }
                        // calcolo della dimensione del file da condividere
                        fseek(fp, 0, SEEK_END);
                        size = ftell(fp);
                        // ripristino del puntatore all'inizio del file
                        fseek(fp, 0, SEEK_SET);
                        DEBUG_PRINT(("condivisione file: %s (dimensione: %ld)", file_name, size));
                        net_sendTCP(sd, "SHARE", file_name, strlen(file_name));
                        memset(buffer, '\0', sizeof(buffer));
                        sended = 0;
                        while((len = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
                            net_sendTCP(sd, "FILE:", buffer, len);
                            sended+=len;
                            // questa stringa dovrebbe essere sovrascritta alla terminazione dell'upload
                            printf("\r> %s < %.2lf%% (%ld / %ld)", elem->username, (double)sended/size*100, sended, size);
                            fflush(stdout);
                        }
                        if (sended != size)
                            perror("il file non è stato inviato correttamente: ");
                        payload = NULL;
                        newlen = strlen(Device.username) + 
                                            strlen(Device.joined_chat_receivers) + 
                                            strlen(file_name) + 
                                            strlen(FORMAT_RECV_FILE);
                        payload = realloc(payload, newlen);
                        memset(payload, '\0', newlen);
                        sprintf(payload, FORMAT_RECV_FILE, Device.joined_chat_receivers, Device.username, file_name);
                        net_sendTCP(sd, "|EOF|", payload, newlen);
                        close(sd);
                        fclose(fp);
                        // l'informazione di condivisione file, viene salvata come un messaggio in chat
                        payload = NULL;
                        newlen = strlen(Device.username) + 
                                            strlen(Device.joined_chat_receivers) + 
                                            strlen(file_name) + 
                                            strlen(FORMAT_SEND_FILE);
                        payload = realloc(payload, newlen);
                        memset(payload, '\0', newlen);
                        sprintf(payload, FORMAT_SEND_FILE, Device.username, Device.joined_chat_receivers, file_name);
                        SCREEN_PRINT(("%s", payload));
                        sprintf(chat_file, FORMAT_CHAT, Device.username, dest);
                        DEBUG_PRINT(("salvataggio di %s in %s", payload, chat_file));
                        fp = fopen(chat_file, "a");
                        if (fp == NULL) {
                            DEBUG_PRINT(("Impossibile aprire file %s", chat_file));
                        } else {
                            fprintf(fp, "%s", (char*)payload);
                            fclose(fp);
                        }
                        DEBUG_PRINT(("messaggio salvato in %s", chat_file));
                        dest = strtok(NULL, JOINED_CHAT_SEPARATOR);
                        exit(0); //il processo ha terminato di inviare il file. Quindi termina
                    } // fine processo figlio
                } // if dest == elem->username
            } // for (elem in contacts)
            dest = strtok(NULL, JOINED_CHAT_SEPARATOR);
        } // while(dest)
        DEBUG_PRINT(("┴"));
    }
}

// Comportamento di default della chat per l'invio di messaggi ai destinatari
//
// Digitando del testo, seguito da INVIO, all'interno di una chat con dei partecipanti
// si richiede al server di inoltrare il contenuto del messaggio a tutti i destinatari
//
// FUNZIONALITA' DECENTRALIZZATA:
// nel caso fosse impossibile contattare il server, il device cercherà in autonomia di
// contattare ciascun destinatario, così da recapitargli direttamente il messaggio.
// Questa funzionalità, non richiesta dalle specifiche di progetto, fornisce un QoS
// di tipo best-effort, in quanto l'applicazione potrà fare affidamento solo ai valori
// di porta che ha salvato precedentemente, e non cercherà in alcun modo di effettuare
// altre richieste per colmare le informazioni di connessione mancanti.
void Device_send(char* message)
{
    int sd, newlen, pid;
    char *payload, 
        *joined_chat_receivers,
        *dest,
        cmd[REQ_LEN],
        chat_file[2*USERNAME_LEN + strlen(FORMAT_CACHED_CHAT) + 1]; // max(strlen(FORMAT_CHAT), strlen(FORMAT_CACHED_CHAT)) == strlen(FORMAT_CACHED_CHAT)
    void* tmp;
    FILE* fp;
    UserContact* elem;
    bool cached; // se il messaggio non è stato recapitato, ma salvato in cache dal server

    payload = NULL;
    DEBUG_PRINT(("invio messaggio: %s", message));
    newlen = strlen(Device.username) + 
                        strlen(Device.joined_chat_receivers) + 
                        strlen(message) + 
                        strlen(FORMAT_PRNT_RECV_MSG);
    DEBUG_PRINT(("username = %s", Device.username));
    DEBUG_PRINT(("joined_chat_receivers = %s", Device.joined_chat_receivers));
    DEBUG_PRINT(("message = %s", message));
    payload = realloc(payload, newlen);
    memset(payload, '\0', newlen);
    sprintf(payload, FORMAT_PRNT_RECV_MSG, Device.joined_chat_receivers, Device.username, message);

    sd = net_initTCP(Device.srv_port);
    if (sd == -1) { // impossibile contattare il server. MODALITA' DECENTRALIZZATA
        DEBUG_PRINT(("impossibile contattare il server. Tentativo di invio diretto del messaggio"));
        joined_chat_receivers = NULL;
        joined_chat_receivers = realloc(joined_chat_receivers, strlen(Device.joined_chat_receivers));
        strcpy(joined_chat_receivers, Device.joined_chat_receivers);
        dest = strtok(joined_chat_receivers, JOINED_CHAT_SEPARATOR);
        while(dest) { // per ogni destinatario con cui si sta chattando
            DEBUG_PRINT(("├invio messaggio a: %s", dest));
            for (elem = Device.contacts_head; elem != NULL; elem = elem->next) { // cerca contatto in rubrica
                if (!strcmp(elem->username, dest)){
                    if (elem->port == -1) { 
                        // QoS best effort: non cerchiamo di contattare altri device per recuperare la porta di questo utente
                        DEBUG_PRINT(("| non conosco la porta, impossibile recapitare il messaggio"));
                        break;
                    } else { // si conosce la porta del destinatario
                        DEBUG_PRINT(("|└%s si trova alla porta %d", elem->username, elem->port));
                    }
                    // Non è necessario effettuare l'invio del messaggio su un altro processo,
                    // in quanto (si spera) l'invio di una singola linea di testo dovrebbe essere
                    // abbastanza rapido da non bloccare lo stdin troppo a lungo
                    sd = net_initTCP(elem->port);
                    if (sd == -1) {
                        DEBUG_PRINT(("utente %s è disconnesso. Impossibile recapitare il messaggio", dest));
                        elem->port = -1; //salviamo l'informazione di utente disconnesso
                        break;
                    }
                    DEBUG_PRINT(("├─effettuo l'invio del messaggio adesso sul sd: %d", sd));
                    net_sendTCP(sd, "|MSG|", payload, strlen(payload));
                    net_receiveTCP(sd, cmd, &tmp);
                    if(!strcmp(cmd, "READ:")){
                        DEBUG_PRINT(("messaggio recapitato correttamente"));
                    }else{
                        DEBUG_PRINT(("errore nella risposta al messaggio"));
                    }
                    close(sd);
                } // if dest == elem->username
            } // for (elem in contacts)
            dest = strtok(NULL, JOINED_CHAT_SEPARATOR);
        } // while(dest)
        DEBUG_PRINT(("┴"));
    } else { // richiesta al server di inoltro del messaggio
        net_sendTCP(sd, "|MSG|", payload, strlen(payload));
        net_receiveTCP(sd, cmd, (void*)payload);
        cached = true;
        if(!strcmp(cmd, "READ:")){
            DEBUG_PRINT(("messaggio recapitato correttamente"));
            cached = false;
        } else if (!strcmp(cmd, "CACHE")){
            DEBUG_PRINT(("messaggio salvato nella cache del server"));
        }else{
            DEBUG_PRINT(("errore nella risposta al messaggio"));
        }
        close(sd);

    }
    joined_chat_receivers = NULL;
    joined_chat_receivers = realloc(joined_chat_receivers, strlen(Device.joined_chat_receivers));
    strcpy(joined_chat_receivers, Device.joined_chat_receivers);
    sprintf(payload, FORMAT_SEND_MSG, Device.username, Device.joined_chat_receivers, message);
    SCREEN_PRINT(("%s", payload));
    dest = strtok(joined_chat_receivers, JOINED_CHAT_SEPARATOR);
    while(dest) { // per ogni destinatario con cui si sta chattando
        if (cached) // se il messaggio non è stato recapitato, 
                    // salvarlo in una cache locale
                    // tra i messaggi che il destinatario deve leggere
            sprintf(chat_file, FORMAT_CACHED_CHAT, Device.username, dest);
        else // altrimenti il messaggio può già entrare a far parte dei messaggi letti
            sprintf(chat_file, FORMAT_CHAT, Device.username, dest);
        fp = fopen(chat_file, "a");
        if (fp == NULL) {
            DEBUG_PRINT(("Impossibile appendere sul file %s", chat_file));
            fp = fopen(chat_file, "w");
            if (fp == NULL) {
                DEBUG_PRINT(("Impossibile aprire file %s", chat_file));
                return;
            }
        }
        fprintf(fp, "%s", (char*)payload);
        fclose(fp);
        DEBUG_PRINT(("messaggio salvato in %s", chat_file));
        dest = strtok(NULL, JOINED_CHAT_SEPARATOR);
    }

    free(payload);
    close(sd); 
}

// Gestione dei comandi provenienti dallo standard-input
//
// Si faccia riferimento alle specifiche per una lista
// dettagliata ed esplicativa del funzionamento dei
// comandi disponibili, sia nel menu che nella chat
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
    SCREEN_PRINT(("                                      ")); // "pulitura" dello schermo, necessario con \r nel prompt
    free(tmp);
}

// Funzione di utility per il salvataggio di un messaggio nel file della chat
void Device_saveMsg(char* buffer)
{
    char sender[USERNAME_LEN];
    char receivers[10*USERNAME_LEN]; // stimiamo massimo 10 destinatari contemporanei
    char msg[1024];
    char chat_file[2*USERNAME_LEN + strlen(FORMAT_CHAT) + 1];
    FILE* fp;

    memset(sender, '\0', USERNAME_LEN);
    memset(receivers, '\0', 10*USERNAME_LEN);
    memset(msg, '\0', 1024);
    sscanf(buffer, FORMAT_SCAN_RECV_MSG, receivers, sender, msg);

    sprintf(chat_file, FORMAT_CHAT, Device.username, sender);
    DEBUG_PRINT(("salvataggio di %s in %s", buffer, chat_file));
    fp = fopen(chat_file, "a");
    if (fp == NULL) {
        DEBUG_PRINT(("Impossibile appendere sul file %s", chat_file));
        fp = fopen(chat_file, "w");
        if (fp == NULL) {
            DEBUG_PRINT(("Impossibile aprire file %s", chat_file));
            return;
        }
    }
    fprintf(fp, "%s", buffer);
    if(Device.is_chatting){
         UserContact *elem;
        for (elem = Device.contacts_head; elem != NULL; elem=elem->next){
            if(!strcmp(elem->username, sender)){
                if(elem->is_in_chat){
                    SCREEN_PRINT(("\n%s", buffer));
                }
                break;
            }
        }
    }
    fclose(fp);
    DEBUG_PRINT(("messaggio salvato in %s", chat_file));

}

// Funzione di utility per l'aggiornamento di lettura dei messaggi inviati e non recapitati
//
// Alla richiesta di lettura di messaggi pendenti (tramite il comando 'show')
// il device dell'utente mittente viene notificato dal server.
// In questo modo il device può trasferire tali messaggi per il destinatario
// dalla cache locale dei messaggi non letti, al file contenente tutti i messaggi della chat
void Device_markRead(char* user)
{
    char hanging_file[2*USERNAME_LEN + strlen(FORMAT_CACHED_CHAT)], 
        chat_file[2*USERNAME_LEN + strlen(FORMAT_CHAT)];
    int len;
    char buffer[1024];
    FILE *hfp, *cfp; // hanging-file-pointer, cache-file-pointer

    sprintf(hanging_file, FORMAT_CACHED_CHAT, Device.username, user);
    sprintf(chat_file, FORMAT_CHAT, Device.username, user);
    hfp = fopen(hanging_file, "r");
    if (hfp == NULL){
        perror("file di messaggi non letti non trovato");
        return;
    }
    cfp = fopen(chat_file, "a");
    if (cfp == NULL){
        perror("impossibile appendere sul file di chat");
        cfp = fopen(chat_file, "w");
        if (cfp == NULL){
            perror("impossibile accedere al file di chat");
            fclose(hfp);
            return;
        }
    }
    memset(buffer, '\0', sizeof(buffer));
    // trasferimento da un file all'altro
    while((len = fread(buffer, 1, sizeof(buffer), hfp)) > 0){
        fwrite(buffer, 1, len, cfp);
        DEBUG_PRINT(("messaggi appena letti da %s: %s", user, buffer));
    }
    fclose(cfp);
    fclose(hfp);
    if(!remove(hanging_file)){
        DEBUG_PRINT(("file di messaggi non letti %s rimosso con successo", hanging_file));
    } else {
        DEBUG_PRINT(("impossibile rimuovere il file di messaggi non letti %s", hanging_file));
    }
}

// Comportamento alla ricezione di un pacchetto sul protocollo UDP
//
// L'unico messaggio possibile in ricezione sul device è la richiesta periodica
// di invio dell'heart-beat al server, così che quest'ultimo possa assicurarsi
// della connettività dei device
void Device_handleUDP(int sd)
{
    net_answerHeartBeat(sd, Device.port);
}

// Comportamento alla ricezione di un pacchetto sul protocollo TCP
//
// Un device può ricevere diversi messaggi TCP. 
// Ognuno è identificato da un comando di dimensione fissa (5 caratteri + \0)
// che determina la natura del contenuto, il quale ha dimensione variabile
//
// Una lista dei comandi riconosciuti, con significato del contenuto:
// - SHARE -> inizio della condivisione di un file. Trasmette il nome del file
//    - FILE: -> chunk appartenente al file in trasferimento
//    - |EOF| -> fine della condivisione del file. Trasmissione del messaggio-notifica da inserire in chat
// - |MSG| -> invio di un messaggio da un utente o da parte del server per conto di un utente
// - READ: -> segnalazione di lettura dei messaggi non recapitati da parte di un utente
void Device_handleTCP(int sd)
{
    void *tmp;
    char cmd[REQ_LEN];
    FILE* fp;
    char file_name[128+USERNAME_LEN]; // stima della lunghezza massima del file in ricezione
    int len;

    net_receiveTCP(sd, cmd, &tmp);
    if (!strcmp(cmd, "SHARE")){ // condivisione di un file
        sprintf(file_name, FORMAT_COPY_FILE, Device.username, (char*)tmp);
        DEBUG_PRINT(("salvataggio del file in %s", file_name));
        fp = fopen(file_name, "wb");
        if (!fp) {
            perror("errore nella creazione del file");
            return;
        }
        do { // trasmissione di un chunk di dati appartenente al file
            free(tmp);
            len = net_receiveTCP(sd, cmd, &tmp);
            if (strcmp(cmd, "FILE:"))
                break;
            fwrite(tmp, 1, len, fp);
        } while(strcmp(cmd, "|EOF|")); // fine del file e messaggio da inserire in chat
        Device_saveMsg((char*)tmp);
        fclose(fp);
    } else if (!strcmp(cmd, "|MSG|")){ // trasmissione di un messaggio da salvare in chat
        DEBUG_PRINT(("ricevuto messaggio: %s", (char*)tmp));
        Device_saveMsg((char*)tmp);
        net_sendTCP(sd, "READ:", "", 0);
    } else if (!strcmp(cmd, "READ:")){ // notifica di lettura dei messaggi non letti da parte di un utente
        Device_markRead((char*)tmp);
    }
    free(tmp);
    close(sd);
    FD_ZERO(&iom.master);
}

int main(int argv, char *argc[]) 
{
    Device_init(argv, argc);
    SCREEN_PRINT(("\n\t\t*** DEVICE : %d ***", Device.port));
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
