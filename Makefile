CC = gcc
CFLAGS = -Wall
FILES = client.c server.c

all: $(FILES)
	make client
	make server

client: client.c
	$(CC) $? $(CFLAGS) -o $@.out

server: server.c
	$(CC) $? $(CFLAGS) -o $@.out

clean:
	rm -f *.out