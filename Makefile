
#CC = gcc
CC = /usr/bin/gcc-4.2
DEBUGFLAGS = -g -O0
OPTFLAGS = -g -O3
PROFFLAGS = -g -O3 -pg
#CFLAGS = -Wall -Wextra --std=c99 $(DEBUGFLAGS)
CFLAGS = -Wall -Wextra --std=c99 $(OPTFLAGS)
#CFLAGS = -Wall -Wextra --std=c99 $(PROFFLAGS)
CTAGS = ctags
SRCFILES := $(wildcard *.c)
HEADERS  := $(wildcard *.h)
OBJFILES := $(patsubst %.c,%.o,$(wildcard *.c))

.PHONY: all clean tags

all: grasshopper

grasshopper: $(OBJFILES)
	$(CC) $(LDFLAGS) $(OBJFILES) -o grasshopper

tags: $(SRCFILES)
	$(CTAGS) $(HEADERS) $(SRCFILES)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJFILES) tags
