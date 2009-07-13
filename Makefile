
#CC = gcc
CC = /usr/bin/gcc-4.2
CTAGS = ctags

COMMONFLAGS = -Wall -Wextra --std=c99
DEBUGFLAGS = $(COMMONFLAGS) -g -O0
OPTFLAGS = $(COMMONFLAGS) -g -O3 -NDEBUG
CFLAGS = $(DEBUGFLAGS)
#CFLAGS = -Wall -Wextra --std=c99 $(OPTFLAGS)

SRCFILES := $(wildcard *.c)
HEADERS  := $(wildcard *.h)
OBJFILES := $(patsubst %.c,%.o,$(wildcard *.c))

.PHONY: all clean tags debug opt

debug:
	$(MAKE) clean all CFLAGS="$(DEBUGFLAGS)"

opt:
	$(MAKE) clean all CFLAGS="$(OPTFLAGS)"

all: daydreamer

daydreamer: $(OBJFILES)
	$(CC) $(LDFLAGS) $(OBJFILES) -o daydreamer

tags: $(SRCFILES)
	$(CTAGS) $(HEADERS) $(SRCFILES)

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(OBJFILES) daydreamer tags
