#include <stdbool.h>
#include <stdio.h>

#define SCREEN_PRINT(x) printf("\r"); printf x; printf("\n>> "); fflush(stdout);

struct Cmd_s {
    char* name;
    char* arguments[20];
    int arg_num;
    char* help;
    bool requires_login;
    bool always_print;
};

typedef struct Cmd_s Cmd;

void Cmd_showMenu(Cmd* cmd_list, int cmd_list_len, bool priv)
{
    SCREEN_PRINT(("        \nDigita un comando:\n"));
    for (int i = 0; i < cmd_list_len; i++) {
        if (cmd_list[i].always_print || cmd_list[i].requires_login == priv) {
            char current_args[50] = "";
            for (int j = 0; j < cmd_list[i].arg_num; j++)
                sprintf(current_args, "%s <%s>", current_args, cmd_list[i].arguments[j]);
            SCREEN_PRINT(("    %s%s --> %s", cmd_list[i].name, current_args, cmd_list[i].help));
        }
    }
}
