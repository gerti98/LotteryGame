CC= gcc
CFLAGS= -c -Wall 

all: lotto_server lotto_client

lotto_server.o: lotto_server.c 
	$(CC) $(CFLAGS) lotto_server.c

lotto_client.o: lotto_client.c
	$(CC) $(CFLAGS) lotto_client.c

lotto_server: lotto_server.o
	$(CC) lotto_server.o -o lotto_server

lotto_client: lotto_client.o
	$(CC) lotto_client.o -o lotto_client 

clean:
	rm *.o lotto_server lotto_client
