#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <dirent.h>
#include <sys/stat.h>
#include "util/IOMultiplex.h"
#include "util/cmd.h"
#include "util/network.h"
#include "util/time.h"

// ATTENZIONE: per attivare la modalità verbose, cambiare DEBUG_OFF -> DEBUG_ON prima di compilare
#define DEBUG_ON

#define USERNAME_LEN    32
#define CMDLIST_LEN     3
#define MAX_CHANCES     3
#define REQ_LEN         6
#define TIMEOUT         1

#define FORMAT_HANGING          "./hanging/%s"
#define FORMAT_HANGING_FILE     "./hanging/%s/%s"
#define FORMAT_SCAN_RECV_MSG    "(%[^)])<-(%[^)]): %[^\n]"

#define JOINED_CHAT_SEPARATOR ", "

#ifdef DEBUG_ON
# define DEBUG_PRINT(x) printf("[DEBUG]: "); printf x; printf("\n"); fflush(stdout)
#else
# define DEBUG_PRINT(x) do {} while (0)
#endif

#define SCREEN_PRINT(x) printf("\r"); printf x; printf("\n>> "); fflush(stdout);

typedef struct UserEntry_s UserEntry;
struct UserEntry_s {
    char user_dest[USERNAME_LEN]; // username dell'utente degistrato
    char password[USERNAME_LEN]; // password dell'utente registrato
    int port; // la porta su cui è raggiungibile l'utente
            // ha senso soltanto se timestamp_login > timestamp_logout
    time_t timestamp_login; // timestamp di login
    time_t timestamp_logout; // timestamp di logout
    int chances; // chances entro le quali il device è ritenuto online
                // rispondere alla richiesta di heartbeat ripristina le chances
    UserEntry* next;
};

struct Server_s {
    int port; // porta su cui il server è in ascolto su localhost
    Cmd available_cmds[CMDLIST_LEN];
    UserEntry* user_register_head; // lista degli utenti registrati
    UserEntry* user_register_tail;
    char* shadow_file; // nome del file contenente le credenziali per l'autenticazione utenti
} Server = {4242, // port
    {   //name, arguments, arg_num, help_msg                    ,requires_login, always_print
        {"help",    {""}, 0, "mostra i dettagli dei comandi",           false, true},
        {"list",    {""}, 0, "mostra un elenco degli utenti connessi",  false, true},
        {"esc",     {""}, 0, "chiude il server",                        false, true}
    }, 
    NULL, // user_register_head
    NULL, // user_register_tail
    "shadow" // shadow_file
};

// Inizializzazione parametri del server (la porta su cui stare in ascolto)
void Server_init(int argv, char *argc[])
{
    if (argv > 1)
        Server.port = atoi(argc[1]);
}

// Funzione di utility per leggere dal file-registro degli utenti e riempire la lista
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
            }
        }
    fclose(fp);
    if (line)
        free(line);
    }
}

// Funzione di utility per salvare il contenuto del registro degli utenti
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
    fclose(fp);
}

// Comando di disconnessione e chiusura del server
//
// I devices non vengono avvisati della disconnessione del server.
// Questi continueranno a funzionare in maniera decentralizzata con le sole
// informazioni che avevano quando il server si è disconnesso
void Server_esc()
{
    printf("Arrivederci\n");
    exit(0);
}

// Comando di listing degli utenti attualmente online nel servizio
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
        }
    }
    SCREEN_PRINT(("%d utenti connessi su %d totali", logged_in_users, total_users));
    Server_saveUserEntry();
}

// Gestione dei comandi provenienti da standard-input
//
// Si faccia riferimento alle specifiche per una lista dettagliata
// ed esplicativa del funzionamento dei comandi disponibili
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

// Comportamento alla ricezione di un pacchetto sul protocollo UDP
//
// L'unico messaggio possibile in ricezione sul server è la notifica
// di connessione (alive) in risposta alla richiesta di hartbeat effettuata
// precedentemente dal server. Questo comporta il ripristino delle
// chances disponibili per il device
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

// Comportamento allo scadere del timeout
//
// In maniera periodica, il server effettua una routine di controllo dello stato
// di connessione degli utenti che attualmente hanno effettuato il login e non hanno
// dichiarato alcun logout. A ciascuno viene inviata una richiesta di hartbeat sul protocollo
// UDP e, data la natura unreliable del protocollo, si tiene traccia dei tentativi rimasti
// decrementando il numero di chances disponibili per ogni utente a cui si fa richiesta.
// Se il device non risponde per un numero sufficiente di volte, questo viene automaticamente 
// considerato disconnesso, non appena il suo numero di chances raggiunge lo zero.
void Server_onTimeout()
{
    UserEntry* elem;
    Server_loadUserEntry();
    for (elem = Server.user_register_head; elem != NULL; elem = elem->next) {
        if (elem->timestamp_login > elem->timestamp_logout) {
            if (elem->chances > 0) {
                net_askHearthBeat(elem->port, Server.port);
                elem->chances--;
            } else { // no chances left
                elem->timestamp_logout = getTimestamp();
                elem->port = -1;
                DEBUG_PRINT(("%s disconnected", elem->user_dest));
            }
        }
    }

    Server_saveUserEntry();
}

// Funzionalità di autenticazione degli utenti che vogliono accedere al servizio
//
// Le credenziali fornite vengono controntate con quelle presenti nel file shadow del server.
// Non viene effettuata alcuna forma di cifraggio delle credenziali, né durante la
// comunicazione, né durante il salvataggio su file
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
}

// Funzionalità di registrazione di un nuovo utente al servizio
//
// Le credenziali vengono salvate sul file shadow del server,
// così da renderle disponibili in fase di autenticazione durante i successivi login
// Viene inoltre creata la sottocartella per il nuovo utente, in ./hanging/
// dove verranno salvati i messaggi non recapitati, quando l'utente è offline
bool Server_signupCredentials(char* username, char* password, int dev_port)
{
    UserEntry* elem;
    Server_loadUserEntry();
    char hanging_folder[50];
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
    memset(hanging_folder, '\0', sizeof(hanging_folder));
    sprintf(hanging_folder, FORMAT_HANGING, username);
    if(!mkdir(hanging_folder, 0777)){
        DEBUG_PRINT(("cartella creata %s", hanging_folder));
    } else {
        DEBUG_PRINT(("errore nella creazione della cartella %s", hanging_folder));
    }
    return true;
}

// Funzionalità di salvataggio in cache dei messaggi non recapitati
//
// Se alla richiesta di inoltro di un messaggio il destinatario non è online,
// il server salva in un file di cache locale tale messaggio, in attesa che l'utente
// si riconnetta e scarichi i messaggi pendenti.
void Server_hangMsg(char* dest, char* sender, void* buffer)
{
    char hanging_file[64];
    FILE* fp;
    sprintf(hanging_file, FORMAT_HANGING_FILE, dest, sender);
    fp = fopen(hanging_file, "a");
    if (fp == NULL) {
        DEBUG_PRINT(("Impossibile aprire file %s", hanging_file));
    } else {
        fprintf(fp, "%s", (char*)buffer);
    }
    DEBUG_PRINT(("messaggio salvato in %s", hanging_file));
    fclose(fp);
}

// Richiesta di inoltro messaggio
//
// La componente centralizzata del servizio di messaggistica richiede che i devices
// comunichino al server il messaggio e a quali utenti recapitarlo.
// E' poi compito del server inoltrare o salvare localmente il messaggio nel caso
// in cui il destinatario non fosse raggiungibile perché offline
void Server_forwardMsg(int sd, void* buffer)
{
    char sender[USERNAME_LEN],
        receivers[10*USERNAME_LEN], // stimiamo massimo 10 destinatari contemporanei
        msg[1024],
        *dest;
    int newsd;
    bool at_least_one_cached;
    UserEntry* elem;

    memset(sender, '\0', USERNAME_LEN);
    memset(receivers, '\0', 10*USERNAME_LEN);
    memset(msg, '\0', 1024);
    DEBUG_PRINT(("forwarding %s", (char*)buffer));
    sscanf(buffer,FORMAT_SCAN_RECV_MSG, receivers, sender, msg);
    DEBUG_PRINT(("sender: %s\n receivers: %s\n msg: %s", sender, receivers, msg));
    Server_loadUserEntry(); // non si modifica il contenuto, quindi non c'è bisogno di chiamare saveUserEntry() alla fine
    at_least_one_cached = false;
    dest = strtok(receivers, JOINED_CHAT_SEPARATOR);
    while(dest){ // per ogni utente nella lista dei destinatari
        for (elem = Server.user_register_head; elem != NULL; elem = elem->next) {
            if (!strcmp(elem->user_dest, dest)) {
                if (elem->port == -1){
                    Server_hangMsg(dest, sender, buffer);
                    at_least_one_cached = true;
                }else{
                    newsd = net_initTCP(elem->port);
                    if (newsd == -1){
                        sleep(3*TIMEOUT); // grazie ad askHeartBeat, questo è equivalente a tentare tre volte la riconnessione
                        newsd = net_initTCP(elem->port);
                        if (newsd == -1){ // se dopo 3 tentativi il device non ha risposto alla 
                                        // richiesta di hartbeat, può essere considerato irraggiungibile
                            Server_hangMsg(dest, sender, buffer);
                            at_least_one_cached = true;
                        }
                    } else {
                        net_sendTCP(newsd, "|MSG|", buffer, strlen(buffer));
                        DEBUG_PRINT(("messaggio trasmesso a %s", dest));
                    }
                    close(newsd);
                }
            }
        }   
        dest = strtok(NULL, ", ");
    }
    if (at_least_one_cached){
        net_sendTCP(sd, "CACHE", "", 0);
    } else{
        net_sendTCP(sd, "READ:", "", 0);
    }
}

// Comando di listing degli utenti con dei messaggi non recapitati
void Server_hanging(int sd, char* user)
{
    struct dirent *de;
    char userdir[USERNAME_LEN + strlen(FORMAT_HANGING)+1],
        count_str[4];
    DIR *dr;
    int count;

    sprintf(userdir, FORMAT_HANGING, user);
    DEBUG_PRINT(("apertura della directory: %s", userdir));
    dr = opendir(userdir);
    if (dr == NULL) {
        perror("impossibile aprire la directory");
        net_sendTCP(sd, "ERROR", "", 0);
        return;
    }
    count = 0;
    while((de = readdir(dr)) != NULL){
        if(!strcmp(de->d_name, ".") || !strcmp(de->d_name, ".."))
            continue;
        DEBUG_PRINT(("trovato file: %s\n", de->d_name));
        net_sendTCP(sd, "HUSER", de->d_name, strlen(de->d_name));
        count++;
    }
    sprintf(count_str, "%d", count);

    net_sendTCP(sd, "OK-OK", count_str, strlen(count_str));
    closedir(dr);
}

// Richiesta di trasferimento dei messaggi non recapitati inviati da uno specifico utente
//
// Il mittente viene segnalato della lettura dei suoi messaggi che non erano stati recapitati.
// In questo modo può a sua volta sincronizzare il suo storico dei messaggi,
// trasferendo i messaggi che aveva inviato e che non erano stati letti nei messaggi della chat
void Server_show(int sd, char* src_dest)
{
    char user[USERNAME_LEN], 
        hanging_user[USERNAME_LEN], 
        hanging_file[2*USERNAME_LEN + strlen(FORMAT_HANGING_FILE) + 1];
    FILE* fp;
    int len, new_sd;
    char buffer[1024];

    DEBUG_PRINT(("sono mostrati i messaggi pendenti (destinatario, mittente) %s", src_dest));
    sscanf(src_dest, "%s %s", user, hanging_user);
    sprintf(hanging_file, FORMAT_HANGING_FILE, user, hanging_user);
    fp = fopen(hanging_file, "r");
    if (fp == NULL){
        net_sendTCP(sd, "ERROR", src_dest, strlen(src_dest));
        return;
    }
    memset(buffer, '\0', sizeof(buffer));
    while((len = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
        net_sendTCP(sd, "HANGF", buffer, len); // trasferimento di un chunk dell'hanging file
    }
    net_sendTCP(sd, "|EOH|", "", 0); // fine del trasferimento dell'hanging file
    fclose(fp);

    if(!remove(hanging_file)){
        DEBUG_PRINT(("file di messaggi non letti %s rimosso con successo", hanging_file));
    } else {
        DEBUG_PRINT(("impossibile rimuovere il file di messaggi non letti %s", hanging_file));
    }
    // adesso avvisiamo hanging_user che i suoi messaggi sono stati letti
    Server_loadUserEntry();
    UserEntry* elem;
    for (elem=Server.user_register_head; elem!=NULL; elem=elem->next){
        if(!strcmp(elem->user_dest, hanging_user)){
            if(elem->port == -1)
                break;
            new_sd = net_initTCP(elem->port);
            if(new_sd == -1)
                break;
            net_sendTCP(new_sd, "READ:", user, strlen(user));
            DEBUG_PRINT(("%s avvisato con successo della lettura dei messaggi da parte di %s", hanging_user, user));
            return;
        }
    }
    DEBUG_PRINT(("impossibile avvisare %s della lettura dei messaggi da parte di %s", hanging_user, user));
}

// Comportamento alla ricezione di un pacchetto sul protocollo TCP
//
// Il server può ricevere diversi messaggi TCP. 
// Ognuno è identificato da un comando di dimensione fissa (5 caratteri + \0)
// che determina la natura del contenuto, il quale ha dimensione variabile
//
// Una lista dei comandi riconosciuti, con significato del contenuto:
// - LOGIN -> richiesta di login. 
//              Trasmette le credenziali dell'utente
// - SIGUP -> richiesta di signup. 
//              Trasmette le credenziali dell'utente da registrare
// - LGOUT -> richiesta di logout. 
//              Se disponibile, contiene l'istante di logout. Altrimenti si considera l'istante di arrivo
// - ISONL -> richiesta di status connettività di un utente. 
//              Contiene lo username dell'utente da controllare
// - |MSG| -> richiesta di inoltro di un messaggio. 
//              Contiene il messaggio, mittente e destinatari
// - HANG? -> richiesta di utenti con messaggi pendenti. 
//              Trasmette lo username dell'utente che richiede il listing
// - SHOW: -> richiesta di ricezione dei messaggi pendenti. 
//              Contiene lo username che fa la richiesta e l'utente di cui si vogliono leggere i messaggi
void Server_handleTCP(int sd)
{
    void *tmp;
    char cmd[REQ_LEN];
    char username[USERNAME_LEN], 
        password[USERNAME_LEN],
        port_str[6];
    int dev_port;
    time_t logout_ts;
    UserEntry* elem;
    bool found;

    memset(cmd, '\0', REQ_LEN);
    net_receiveTCP(sd, cmd, &tmp);
    // DEBUG_PRINT(("comando ricevuto: %s", cmd));
    if (!strcmp("LOGIN", cmd)) {
        DEBUG_PRINT(("corpo di login: %s", (char*)tmp));
        if (sscanf(tmp, "%s %s %d", username, password, &dev_port) == 3){
            DEBUG_PRINT(("ricevuta richiesta di login da parte di ( %s : %s : %d )", username, password, dev_port));
            if (Server_checkCredentials(username, password, dev_port)) {
                net_sendTCP(sd, "OK-OK", "", 0);
                DEBUG_PRINT(("richiesta di login accettata"));
            } else {
                net_sendTCP(sd, "UKNWN", "", 0);
                DEBUG_PRINT(("rifiutata richiesta di login da utente sconosciuto"));
            }
        } else {
            net_sendTCP(sd, "ERROR", "", 0);
            DEBUG_PRINT(("rifiutata richiesta di login non valida"));
        }
    } else if (!strcmp("SIGUP", cmd)) {
        if (sscanf(tmp, "%s %s %d", username, password, &dev_port) == 3){
            DEBUG_PRINT(("ricevuta richiesta di signup da parte di ( %s : %s )", username, password));
            if (Server_signupCredentials(username, password, dev_port)) {
                net_sendTCP(sd, "OK-OK", "", 0);
                DEBUG_PRINT(("richiesta di signup accettata"));
            } else {
                net_sendTCP(sd, "KNOWN", "", 0);
                DEBUG_PRINT(("rifiutata richiesta di signup da utente già registrato"));
            }
        } else {
            net_sendTCP(sd, "ERROR", "", 0);
            DEBUG_PRINT(("rifiutata richiesta di signup non valida"));
        }
    } else if (!strcmp("LGOUT", cmd)) {
        if (tmp != NULL && sscanf(tmp, "%s %ld", username, &logout_ts) == 2) {
            Server_loadUserEntry();
            for (elem = Server.user_register_head; elem != NULL; elem = elem->next) {
                if (!strcmp(elem->user_dest, username)) {
                    if (logout_ts == 0) {
                        elem->timestamp_logout = getTimestamp();
                        elem->port = -1;
                    } else
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
                        net_sendTCP(sd, "ONLIN", port_str, strlen(port_str));
                    } else {
                        net_sendTCP(sd, "DSCNT", "", 0);
                    }
                    found = true;
                    break;
                }
            }
            if (!found)
                net_sendTCP(sd, "UNKWN", "", 0);
        } else {
            DEBUG_PRINT(("ricevuta richiesta di conenttività ma nessun username trasmesso"));
        }
    } else if (!strcmp("|MSG|", cmd)) {
        Server_forwardMsg(sd, tmp);
    } else if (!strcmp("HANG?", cmd)) {
        Server_hanging(sd, (char*)tmp);
    } else if (!strcmp("SHOW:", cmd)) {
        Server_show(sd, (char*)tmp);
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
    SCREEN_PRINT(("\n\t\t*** SERVER : %d ***", Server.port));
    Cmd_showMenu(Server.available_cmds, CMDLIST_LEN, true);
    IOMultiplex(Server.port, 
                true, 
                Server_handleSTDIN, 
                Server_handleUDP, 
                Server_handleTCP, 
                TIMEOUT, 
                Server_onTimeout);
    return 1;
}
