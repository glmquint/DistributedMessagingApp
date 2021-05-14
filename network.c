#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include "vector.h"

#define STDIN 0
#define REQ_LEN 6
#define BUFFER_LEN 1024
#define TIMEOUT 1

#define DEBUG_ON

#ifdef DEBUG_ON
# define DEBUG_PRINT(x) printf x
#else
# define DEBUG_PRINT(x) do {} while (0)
#endif
#define DONT_USE_FORK

void sendNeighborsUpdate(int peer, cvector new_neighbors)
{
    int ret, sd;
    struct sockaddr_in peer_addr;
    char buffer[1024];
    memset(buffer, 0, sizeof(buffer));

    if (peer <= 0)
        return;

    sd = socket(AF_INET, SOCK_STREAM, 0);

    memset(&peer_addr, 0, sizeof(peer_addr));
    peer_addr.sin_family = AF_INET;
    peer_addr.sin_port = htons(peer);
    inet_pton(AF_INET, "127.0.0.1", &peer_addr.sin_addr);

    ret = connect(sd, (struct sockaddr *)&peer_addr, sizeof(peer_addr));
    if (ret < 0)
    {
        perror("Errore in fase di connessione: ");
        exit(-1);
    }

    strcpy(buffer, "UPDNEI");

    ret = send(sd, (void *)buffer, REQ_LEN, 0);
    if (ret < 0)
    {
        perror("Errore in fase di invio: ");
        exit(1);
    }

    strcpy(buffer, RegistroPeer.boot_vicini);

    printf("Comunico al peer %d i suoi nuovi vicini\n\n", peer);
    ret = send(sd, (void *)buffer, BOOT_RESP, 0);
    if (ret < 0)
    {
        perror("Errore in fase di invio: ");
        exit(1);
    }
}

int bootReq(int p_port, char* ds_addr, int ds_port, char* nei_list)
{
    /* POSSIBLE NON-BLOCKING WIP ALTERNATIVE
    SOCKET     theSocket;
    struct timeval stTimeOut;

    fd_set stReadFDS;
    fd_set stExcepFDS;
    fd_set stWriteFDS;

    FD_ZERO(&stReadFDS);
    FD_ZERO(&stExcepFDS);
    FD_ZERO(&stWriteFDS);

    // Timeout of one second
    stTimeOut.tv_sec = 1;
    stTimeOut.tv_usec = 0;

    FD_SET(theSocket, &stReadFDS);
    FD_SET(theSocket, &stExcepFDS);
    FD_SET(theSocket, &stWriteFDS);

    int test = 0;
    char szBuf[256];
    memset(szBuf, ' ', sizeof(szBuf));

    while (true) {
        int t = select(-1, &stReadFDS, &stWriteFDS, &stExcepFDS, &stTimeOut);
        if (t = SOCKET_ERROR) {
            fprintf(stderr, "Call to select() failed");
            exit(1);
        }
        if (t != 0) {
            printf("Something to do...");
            if (FD_ISSET (theSocket, &stExcepFDS)) {
                 printf("Exception flag is set");// Deal with this
            }
            if (FD_ISSET(theSocket, &stReadFDS)) {
                 printf("There is data pending to be read..."); // Read data with recv()
            }
            if (FD_ISSET(theSocket, &stWriteFDS)) {
                 printf("There is data pending to be written...");// Send Data with send()

            }
        }
        else {
            printf("t is zero");
            exit(1);
        }
    }
    */
    
    //faccio il boot
    int ret, sd;
    socklen_t len;
    struct sockaddr_in srv_addr;
    char buffer[1024], msg[1024], tmp[10];
    sprintf(tmp, "%d", p_port);
    strcpy(msg, tmp);

    sd = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, 0);

    memset(&srv_addr, 0, sizeof(srv_addr));
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(ds_port);
    inet_pton(AF_INET, ds_addr, &srv_addr.sin_addr);

    do
    {
        printf("Sono %d e invio il messaggio di boot: %s %d\n", p_port, ds_addr, ds_port);
        ret = sendto(sd, msg, 1024, 0, (struct sockaddr *)&srv_addr, sizeof(srv_addr));
        sleep(TIMEOUT);
        ret = recvfrom(sd, buffer, 1024, 0, (struct sockaddr *)&srv_addr, &len);
        //se dopo aver aspettato il timeout non ricevo riposta riinvio il messaggio
    } while (ret < 0);

    printf("Richiesta di boot accettata\n");

    sscanf(buffer, "%d %s", &ret, nei_list);

    close(sd);
    return(ret);
}
int sendNewUDP(int port, char* msg)
{
    return(-1);
}

void sendUDP(int sd, char* msg)
{
}

int sendNewTCP(int port, char* msg)
{
    return(-1);
}

void sendTCP(int sd, char* msg)
{
}

int IOMultiplex(int port, 
                bool use_udp, 
                void (*handleCmd)(char* cmd), 
                void (*handleUDP)(int sd, char* cmd, char* answer),
                void (*handleTCP)(int sd, char* cmd),
                void (*handleTimeout)())
{
    int ret, newfd, fdmax, tcp_listener, udp_socket, i;
#ifdef USE_FORK
    pid_t pid;
#endif
    socklen_t addrlen;

    struct sockaddr_in my_addr, cl_addr, connecting_addr;
    char buffer[BUFFER_LEN];
    
    fd_set master, read_fds, write_fds;

    struct timeval to;
    
    memset(&my_addr, 0, sizeof(my_addr));
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(port);
    my_addr.sin_addr.s_addr = INADDR_ANY;

    tcp_listener = socket(AF_INET, SOCK_STREAM, 0);
    if (tcp_listener < 0) {
        perror("tcp_listener");
        exit(0);
    }

    ret = bind(tcp_listener, (struct sockaddr*)&my_addr, sizeof(my_addr));
    if (ret < 0) {
        perror("bind");
        exit(0);
    }
    
    listen(tcp_listener, 10);

    FD_ZERO(&master);
    FD_ZERO(&read_fds);
    FD_ZERO(&write_fds);

    FD_SET(tcp_listener, &master);
    FD_SET(STDIN, &master);

    udp_socket = -1;
    if (use_udp) {
        udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
        ret = bind(udp_socket, (struct sockaddr *)&my_addr, sizeof(my_addr));
        if (ret < 0) {
            perror("Bind udp_socket\n");
            exit(0);
        }
        FD_SET(udp_socket, &master);
    }

    fdmax = (tcp_listener > udp_socket) ? tcp_listener : udp_socket;

    for(;;) {
        read_fds = master;
        
        to.tv_sec = 1;
        to.tv_usec = 0;
        if (select(fdmax + 1, &read_fds, NULL, NULL, &to) > 0){

            for (i = 0; i <= fdmax; i++) {
                //printf("%d\n", i);
                if (FD_ISSET(i, &read_fds)) {
                    DEBUG_PRINT(("i: %d\n", i));

                    if (i == tcp_listener) {
                        addrlen = sizeof(cl_addr);
                        newfd = accept(tcp_listener, (struct sockaddr*)&cl_addr, &addrlen);
                        FD_SET(newfd, &master);
                        if (newfd > fdmax)
                            fdmax = newfd;
                    }
                    else if (i == STDIN) {
                        memset(buffer, 0, BUFFER_LEN);
                        fgets(buffer, BUFFER_LEN - 1, stdin);
                        DEBUG_PRINT(("comando da stdin: %s\n", buffer));
                        handleCmd(buffer);
                    }
                    else if (i == udp_socket) {
                        char answer[1024];
                        memset(answer, 0, 1024);
                        DEBUG_PRINT(("udp_socket: %d\n", i));
                        addrlen = sizeof(connecting_addr);
                        ret = recvfrom(i, buffer, REQ_LEN, 0, (struct sockaddr *)&connecting_addr, &addrlen);
                        handleUDP(i, buffer, answer);
                        sendto(i, answer, strlen(answer), 0, (struct sockaddr *)&connecting_addr, sizeof(connecting_addr));
                    }
                    else {
                        DEBUG_PRINT(("tcp_listener: %d\n", i));
                        memset(buffer, 0, REQ_LEN + 1);

                        ret = recv(i, (void *)buffer, REQ_LEN, 0);
                        if (ret < 0) {
                            perror("ricezione");
                            exit(1);
                        }
                        DEBUG_PRINT(("Ricevuto messaggio valido %s\n", buffer));

                        #ifdef USE_FORK
                        pid = fork();
                        if (pid == -1) {
                            perror("pid");
                        }
                        
                        if (pid == 0) {
                            // figlio
                            close(tcp_listener);
                            send(i, (void*)buffer, REQ_LEN, 0);
                            handleTCP(i, buffer);
                            close(i);
                            exit(0);
                        }
                        // padre
                        close(i);
                        FD_CLR(i, &master);
                        #else
                        send(i, (void*)buffer, REQ_LEN, 0);
                        handleTCP(i, buffer);
                        close(i);
                        FD_CLR(i, &master);
                        #endif
                    }
                }
            } 
        }else {
            handleTimeout();
        }

    }
    close(tcp_listener);
    close(udp_socket);
}
