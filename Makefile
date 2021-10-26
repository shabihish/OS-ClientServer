CC=gcc
CFLAGS=-I.

all: s.o c.o

s.o: server.c
	$(CC) server.c -g -o s.o

c.o: client.c
	$(CC) client.c -g -o c.o


.PHONY: clean
clean:
	rm -f *.o *.out