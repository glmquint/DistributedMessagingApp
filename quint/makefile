all: dev serv

# make rule per il client
dev: device.o IOMultiplex.o cmd.o network.o time.o
	gcc -Wall device.o IOMultiplex.o cmd.o network.o time.o -o dev

device.o: device.c
	gcc -Wall -c -g device.c

# make rule per il server
serv: server.o IOMultiplex.o cmd.o network.o time.o
	gcc -Wall server.o IOMultiplex.o cmd.o network.o time.o -o serv

server.o: server.c
	gcc -Wall -c -g server.c

IOMultiplex.o: util/IOMultiplex.c
	gcc -Wall -c -g util/IOMultiplex.c

cmd.o: util/cmd.c
	gcc -Wall -c -g util/cmd.c

network.o: util/network.c
	gcc -Wall -c -g util/network.c

time.o: util/time.c
	gcc -Wall -c -g util/time.c

# pulizia dei file della compilazione 
clean: 
	mkdir -p user1
	mkdir -p user2
	mkdir -p user3
	mkdir -p hanging/user1
	mkdir -p hanging/user2
	mkdir -p hanging/user3
	-rm *.exe *.o dev serv user*/* hanging/*/*
	echo "user1 pass -1 0 0 3\nuser2 pass -1 0 0 3\nuser3 pass -1 0 0 3" > shadow
	echo "user2\nuser4" > user1/contacts
	echo "user1\nuser3" > user2/contacts
	echo "user2" > user3/contacts
