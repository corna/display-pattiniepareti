CC=gcc
LIBS=-lpthread
EXTRA=
PREFIX=/usr/local

all:
	$(CC) $(EXTRA) $(LIBS) -o display-pattiniepareti main.c

install:
	cp display-pattiniepareti $(PREFIX)/bin/

clean:
	rm -f *.o ./display-pattiniepareti
