
CC = gcc
CFLAGS = -Wall -Wextra -g --std=c99
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
