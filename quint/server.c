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

#define CMDLIST_LEN 3
#define MAX_CHANCES 3
#define REQ_LEN 6
#define TIMEOUT 1

#define DEBUG_OFF

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
                elem->port = -1;
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
    sprintf(hanging_folder, "./hanging/%s", username);
    if(!mkdir(hanging_folder, 0777)){
        DEBUG_PRINT(("cartella creata %s", hanging_folder));
    } else {
        DEBUG_PRINT(("errore nella creazione della cartella %s", hanging_folder));
    }
    return true;

    // return strcmp(username, "pippo"); // && strcmp(password, "P!pp0");
}

void Server_hangMsg(char* dest, char* sender, void* buffer)
{
    char hanging_file[64];
    FILE* fp;
    sprintf(hanging_file, "./hanging/%s/%s", dest, sender);
    fp = fopen(hanging_file, "a");
    if (fp == NULL) {
        DEBUG_PRINT(("Impossibile aprire file %s", hanging_file));
    } else {
        fprintf(fp, "%s", (char*)buffer);
        // fprintf(fp, "===================");
    }
    DEBUG_PRINT(("messaggio salvato in %s", hanging_file));
    fclose(fp);
}

void Server_forwardMsg(int sd, void* buffer)
{
    // TODO:
    char sender[32];
    char receivers[128];
    char msg[1024];
    char* dest;
    int newsd;
    bool at_least_one_cached;
    UserEntry* elem;
    memset(sender, '\0', 32);
    memset(receivers, '\0', 128);
    memset(msg, '\0', 1024);
    DEBUG_PRINT(("forwarding %s", (char*)buffer));
    //sscanf(buffer, "%[^\n]\n%[^\n]\n%[^\n]", sender, receivers, msg);
    sscanf(buffer, "(%[^)])<-(%[^)]): %[^\n]", receivers, sender, msg);
    DEBUG_PRINT(("sender: %s\n receivers: %s\n msg: %s", sender, receivers, msg));
    Server_loadUserEntry(); // non se ne modifica il contenuto, quindi non c'è bisogno di chiamare saveUserEntry() alla fine
    at_least_one_cached = false;
    dest = strtok(receivers, ", ");
    while(dest){
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
                        if (newsd == -1){
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

void Server_hanging(int sd, char* user)
{
    struct dirent *de;
    char userdir[64];
    DIR *dr;
    char* hanging_users;
    int count;
    char count_str[4];
    sprintf(userdir, "./hanging/%s/", user);
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

void Server_show(int sd, char* src_dest)
{
    char user[32], hanging_user[32], hanging_file[70];
    FILE* fp;
    int len, new_sd;
    char buffer[1024];
    DEBUG_PRINT(("showing hanging messages (for, from) %s", src_dest));
    sscanf(src_dest, "%s %s", user, hanging_user);
    sprintf(hanging_file, "./hanging/%s/%s", user, hanging_user);
    fp = fopen(hanging_file, "r");
    if (fp == NULL){
        net_sendTCP(sd, "ERROR", src_dest, strlen(src_dest));
        return;
    }
    memset(buffer, '\0', sizeof(buffer));
    while((len = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
        net_sendTCP(sd, "HANGF", buffer, len);
    }
    net_sendTCP(sd, "|EOH|", "", 0);
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


    /*TODO: @ghi0m
    sscanf(buffer -> user, hanging)
    fopen(./hanging/<user>/<hanging>)
    while(read):
        send("HANGF", content)
    send("|EOH|")
    for user in user_list
        if user->username == hanging && user->port != -1:
            net_init(<hanging>.port)
            send("READ:", user)
            close()
            return
    DEBUG(impossibile avvisare <hanging> della lettura dei messaggi pendenti)
    */
}

void Server_handleTCP(int sd)
{
    void *tmp;
    char cmd[REQ_LEN];
    char username[32], password[32];
    char port_str[6];
    int dev_port;
    time_t logout_ts;
    UserEntry* elem;
    bool found;
    // DEBUG_PRINT(("ricevuto messaggio TCP su socket: %d", sd));
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
        //DEBUG_PRINT(("ricevuta richiesta di inoltro messaggio: %s", (char*)tmp));
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
