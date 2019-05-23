#Bruker runServer og runClient til å kjøre programmene

CC=gcc
CFLAGS=-Wall	-Wextra	-std=c99

client:	client.c
	$(CC)	-g	$(CFLAGS)	$^	-o	$@

server:	server.c
	$(CC)	-g	$(CFLAGS)	$^	-o	$@

runClient:	client
	./client	127.0.0.1	2220

runServer:	server
	./server haiku.job 2220

checkClient: client
	valgrind --leak-check=full --track-origins=yes -v ./client	127.0.0.1	2009

checkServer: server
	valgrind --leak-check=full --track-origins=yes -v ./server linus.job 2009

cleanClient:
	rm	-f	client

cleanServer:
	rm	-f	server
