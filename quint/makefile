all: peer ds caricaArchivio caricaRegistro

peer: peer.o ArchivioAggregazioni.o RegistroGenerale.o FileManager.o IOController.o
	gcc -Wall peer.o ArchivioAggregazioni.o RegistroGenerale.o FileManager.o IOController.o -o peer

peer.o: Include.h peer.c
	gcc -Wall -g -c peer.c 

ArchivioAggregazioni.o: Include.h ArchivioAggregazioni.c
	gcc -Wall -g -c ArchivioAggregazioni.c 

RegistroGenerale.o: Include.h RegistroGenerale.c
	gcc -Wall -g -c RegistroGenerale.c 

FileManager.o: Include.h FileManager.c
	gcc -Wall -g -c FileManager.c 

IOController.o: Include.h IOController.c
	gcc -Wall -g -c IOController.c 

ds: ds.c
	gcc -Wall -g ds.c -o ds

caricaArchivio: caricaArchivio.c
	gcc -g caricaArchivio.c -o caricaArchivio

caricaRegistro: caricaRegistro.c
	gcc -g caricaRegistro.c -o caricaRegistro

clean:
	rm *o peer ds
