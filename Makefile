CC ?= gcc
PKG_CONFIG ?= pkg-config
CFLAGS ?= -std=c99 -pedantic -Wall -Os
LDFLAGS ?=
LDLIBS ?=

SRC = termetris.c
OBJ = ${SRC:.c=.o}
BINDIR = /usr/local/bin

NCURSES_CFLAGS = $(shell $(PKG_CONFIG) --cflags ncurses)
NCURSES_LIBS   = $(shell $(PKG_CONFIG) --libs ncurses)

all: termetris

termetris: $(OBJ)
	$(CC) $(LDFLAGS) $^ -o $@ $(NCURSES_LIBS) $(LDLIBS)

%.o: %.c
	$(CC) $(CFLAGS) $(NCURSES_CFLAGS) -c $< -o $@

clean:
	$(RM) termetris $(OBJ)

install: all
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 termetris $(DESTDIR)$(BINDIR)

uninstall:
	$(RM) $(DESTDIR)$(BINDIR)/termetris

.PHONY: all clean install uninstall
