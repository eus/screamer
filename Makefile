CC=gcc 
CFLAGS=-Wall -g

all: screamer listener

screamer: scream.o scream-common.o

listener: listen.o scream-common.o

clean:
	rm -f screamer listener *.o
