
CLANGFLAGS =
GCCFLAGS = --std=c99
ARCHFLAGS = -m64
#ARCHFLAGS = -m32

CLANGHOME = $(HOME)/local/clang
SCANVIEW = $(CLANGHOME)/scan-build
ANALYZER = $(CLANGHOME)/libexec/ccc-analyzer
#CC = /opt/local/bin/gcc $(GCCFLAGS)
CC = /usr/bin/gcc $(GCCFLAGS)
#CC = $(CLANGHOME)/bin/clang $(CLANGFLAGS)
#CC = i386-mingw32-gcc $(GCCFLAGS)
CTAGS = ctags

COMMONFLAGS = -Wall -Wextra -Wno-unused-function $(ARCHFLAGS) -Igtb
LDFLAGS = $(ARCHFLAGS) -ldl -Lgtb -lgtb
DEBUGFLAGS = $(COMMONFLAGS) -g -O0 -DEXPENSIVE_TESTS -DASSERT2
ANALYZEFLAGS = $(COMMONFLAGS) $(GCCFLAGS) -g -O0
DEFAULTFLAGS = $(COMMONFLAGS) -g -O2
OPTFLAGS = $(COMMONFLAGS) -O3 -msse -DNDEBUG
PGO1FLAGS = $(OPTFLAGS) -fprofile-generate
PGO2FLAGS = $(OPTFLAGS) -fprofile-use
CFLAGS = $(DEFAULTFLAGS)

DBGCOMPILESTR = -DCOMPILE_COMMAND=\"\\\"`basename $(CC)` $(DEBUGFLAGS)\\\"\"
OPTCOMPILESTR = -DCOMPILE_COMMAND=\"\\\"`basename $(CC)` $(OPTFLAGS)\\\"\"
PGOCOMPILESTR = -DCOMPILE_COMMAND=\"\\\"`basename $(CC)` $(PGO2FLAGS)\\\"\"
DFTCOMPILESTR = -DCOMPILE_COMMAND=\"\\\"`basename $(CC)` $(DEFAULTFLAGS)\\\"\"

GITFLAGS = -DGIT_VERSION=\"\\\"`git rev-parse --short HEAD`\\\"\"

SRCFILES := $(wildcard *.c)
HEADERS  := $(wildcard *.h)
OBJFILES := $(addprefix obj/, $(patsubst %.c,%.o,$(wildcard *.c)))

.PHONY: all clean gtb tags debug opt pgo-start pgo-finish pgo-clean
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

pgo-start:
	$(MAKE) daydreamer CFLAGS="$(PGO1FLAGS) $(GITFLAGS) $(OPTCOMPILESTR)" \
	    LDFLAGS="$(LDFLAGS) -fprofile-generate"

pgo-finish: pgo-clean
	$(MAKE) daydreamer CFLAGS="$(PGO2FLAGS) $(GITFLAGS) $(PGOCOMPILESTR)"

all: default

daydreamer: gtb obj $(OBJFILES)
	$(CC) $(LDFLAGS) $(OBJFILES) -o daydreamer

tags: $(SRCFILES)
	$(CTAGS) $(HEADERS) $(SRCFILES)

obj/%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

obj:
	mkdir obj

gtb:
	(cd gtb && $(MAKE))

pgo-clean:
	rm obj/*.o daydreamer

clean:
	rm -rf obj daydreamer tags && (cd gtb && $(MAKE) clean)

