#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>

#define STDIN 0
#define REQ_LEN 6
#define BUFFER_LEN 1024
#define DEBUG_ON
#define USE_FORK

int IOMultiplex(int port, 
                bool use_udp, 
                void (*handleCmd)(char* cmd), 
                void (*handleUDP)(int sd, char* cmd),
                void (*handleTCP)(int sd, char* cmd),
                void (*handleTimeout)())
{
    int ret, newfd, fdmax, tcp_listener, udp_listener, i;
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

    udp_listener = -1;
    if (use_udp) {
        udp_listener = socket(AF_INET, SOCK_DGRAM, 0);
        ret = bind(udp_listener, (struct sockaddr *)&my_addr, sizeof(my_addr));
        if (ret < 0) {
            perror("Bind udp_listener\n");
            exit(0);
        }
        FD_SET(udp_listener, &master);
    }

    fdmax = (tcp_listener > udp_listener) ? tcp_listener : udp_listener;

    for(;;) {
        read_fds = master;
        
        to.tv_sec = 1;
        to.tv_usec = 0;
        if (select(fdmax + 1, &read_fds, NULL, NULL, &to) > 0){

            for (i = 0; i <= fdmax; i++) {
                //printf("%d\n", i);
                if (FD_ISSET(i, &read_fds)) {
                    #ifdef DEBUG_ON
                    printf("i: %d\n", i);
                    #endif

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
                        #ifdef DEBUG_ON
                        printf("comando da stdin: %s\n", buffer);
                        #endif
                        handleCmd(buffer);
                    }
                    else if (i == udp_listener) {
                        #ifdef DEBUG_ON
                        printf("udp_listener: %d\n", i);
                        #endif
                        addrlen = sizeof(connecting_addr);
                        ret = recvfrom(i, buffer, REQ_LEN, 0, (struct sockaddr *)&connecting_addr, &addrlen);
                        handleUDP(i, buffer);
                        //printf("%d\t%s\n", ret, buffer);
                    }
                    else {
                        #ifdef DEBUG_ON
                        printf("tcp_listener: %d\n", i);
                        #endif
                        memset(buffer, 0, REQ_LEN + 1);

                        ret = recv(i, (void *)buffer, REQ_LEN, 0);
                        if (ret < 0) {
                            perror("ricezione");
                            exit(1);
                        }
                        #ifdef DEBUG_ON
                        printf("Ricevuto messaggio valido %s\n", buffer);
                        #endif

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
    close(udp_listener);
}



