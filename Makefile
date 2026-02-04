CC = gcc
CFLAGS = -Wall -Wextra -std=c99
LDFLAGS = -lpthread

all: server client

server: server.c
	$(CC) $(CFLAGS) -o server server.c $(LDFLAGS)

client: client.c
	$(CC) $(CFLAGS) -o client client.c

clean:
	rm -f server client

run-server: server
	./server

run-client: client
	./client

.PHONY: all clean run-server run-client
