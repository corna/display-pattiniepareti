CC=gcc
LIBS=-lpthread
EXTRA=
PREFIX=/usr/local

all:
	$(CC) $(EXTRA) $(LIBS) -o display main.c

install:
	cp display $(PREFIX)/bin/

clean:
	rm -f *.o ./display
