-include Makefile.cfg

OBJDIR=obj
SRCDIR=src

_CC=gcc
LD=ld
CFLAGS = -std=c11 -Wall $(shell ncursesw6-config --cflags)
DEFS += -D_POSIX_C_SOURCE
LDFLAGS = $(shell ncursesw6-config --libs)

_SZ_OBJS = main.o common.o
_EXA_OBJS = exasol.o common.o
_KABU_OBJS = kabusol.o common.o

ifdef PROFILE
CFLAGS += -pg
LDFLAGS += -pg
endif

ifdef RELEASE
DEFS += -msse3 -O3 -DNDEBUG
else
DEFS += -g -O0
endif

CC = $(CCPREFIX)$(_CC)
SZ_OBJS = $(patsubst %,$(OBJDIR)/%,$(_SZ_OBJS))
EXA_OBJS = $(patsubst %,$(OBJDIR)/%,$(_EXA_OBJS))
KABU_OBJS = $(patsubst %,$(OBJDIR)/%,$(_KABU_OBJS))

install: szsol exasol kabusol

szsol: $(SZ_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

exasol: $(EXA_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

kabusol: $(KABU_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	@mkdir -p $(OBJDIR)
	$(CC) -c -o $@ $(CFLAGS) $(DEFS) $^

$(OBJDIR):
	mkdir $(OBJDIR)

clean:
	rm -rf $(OBJDIR)/*.o szsol

.PHONY: clean install
