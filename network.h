#include <stdbool.h>
int IOMultiplex(int port, 
                bool use_udp, 
                void (*handleCmd)(char* cmd), 
                void (*handleUDP)(int sd, char* cmd),
                void (*handleTCP)(int sd, char* cmd),
                void (*handleTimeout)());
