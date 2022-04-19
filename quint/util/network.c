#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "network.h"

#define DEBUG_OFF

#ifdef DEBUG_ON
# define DEBUG_PRINT(x) printf("  [debug]: "); printf x; printf("\n"); fflush(stdout)
#else
# define DEBUG_PRINT(x) do {} while (0)
#endif

#define REQ_LEN 6 /* LOGIN\0 */

int net_initTCP(int sv_port)
{
    struct sockaddr_in sv_addr;
    int sd = socket(AF_INET, SOCK_STREAM, 0);

    memset(&sv_addr, 0, sizeof(sv_addr));
    sv_addr.sin_family = AF_INET;
    sv_addr.sin_port = htons(sv_port);
    inet_pton(AF_INET, "127.0.0.1", &sv_addr.sin_addr);

    if (connect(sd, (struct sockaddr *)&sv_addr, sizeof(sv_addr)) < 0) {
        perror("Errore in fase di connessione");
        return -1;
    }
    return sd;
}

int net_sendTCP(int sd, char protocol[6], void* buffer, int len)
{
    // int len;
    uint32_t lmsg;
    int ret; 
    // len = sizeof(buffer);
    lmsg = htonl(len);
    DEBUG_PRINT(("send protocol: %s", protocol));
    if (send(sd, (void *)protocol, REQ_LEN, 0) < 0) {
        perror("Errore in fase di invio protocollo");
        exit(-1);
    }
    DEBUG_PRINT(("send buffer length: %d", len));
    if (ret = send(sd, (void *)&lmsg, sizeof(uint32_t), 0) < 0) {
        perror("Errore in fase di invio lunghezza messaggio");
        exit(-1);
    }
    if (len > 0) { // non eseguire la send se il messaggio è vuoto
        DEBUG_PRINT(("send buffer: %s", (char*)buffer));
        if (ret = send(sd, buffer, len, 0) < len) {
            // DEBUG_PRINT(("inviati %d bytes", ret));
            perror("Errore in fase di invio messaggio");
            exit(-1);
        }
    }
    return ret;
}

// viene allocata memoria per buffer! Ricordarsi di usare free
int net_receiveTCP(int sd, char protocol[6], void** buffer)
{
    int len;
    uint32_t lmsg;
    char error_msg[64];

    // DEBUG_PRINT(("  before buffer=%x", *buffer));
    memset(protocol, '\0', 6);

    if (recv(sd, (char *)protocol, REQ_LEN, 0) < REQ_LEN) {
        DEBUG_PRINT((">received %s on %d", (char*)protocol, sd));
        perror("Errore in fase di ricezione protocollo");
        exit(-1);
    }
    DEBUG_PRINT(("received protocol: %s", protocol));

    if (recv(sd, (void *)&lmsg, sizeof(uint32_t), 0) < 0) {
        perror("Errore in fase di ricezione lunghezza messaggio");
        exit(-1);
    }
    len = ntohl(lmsg);
    DEBUG_PRINT(("received buffer length: %d", len));

    if (len > 0) { // se il messaggio è vuoto è inutile fare la recv
        *buffer = malloc(len);
        memset(*buffer, 0, len+1);
        if (recv(sd, (void *)*buffer, len, 0) < len) {
            sprintf(error_msg, "Errore in fase di ricezione messaggio (%d)", len);
            perror(error_msg);
            exit(1);
        }
        DEBUG_PRINT(("received buffer: %s", (char*)*buffer));
    } else {
        *buffer = (void*)0;
    }
    return len;
}

void net_askHearthBeat(int remote_port, int local_port)
{
    int sd;
    struct sockaddr_in servaddr;
    char* heart_beat_msg = "HRTBT";
    uint16_t local_port_net;
    DEBUG_PRINT(("asking heart beat to %d", remote_port));

    if ((sd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket creation failed");
        exit(0);
    }

    memset(&servaddr, 0, sizeof(servaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(remote_port);
    inet_pton(AF_INET, "127.0.0.1", &servaddr.sin_addr);

    if (sendto(sd, heart_beat_msg, REQ_LEN, 0, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0)
        perror("errore durante la sendto: ");

    local_port_net = htons(local_port);
    if (sendto(sd, &local_port_net, sizeof(local_port_net), 0, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0)
        perror("errore durante la sendto: ");

    close(sd);
}

void net_answerHeartBeat(int sd, int local_port)
{
    char buffer[REQ_LEN];
    int n, len;
    struct sockaddr_in claddr;
    char* alive_msg = "ALIVE";
    uint16_t remote_port_net, local_port_net;

    len = sizeof(claddr);
    if (recvfrom(sd, buffer, REQ_LEN, 0, (struct sockaddr*)&claddr, &len) < 0)
        perror("[1] errore durante la recvfrom: ");

    DEBUG_PRINT(("ricevuto su UDP: %s", buffer));
    if (strcmp(buffer, "HRTBT"))
        return;

    if (recvfrom(sd, &remote_port_net, sizeof(remote_port_net), 0, (struct sockaddr*)&claddr, &len) < 0)
        perror("[2] errore durante la recvfrom: ");

    claddr.sin_family = AF_INET;
    claddr.sin_port = remote_port_net; // non c'è bisogno di usare htons in quanto è già nel formato net
    // inet_pton(AF_INET, "127.0.0.1", &claddr.sin_addr);

    if (sendto(sd, alive_msg, REQ_LEN, 0, (struct sockaddr*)&claddr, sizeof(claddr)) < REQ_LEN)
        perror("[1] errore durante la sendto");

    local_port_net = htons(local_port);
    if (sendto(sd, &local_port_net, sizeof(local_port_net), 0, (struct sockaddr*)&claddr, sizeof(claddr)) < 0)
        perror("[2] errore durante la sendto: ");

    DEBUG_PRINT(("answered alive to %d (n = %d)", ntohs(claddr.sin_port), n));
}

int net_receiveHeartBeat(int sd)
{
    char buffer[REQ_LEN];
    int remote_port, len;
    struct sockaddr_in claddr;
    uint16_t remote_port_net;

    remote_port = -1;
    len = sizeof(claddr);
    if (recvfrom(sd, buffer, REQ_LEN, 0, (struct sockaddr*)&claddr, &len) < 0)
        perror("[1] errore durante la recvfrom: ");
    if (!strcmp(buffer, "ALIVE")) {
        if (recvfrom(sd, &remote_port_net, sizeof(remote_port_net), 0, (struct sockaddr*)&claddr, &len) < 0)
            perror("[2] errore durante la recvfrom: ");
        remote_port = htons(remote_port_net);
    }
    return remote_port;
}
