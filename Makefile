CC=cc
CFLAGS=-Wall -g3 -pthread

.PHONY: doc clean all

all: screamer listener

scream-common.o: scream-common.h

scream.o: scream.h

listen.o: listen.h

screamer: scream.o scream-common.o

listener: listen.o scream-common.o

doc:
	doxygen Doxyfile

clean:
	rm -Rf screamer listener *.o doc/generated/*
