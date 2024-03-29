LIBS = -lncurses
SRC = termetris.c
OBJ = ${SRC:.c=.o}
CFLAGS = -std=c99 -pedantic -Wall -Os
DESTDIR = /usr/local/bin

all: termetris

termetris: ${OBJ}
	gcc ${OBJ} -o termetris ${LIBS}

termetris.o: ${SRC}
	gcc ${CFLAGS} -c termetris.c

clean:
	rm -f ${DESTDIR}/termetris

install: all
	cp -f termetris ${DESTDIR}

.PHONY: clean all install debug
