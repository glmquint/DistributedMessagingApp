#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "network.h"

#define REQ_LEN 6 /* REQAG\0 REQRE\0 FLOOD\0*/

int net_initTCP(int sv_port)
{
    struct sockaddr_in sv_addr;
    int sd = socket(AF_INET, SOCK_STREAM, 0);

    memset(&sv_addr, 0, sizeof(sv_addr));
    sv_addr.sin_family = AF_INET;
    sv_addr.sin_port = htons(sv_port);
    inet_pton(AF_INET, "127.0.0.1", &sv_addr.sin_addr);

    if (connect(sd, (struct sockaddr *)&sv_addr, sizeof(sv_addr)) < 0)
    {
        perror("Errore in fase di connessione: ");
        exit(-1);
    }
    return sd;
}

void net_sendTCP(int sd, char protocol[6], char* buffer)
{
    int len = strlen(buffer);
    uint16_t lmsg = htons(len);
    if (send(sd, (void *)protocol, REQ_LEN, 0) < 0) {
        perror("Errore in fase di invio protocollo: ");
        exit(-1);
    }
    if (send(sd, (void *)&lmsg, sizeof(uint16_t), 0) < 0) {
        perror("Errore in fase di invio lunghezza messaggio: ");
        exit(-1);
    }
    if (len > 0) { // non eseguire la send se il messaggio è vuoto
        if (send(sd, (char *)buffer, len, 0) < len) {
            perror("Errore in fase di invio messaggio: ");
            exit(-1);
        }
    }
}

// this allocates memory, so remember to free it after use!
void net_receiveTCP(int sd, char* buffer)
{
    int len;
    uint16_t lmsg;

    if (recv(sd, (void *)&lmsg, sizeof(uint16_t), 0) < 0)
    {
        perror("Errore in fase di ricezione lunghezza messaggio: ");
        exit(-1);
    }

    len = ntohs(lmsg);
    if (len > 0) { // il messaggio è vuoto, inutile fare la recv
        buffer = malloc(len);
        memset(buffer, 0, len);
        if (recv(sd, (void *)buffer, len, 0) < len)
        {
            perror("Errore in fase di ricezione messaggio: ");
            exit(1);
        }
    } else {
        buffer = (void*)0; // NULL
    }

}