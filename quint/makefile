all: peer ds caricaArchivio caricaRegistro

peer: peer.o vector.o IOMultiplex.o network.o
	gcc -Wall peer.o vector.o IOMultiplex.o network.o -o peer

network.o: network.c
	gcc -Wall -g -c network.c

peer.o: Include.h peer.c
	gcc -Wall -g -c peer.c 

ds: ds.o vector.o IOMultiplex.o
	gcc -Wall ds.o vector.o IOMultiplex.o -o ds

ds.o: ds.c vector.h IOMultiplex.h
	gcc -Wall -g -c ds.c

vector.o: vector.c
	gcc -Wall -g -c vector.c

IOMultiplex.o: IOMultiplex.c
	gcc -Wall -g -c IOMultiplex.c

clean:
	rm *.o caricaArchivio caricaRegistro peer ds
