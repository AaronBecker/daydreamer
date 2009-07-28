
CLANGFLAGS =
GCCFLAGS = --std=c99

CLANGHOME = $(HOME)/local/clang
SCANVIEW = $(CLANGHOME)/scan-build
ANALYZER = $(CLANGHOME)/libexec/ccc-analyzer
#CC = $(CLANGHOME)/bin/clang $(CLANGFLAGS)
#CC = /opt/local/bin/gcc $(GCCFLAGS)
CC = /usr/bin/gcc $(GCCFLAGS)
CTAGS = ctags

COMMONFLAGS = -Wall -Wextra
DEBUGFLAGS = $(COMMONFLAGS) -g -O0
ANALYZEFLAGS = $(COMMONFLAGS) $(GCCFLAGS) -g -O0
OPTFLAGS = $(COMMONFLAGS) -g -O3 -DOMIT_CHECKS
EXTRAOPTFLAGS = $(COMMONFLAGS) -O3 -DOMIT_CHECKS -DNDEBUG
CFLAGS = $(OPTFLAGS)

SRCFILES := $(wildcard *.c)
HEADERS  := $(wildcard *.h)
OBJFILES := $(patsubst %.c,%.o,$(wildcard *.c))

.PHONY: all clean tags debug opt
.DEFAULT_GOAL := default

analyze:
	$(SCANVIEW) -k -v $(MAKE) daydreamer \
	    CC="$(ANALYZER)" CFLAGS="$(ANALYZEFLAGS)"

debug:
	$(MAKE) daydreamer CFLAGS="$(DEBUGFLAGS)"

default:
	$(MAKE) daydreamer CFLAGS="$(OPTFLAGS)"

opt:
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
