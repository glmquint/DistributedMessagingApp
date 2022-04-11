int net_initTCP(int sv_port);
void net_sendTCP(int sd, char protocol[6], char* buffer);
void net_receiveTCP(int sd, char protocol[6], char** buffer);
void net_askHearthBeat(int port);
void net_answerHeartBeat(int sd);
