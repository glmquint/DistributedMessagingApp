#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include "util/IOMultiplex.h"

#define SIGNUP  0
#define IN      1
#define HANGING 2
#define SHOW    3
#define CHAT    4
#define OUT     5

#define STDIN_BUF_LEN 128

#define SCREEN_PRINT(x) printf("\r"); printf x; printf("\n>> "); fflush(stdout);

struct Device {
    bool is_logged_in;
    int port;
    char stdin_buffer[STDIN_BUF_LEN];
} Device;

void DeviceInit(int argv, char *argc[])
{
    if (argv < 2) {
        printf("Utilizzo: %s <porta>", argc[0]);
        exit(0);
    }
    Device.port = atoi(argc[0]);
    Device.is_logged_in = false;
}

void Device_showMenu()
{
    SCREEN_PRINT(("\nDigita un comando:\n"));
    SCREEN_PRINT(("    signup <username> <password> --> crea un account sul server"));
    SCREEN_PRINT(("    in <srv_port> <username> <password> --> richiede al server la conneione al servizio"));
    SCREEN_PRINT(("    hanging --> riceve la lista degli utenti che hanno inviaato messaggi mentre si era offline"));
    SCREEN_PRINT(("    show <username> --> riceve dal server i messaggi pendenti inviati da <username> mentre si era offline"));
    SCREEN_PRINT(("    chat <username> --> avvia una chat con l'utente <username>"));
    SCREEN_PRINT(("    share <file_name> --> invia il file <file_name> (nella current directory) al device su cui è connesso l'utente o gli utenti con cui si sta chattando"));
    SCREEN_PRINT(("    out --> richiede una disconnessione dal network"));
    SCREEN_PRINT(("    esc --> chiude l'applicazione\n"));
}

void Device_esc()
{
    SCREEN_PRINT(("Arrivederci!"));
    exit(0);
}

void Device_handleSTDIN(char* buffer)
{
    char tmp[20];
    sscanf(buffer, "%s", tmp);
    if(!strcmp("esc", tmp))
        Device_esc();
    else
        SCREEN_PRINT(("comando non valido: %s", tmp));
    Device_showMenu();
}

void showAccessPortal()
{
}

void handleInput()
{
    Device.is_logged_in = true;
}

int main(int argv, char *argc[]) 
{
    /*
    sscanf(argc[1], "%d", &Client.port);
    printf("Inserire username e password: \n");
    memset(buffer, 0, 128);
    read(STDIN_FILENO, buffer, 128);
    buffer[127] = 0x0;
    printf("now buffer is %s", buffer);
    show_menu();
    */
    DeviceInit(argv, argc);
    while(! Device.is_logged_in) {
        showAccessPortal();
        handleInput();
    }

    return 0;
}
