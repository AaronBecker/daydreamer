
CC = gcc
CFLAGS = -Wall -g --std=c99
OBJFILES := $(patsubst %.c,%.o,$(wildcard *.c))

.PHONY: all clean

all: grasshopper

grasshopper: $(OBJFILES)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJFILES)
