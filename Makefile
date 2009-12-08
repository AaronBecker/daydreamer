
CLANGFLAGS =
GCCFLAGS = --std=c99
ARCHFLAGS = -m64

CLANGHOME = $(HOME)/local/clang
SCANVIEW = $(CLANGHOME)/scan-build
ANALYZER = $(CLANGHOME)/libexec/ccc-analyzer
CC = /usr/bin/gcc $(GCCFLAGS)
#CC = /opt/local/bin/gcc $(GCCFLAGS)
#CC = $(CLANGHOME)/bin/clang $(CLANGFLAGS)
#CC = i386-mingw32-gcc $(GCCFLAGS)
CTAGS = ctags

COMMONFLAGS = -Wall -Wextra -Wno-unused-function $(ARCHFLAGS)
LDFLAGS = $(ARCHFLAGS)
DEBUGFLAGS = $(COMMONFLAGS) -g -O0
ANALYZEFLAGS = $(COMMONFLAGS) $(GCCFLAGS) -g -O0
DEFAULTFLAGS = $(COMMONFLAGS) -g -O2 -DOMIT_CHECKS
OPTFLAGS = $(COMMONFLAGS) -O3 -DOMIT_CHECKS -DNDEBUG
CFLAGS = $(DEFAULTFLAGS)
DBGCOMPILESTR = -DCOMPILE_COMMAND=\"\\\"`basename $(CC)` $(DEBUGFLAGS)\\\"\"
OPTCOMPILESTR = -DCOMPILE_COMMAND=\"\\\"`basename $(CC)` $(OPTFLAGS)\\\"\"
DFTCOMPILESTR = -DCOMPILE_COMMAND=\"\\\"`basename $(CC)` $(DEFAULTFLAGS)\\\"\"
GITFLAGS = -DGIT_VERSION=\"\\\"`git rev-parse --short HEAD`\\\"\"

SRCFILES := $(wildcard *.c)
HEADERS  := $(wildcard *.h)
OBJFILES := $(addprefix obj/, $(patsubst %.c,%.o,$(wildcard *.c)))

.PHONY: all clean tags debug opt
.DEFAULT_GOAL := default

analyze:
	$(SCANVIEW) -k -v $(MAKE) clean daydreamer \
	    CC="$(ANALYZER)" CFLAGS="$(ANALYZEFLAGS)"

debug:
	$(MAKE) daydreamer CFLAGS="$(DEBUGFLAGS) $(GITFLAGS) $(DBGCOMPILESTR)"

default:
	$(MAKE) daydreamer CFLAGS="$(DEFAULTFLAGS) $(GITFLAGS) $(DFTCOMPILESTR)"

opt:
	$(MAKE) daydreamer CFLAGS="$(OPTFLAGS) $(GITFLAGS) $(OPTCOMPILESTR)"

all: default

daydreamer: obj $(OBJFILES)
	$(CC) $(LDFLAGS) $(OBJFILES) -o daydreamer

tags: $(SRCFILES)
	$(CTAGS) $(HEADERS) $(SRCFILES)

obj/%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

obj:
	mkdir obj

clean:
	rm -rf obj daydreamer tags

