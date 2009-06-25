
CC = gcc
CFLAGS = -Wall -Wextra -g --std=c99
CTAGS = ctags >tags
SRCFILES := $(wildcard *.c)
OBJFILES := $(patsubst %.c,%.o,$(wildcard *.c))

.PHONY: all clean

all: grasshopper

grasshopper: $(OBJFILES)
	$(CC) $(LDFLAGS) $(OBJFILES) -o grasshopper

tags: $(SRCFILES)
	$(CTAGS) $(SRCFILES)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJFILES)
