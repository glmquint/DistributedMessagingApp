#include <stdbool.h>

struct Cmd_s {
    char* name;
    char* arguments[20];
    int arg_num;
    char* help;
    bool requires_login;
    bool always_print;
};

typedef struct Cmd_s Cmd;

void Cmd_showMenu(Cmd* cmd_list, int cmd_list_len, bool priv);
