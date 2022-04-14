#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <stdbool.h>

#define STDIN 0
#define REQ_LEN 6

#define DEBUG_OFF

#ifdef DEBUG_ON
# define DEBUG_PRINT(x) printf("[DEBUG]: "); printf x; printf("\n"); fflush(stdout)
#else
# define DEBUG_PRINT(x) do {} while (0)
#endif

#define SCREEN_PRINT(x) printf("\r"); printf x; printf("\n>> ");

struct sIOMultiplexer
{
    fd_set master;
    fd_set read_fds;
    // fd_set write_fds;
    int fdmax;
} iom;

void IOMultiplex(int port, 
                bool use_udp, 
                void (*handleSTDIN)(char* buffer),
                void (*handleUDP)(int sd),
                void (*handleTCP)(int sd),
                int timeout_sec,
                void (*onTimeout)())
{
    int ret, newfd, listener, i, udp_socket;
    socklen_t addrlen;
    struct sockaddr_in my_addr, cl_addr;
    char buffer[1024];
    pid_t pid;
    struct timeval tv;

    listener = socket(AF_INET, SOCK_STREAM, 0);

    memset(&my_addr, 0, sizeof(my_addr));
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(port);
    my_addr.sin_addr.s_addr = INADDR_ANY;

    ret = bind(listener, (struct sockaddr *)&my_addr, sizeof(my_addr));
    if (ret < 0) {
        perror("Bind listener non riuscita\n");
        exit(0);
    }

    listen(listener, 10);
    DEBUG_PRINT(("in ascolto sulla porta %d", port));

    FD_ZERO(&(iom.master));
    FD_ZERO(&(iom.read_fds));
    // FD_ZERO(&(iom.write_fds));

    FD_SET(STDIN, &(iom.master));
    FD_SET(listener, &(iom.master));

    if (use_udp) {
        udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
        ret = bind(udp_socket, (struct sockaddr *)&my_addr, sizeof(my_addr));
        if (ret < 0) {
            perror("Bind UDP non riuscita\n");
            exit(0);
        }
        FD_SET(udp_socket, &(iom.master));
    } else
        udp_socket = -1;

    iom.fdmax = (listener > udp_socket) ? listener : udp_socket;

    tv.tv_sec = timeout_sec;
    tv.tv_usec = 0;

    while (1) {
        iom.read_fds = iom.master;
        if (timeout_sec != 0) {
            i = select(iom.fdmax + 1, &(iom.read_fds), NULL, NULL, &tv);
        } else 
            i = select(iom.fdmax + 1, &(iom.read_fds), NULL, NULL, NULL);
        if (i < 0) {
            perror("select: ");
            exit(1);
        }
        if (i > 0) {

            for (i = 0; i <= iom.fdmax+1; i++) {
                if (FD_ISSET(i, &(iom.read_fds))) {
                    DEBUG_PRINT(("%d is set\n", i));
                    if (i == listener) { // 
                        addrlen = sizeof(cl_addr);
                        newfd = accept(listener, (struct sockaddr *)&cl_addr, &addrlen);
                        FD_SET(newfd, & iom.master);
                        if (newfd > iom.fdmax)
                            iom.fdmax = newfd;
                    }
                    else if (i == STDIN) { // input da tastiera
                        // if (fgets(buffer, sizeof buffer, stdin))
                        if (read(STDIN, buffer, sizeof(buffer)))
                        // while (read(STDIN, buffer, sizeof(buffer))>0)
                            handleSTDIN(buffer);
                    }
                    else if (i == udp_socket) { // messaggio UDP
                        handleUDP(i);
                    }
                    else { // messaggio TCP
                        // handleTCP(i);
                        pid = fork();
                        if (pid == -1) {
                            perror("Errore durante la fork: ");
                            return;
                        }
                        if (pid == 0) { // figlio
                            close(listener);
                            handleTCP(i);
                            exit(0);
                        }
                        close(i);
                        FD_CLR(i, &(iom.master));
                    }
                }
            }
        } else { // select == 0 (timeout elapsed)
            /*
            printf("."); // TODO: sanity checks
            fflush(stdout);
            */
            onTimeout();
            tv.tv_sec = timeout_sec;
            tv.tv_usec = 0;
        }
    }

    close(listener);
}
