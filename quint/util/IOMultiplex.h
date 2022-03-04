struct sIOMultiplexer
{
    fd_set master;
    fd_set read_fds;
    fd_set write_fds;
    int fdmax;
} iom;

//funzione che gestice l'IO del Ds ovvero si occupa di gestire le varie comunicazioni tra i suoi peer e l'input per mezzo della funzione select
void IOMultiplex(int port, 
                // struct sIOMultiplexer* iom, 
                bool use_udp, 
                void (*handleSTDIN)(char* buffer),
                void (*handleUDP)(int sd),
                //void (*handleTCP)(char* cmd, int sd));
                void (*handleTCP)(int sd));
