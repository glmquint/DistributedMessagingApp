int net_initTCP(int sv_port);
int net_sendTCP(int sd, char protocol[6], char* buffer);
int net_receiveTCP(int sd, char protocol[6], char** buffer);
void net_askHearthBeat(int remote_port, int local_port);
void net_answerHeartBeat(int sd, int local_port);
int net_receiveHeartBeat(int sd);
