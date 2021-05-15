#include <stdbool.h>

#define REQ_LEN 6    /* REQAG\0 REQRE\0 FLOOD\0*/
#define STDIN 0

struct sIOMultiplexer
{
    fd_set master;
    fd_set read_fds;
    fd_set write_fds;
    int fdmax;
} iom;

void IOMultiplex(int port, 
                struct sIOMultiplexer* iom, 
                bool use_udp, 
                void (*handleSTDIN)(),
                void (*handleUDP)(int sd),
                void (*handleTCP)(char* cmd, int sd));