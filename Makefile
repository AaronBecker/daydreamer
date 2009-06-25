
CC = gcc
CFLAGS = -Wall -Wextra -g --std=c99
OBJFILES := $(patsubst %.c,%.o,$(wildcard *.c))

.PHONY: all clean

all: grasshopper

grasshopper: $(OBJFILES)
	$(CC) $(LDFLAGS) $(OBJFILES) -o grasshopper

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJFILES)
