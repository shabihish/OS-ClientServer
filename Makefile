CC=gcc
CFLAGS=-I.

all: server.c
	$(CC) server.c -g -o s.o
	$(CC) client.c -g -o c.o

.PHONY: clean

clean:
	rm -f *.o *.out