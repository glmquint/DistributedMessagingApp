#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include "util/IOMultiplex.c"

#define SIGNUP  0
#define IN      1
#define HANGING 2
#define SHOW    3
#define CHAT    4
#define OUT     5

#define STDIN_BUF_LEN 128


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

void showMenu()
{

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
