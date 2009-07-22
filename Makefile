
CC = gcc
CTAGS = ctags

COMMONFLAGS = -Wall -Wextra --std=c99
DEBUGFLAGS = $(COMMONFLAGS) -g -O0
OPTFLAGS = $(COMMONFLAGS) -g -O3 -DOMIT_CHECKS
EXTRAOPTFLAGS = $(COMMONFLAGS) -O3 -DOMIT_CHECKS -DNDEBUG
CFLAGS = $(DEBUGFLAGS)

SRCFILES := $(wildcard *.c)
HEADERS  := $(wildcard *.h)
OBJFILES := $(patsubst %.c,%.o,$(wildcard *.c))

.PHONY: all clean tags debug opt
.DEFAULT_GOAL := opt

debug:
	$(MAKE) daydreamer CFLAGS="$(DEBUGFLAGS)"

opt:
	$(MAKE) daydreamer CFLAGS="$(OPTFLAGS)"

extraopt:
	$(MAKE) daydreamer CFLAGS="$(EXTRAOPTFLAGS)"

all: daydreamer

daydreamer: $(OBJFILES)
	$(CC) $(LDFLAGS) $(OBJFILES) -o daydreamer

tags: $(SRCFILES)
	$(CTAGS) $(HEADERS) $(SRCFILES)

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(OBJFILES) daydreamer tags
