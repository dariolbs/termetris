SRC = termetris.c
OBJ = ${SRC:.c=.o}
CFLAGS = -std=c99 -pedantic -Wall -Os -march=native
DESTDIR = /usr/local/bin
CC = gcc
PKG_CONFIG = pkg-config

NCURSES_FLAGS = $(shell $(PKG_CONFIG) --cflags --libs ncurses)

all: termetris

termetris: ${OBJ}
	$(CC) $(CFLAGS) ${OBJ} -o termetris $(NCURSES_FLAGS)

termetris.o: ${SRC}
	$(CC) ${CFLAGS} -c termetris.c

clean:
	rm -f ./termetris
	rm -f ./termetris.o

install: all
	mkdir -p $(DESTDIR)
	cp -f termetris $(DESTDIR)
	chmod 755 $(DESTDIR)/termetris

.PHONY: clean all install debug
