#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <stdbool.h>
#include "IOMultiplex.h"

#define DEBUG_OFF

#ifdef DEBUG_ON
# define DEBUG_PRINT(x) printf x
#else
# define DEBUG_PRINT(x) do {} while (0)
#endif

#define SCREEN_PRINT(x) printf("\r"); printf x; printf("\n>> ");

//funzione che gestice l'IO del Ds ovvero si occupa di gestire le varie comunicazioni tra i suoi peer e l'input per mezzo della funzione select
void IOMultiplex(int port, 
                struct sIOMultiplexer* iom, 
                bool use_udp, 
                void (*handleSTDIN)(char* buffer),
                void (*handleUDP)(int sd),
                void (*handleTCP)(char* cmd, int sd))
{
    int ret, newfd, listener, i, boot;
    socklen_t addrlen;

    struct sockaddr_in my_addr, cl_addr;
    char buffer[1024];

    listener = socket(AF_INET, SOCK_STREAM, 0);

    memset(&my_addr, 0, sizeof(my_addr));
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(port);
    my_addr.sin_addr.s_addr = INADDR_ANY;

    ret = bind(listener, (struct sockaddr *)&my_addr, sizeof(my_addr));
    if (ret < 0)
    {
        perror("Bind listener non riuscita\n");
        exit(0);
    }

    listen(listener, 10);

    FD_ZERO(&(iom->master));
    FD_ZERO(&(iom->read_fds));
    FD_ZERO(&(iom->write_fds));

    FD_SET(STDIN, &(iom->master));

    FD_SET(listener, &(iom->master));

    if (use_udp) {
        boot = socket(AF_INET, SOCK_DGRAM, 0);
        ret = bind(boot, (struct sockaddr *)&my_addr, sizeof(my_addr));
        if (ret < 0)
        {
            perror("Bind non riuscita2\n");
            exit(0);
        }
        FD_SET(boot, &(iom->master));
    } else
        boot = -1;

    if (listener > boot)
        iom->fdmax = listener;
    else
        iom->fdmax = boot;

    while (1)
    {
        iom->read_fds = iom->master;
        i = select(iom->fdmax + 1, &(iom->read_fds), NULL, NULL, NULL);
        if (i < 0)
        {
            perror("select returned %d: ");
            exit(1);
        }

        for (i = 0; i <= iom->fdmax; i++)
        {
            if (FD_ISSET(i, &(iom->read_fds)))
            {
                DEBUG_PRINT(("%d is set!!!!!!!\n", i));
                if (i == listener)
                {
                    addrlen = sizeof(cl_addr);
                    newfd = accept(listener, (struct sockaddr *)&cl_addr, &addrlen);
                    FD_SET(newfd, & iom->master);
                    if (newfd > iom->fdmax)
                        iom->fdmax = newfd;
                }
                else if (i == STDIN)
                {
                    fgets(buffer, 100, stdin);
                    handleSTDIN(buffer);
                }
                else if (i == boot)
                {
                    handleUDP(i);
                }
                else
                {
                    ret = recv(i, (void *)buffer, REQ_LEN, 0);
                    if (ret < 0)
                    {
                        perror("Errore in fase di ricezione1: ");
                        exit(1);
                    }

                handleTCP(buffer, i);
                }
            }
        }
    }

    close(listener);
}