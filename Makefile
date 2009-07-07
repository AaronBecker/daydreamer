
#CC = gcc
CC = /usr/bin/gcc-4.2
CTAGS = ctags

DEBUGFLAGS = -g -O0
OPTFLAGS = -g -O3
PROFFLAGS = -g -O3 -pg
CFLAGS = -Wall -Wextra --std=c99 $(DEBUGFLAGS)
#CFLAGS = -Wall -Wextra --std=c99 $(OPTFLAGS)
#CFLAGS = -Wall -Wextra --std=c99 $(PROFFLAGS)

SRCFILES := $(wildcard *.c)
HEADERS  := $(wildcard *.h)
OBJFILES := $(patsubst %.c,%.o,$(wildcard *.c))

.PHONY: all clean tags

all: daydreamer

daydreamer: $(OBJFILES)
	$(CC) $(LDFLAGS) $(OBJFILES) -o daydreamer

tags: $(SRCFILES)
	$(CTAGS) $(HEADERS) $(SRCFILES)

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(OBJFILES) daydreamer tags
