struct sIOMultiplexer
{
    fd_set master;
    fd_set read_fds;
    fd_set write_fds;
    int fdmax;
} iom;

void IOMultiplex(int port, 
                // struct sIOMultiplexer* iom, 
                bool use_udp, 
                void (*handleSTDIN)(char* buffer),
                void (*handleUDP)(int sd),
                //void (*handleTCP)(char* cmd, int sd));
                void (*handleTCP)(int sd));
