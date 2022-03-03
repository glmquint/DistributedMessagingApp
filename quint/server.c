#include "util/cmd.h"

#define CMDLIST_LEN 3

struct Server_s {
    int port;
    Cmd available_cmds[CMDLIST_LEN];
} Server = {-1,{
    {"help", {""}, 0, "mostra i dettagli dei comandi", false, true},
    {"list", {""}, 0, "mostra un elenco degli utenti connessi", false, true},
    {"esc", {""}, 0, "chiude il server", false, true}
}};

int main(){
    return 0;
}
