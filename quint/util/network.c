#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
        exit(-1);
    }
    return sd;
}

void net_sendTCP(int sd, char protocol[6], char* buffer)
{
    int len = strlen(buffer);
    uint16_t lmsg = htons(len);
    DEBUG_PRINT(("send protocol: %s", protocol));
    if (send(sd, (void *)protocol, REQ_LEN, 0) < 0) {
        perror("Errore in fase di invio protocollo");
        exit(-1);
    }
    DEBUG_PRINT(("send buffer length: %d", len));
    if (send(sd, (void *)&lmsg, sizeof(uint16_t), 0) < 0) {
        perror("Errore in fase di invio lunghezza messaggio");
        exit(-1);
    }
    if (len > 0) { // non eseguire la send se il messaggio è vuoto
        DEBUG_PRINT(("send buffer: %s", buffer));
        if (send(sd, (char *)buffer, len, 0) < len) {
            perror("Errore in fase di invio messaggio");
            exit(-1);
        }
    }
}

// viene allocata memoria per buffer! Ricordarsi di usare free
void net_receiveTCP(int sd, char protocol[6], char** buffer)
{
    int len;
    uint16_t lmsg;

    // DEBUG_PRINT(("  before buffer=%x", *buffer));

    if (recv(sd, (void *)protocol, REQ_LEN, 0) < REQ_LEN) {
        perror("Errore in fase di ricezione protocollo");
        exit(-1);
    }
    DEBUG_PRINT(("received protocol: %s", protocol));

    if (recv(sd, (void *)&lmsg, sizeof(uint16_t), 0) < 0) {
        perror("Errore in fase di ricezione lunghezza messaggio");
        exit(-1);
    }
    len = ntohs(lmsg);
    DEBUG_PRINT(("received buffer length: %d", len));

    if (len > 0) { // se il messaggio è vuoto è inutile fare la recv
        *buffer = malloc(len);
        memset(*buffer, 0, len+1);
        if (recv(sd, (void *)*buffer, len, 0) < len) {
            char error_msg[64];
            sprintf(error_msg, "Errore in fase di ricezione messaggio (%d)", len);
            perror(error_msg);
            exit(1);
        }
        DEBUG_PRINT(("received buffer: %s", *buffer));
    } else {
        *buffer = (void*)0; // NULL
    }
    // DEBUG_PRINT(("  after buffer=%x", *buffer));
}
