CFLAGS=-g3
LDLIBS=-lsqlite3

.PHONY: doc clean all

all: todo listener

scream-common.o: scream-common.h

listen.o: listen.h

todo.o:

todo: todo.o scream-common.o

listener: listen.o scream-common.o

doc:
	doxygen Doxyfile

clean:
	rm -Rf todo listener *.o
