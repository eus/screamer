CFLAGS=-Wall -g3 -pthread
LDLIBS=-lipq

.PHONY: doc clean all

all: screamer listener screamer_filter

scream-common.o: scream-common.h

scream.o: scream.h

listen.o: listen.h

screamer_filter.o: scream-common.h

screamer_filter: screamer_filter.o

screamer: scream.o scream-common.o

listener: listen.o scream-common.o

doc:
	doxygen Doxyfile

clean:
	rm -Rf screamer listener screamer_filter *.o
