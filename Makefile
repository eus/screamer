CC=gcc 
CFLAGS=-Wall -g

.PHONY: doc clean all

all: screamer listener doc

screamer: scream.o scream-common.o

listener: listen.o scream-common.o

doc:
	doxygen Doxyfile

clean:
	rm -Rf screamer listener *.o doc/*
