#include <stdbool.h>
#include "vector.h"
int IOMultiplex(int port, 
                bool use_udp, 
                void (*handleCmd)(char* cmd), 
                void (*handleUDP)(int sd, char* cmd, char* answer),
                void (*handleTCP)(int sd, char* cmd),
                void (*handleTimeout)());

void sendNeighborsUpdate(int p_port, cvector new_neighbors)
int bootReq(int p_port, char* ds_addr, int ds_port, char* nei_list);
int sendNewUDP(int port, char* msg);
void sendUDP(int sd, char* msg);
int sendNewTCP(int port, char* msg);
void sendTCP(int sd, char* msg);
