-include Makefile.cfg

OBJDIR=obj
SRCDIR=src

_CC=gcc
LD=ld
CFLAGS = -std=c11 -Wall $(shell ncursesw6-config --cflags)
DEFS += -D_POSIX_C_SOURCE
LDFLAGS = $(shell ncursesw6-config --libs)

_SZ_OBJS = main.o
_EXA_OBJS = exasol.o

ifdef PROFILE
CFLAGS += -pg
LDFLAGS += -pg
endif

ifdef RELEASE
DEFS += -xSSE3 -O3 -DNDEBUG
else
DEFS += -g -O0
endif

CC = $(CCPREFIX)$(_CC)
SZ_OBJS = $(patsubst %,$(OBJDIR)/%,$(_SZ_OBJS))
EXA_OBJS = $(patsubst %,$(OBJDIR)/%,$(_EXA_OBJS))

install: szsol exasol

szsol: $(SZ_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

exasol: $(EXA_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c $(OBJDIR)
	$(CC) -c -o $@ $< $(CFLAGS) $(DEFS)

$(OBJDIR):
	mkdir $(OBJDIR)

clean:
	rm -rf $(OBJDIR)/*.o szsol

.PHONY: clean install
