#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define STDIN 0
#define REQ_LEN 6

void menu(){
    printf("questo è il menu!\n");
}

int main(){
    int ret, newfd, listener, i;
    socklen_t addrlen;

    struct sockaddr_in my_addr, cl_addr;
    char buffer[1024];
    
    fd_set master, read_fds, write_fds;

    int fdmax;

    int richesta_in_gestione = 0;

    listener = socket(AF_INET, SOCK_STREAM, 0);

    memset(&my_addr, 0, sizeof(my_addr));
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(4242);
    my_addr.sin_addr.s_addr = INADDR_ANY;

    ret = bind(listener, (struct sockaddr*)&my_addr, sizeof(my_addr));
    if (ret < 0) {
        perror("Bind");
        exit(0);
    }

    listen(listener, 10);

    FD_ZERO(&master);
    FD_ZERO(&read_fds);
    FD_ZERO(&write_fds);

    FD_SET(listener, &master);
    FD_SET(STDIN, &master);
    fdmax = listener;

    for(;;) {
        read_fds = master;
        struct timeval to;
        to.tv_sec = 0;
        to.tv_usec = 0;
        if (select(fdmax + 1, &read_fds, NULL, NULL, 0) > 0){

            for (i = 0; i <= fdmax; i++) {
                printf("%d\n", i);
                if (FD_ISSET(i, &read_fds)){
                    printf("i: %d", i);
                    if (i == listener) {
                        addrlen = sizeof(cl_addr);
                        newfd = accept(listener, (struct sockaddr*)&cl_addr, &addrlen);
                        FD_SET(newfd, &master);
                        if (newfd > fdmax)
                            fdmax = newfd;
                    }
                    else if (i == STDIN) {
                        if (richesta_in_gestione == 0)
                            menu();
                        //close(i);
                        fgets(buffer, 100, stdin);
                        //sscanf(buffer, "%s", buffer);
                        printf("%s", buffer);
                    }
                    else {
                        ret = recv(i, (void *)buffer, REQ_LEN, 0);
                        if (ret < 0) {
                            perror("ricezione");
                            exit(1);
                        }
                        printf("Ricevuto messaggio valido %s\n", buffer);
                        send(i, (void*)buffer, REQ_LEN, 0);
                        close(i);
                        FD_CLR(i, &master);

                    }
                }

            } 
        }else {
            //printf("\rjust waitin'");
            //fflush(stdout);
        }

    }
    close(listener);

}
