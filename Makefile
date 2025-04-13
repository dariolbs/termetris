LIBS = -lncurses -ltinfo
SRC = termetris.c
OBJ = ${SRC:.c=.o}
CFLAGS = -std=c99 -pedantic -Wall -Os
DESTDIR = /usr/local/bin
CC = gcc

all: termetris

termetris: ${OBJ}
	$(CC) ${OBJ} -o termetris ${LIBS}

termetris.o: ${SRC}
	$(CC) ${CFLAGS} -c termetris.c

clean:
	rm -f ${DESTDIR}/termetris
	rm -f ./termetris
	rm -f ./termetris.o

install: all
	cp -f termetris ${DESTDIR}

.PHONY: clean all install debug
