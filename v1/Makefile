
GCCFLAGS = --std=c++11
CXX = clang++ $(GCCFLAGS)

ARCHFLAGS = -m32
COMMONFLAGS = -Wall -Wextra -Wno-unused-function $(ARCHFLAGS) -Igtb
LDFLAGS = $(ARCHFLAGS) -ldl -Lgtb -lgtb -lpthread
DEBUGFLAGS = $(COMMONFLAGS) -g -O0 -DEXPENSIVE_CHECKS -DASSERT2
ANALYZEFLAGS = $(COMMONFLAGS) $(GCCFLAGS) -g -O0
DEFAULTFLAGS = $(COMMONFLAGS) -g -O2
OPTFLAGS = $(COMMONFLAGS) -O3 -DNDEBUG
PGO1FLAGS = $(OPTFLAGS) -fprofile-generate
PGO2FLAGS = $(OPTFLAGS) -fprofile-use
CXXFLAGS = $(DEFAULTFLAGS)

DBGCOMPILESTR = -DCOMPILE_COMMAND=\"\\\"`basename $(CXX)` $(DEBUGFLAGS)\\\"\"
OPTCOMPILESTR = -DCOMPILE_COMMAND=\"\\\"`basename $(CXX)` $(OPTFLAGS)\\\"\"
PGOCOMPILESTR = -DCOMPILE_COMMAND=\"\\\"`basename $(CXX)` $(PGO2FLAGS)\\\"\"
DFTCOMPILESTR = -DCOMPILE_COMMAND=\"\\\"`basename $(CXX)` $(DEFAULTFLAGS)\\\"\"

GITFLAGS = -DGIT_VERSION=\"\\\"`git rev-parse --short HEAD`\\\"\"

SRCFILES := $(wildcard *.cc)
HEADERS  := $(wildcard *.h)
OBJFILES := $(SRCFILES:.cc=.o)
PROFFILES := $(SRCFILES:.cc=.gcno) $(SRCFILES:.cc=.gcda)

.PHONY: all clean gtb tags debug opt pgo-start pgo-finish pgo-clean
.DEFAULT_GOAL := default

debug:
	$(MAKE) daydreamer \
	    CXXFLAGS="$(DEBUGFLAGS) $(GITFLAGS) $(DBGCOMPILESTR)"

default:
	$(MAKE) daydreamer \
	    CXXFLAGS="$(DEFAULTFLAGS) $(GITFLAGS) $(DFTCOMPILESTR)"

opt:
	$(MAKE) daydreamer \
	    CXXFLAGS="$(OPTFLAGS) $(GITFLAGS) $(OPTCOMPILESTR)"

pgo-start:
	$(MAKE) daydreamer \
	    CXXFLAGS="$(PGO1FLAGS) $(GITFLAGS) $(OPTCOMPILESTR)" \
	    LDFLAGS='$(LDFLAGS) -fprofile-generate'

pgo-finish:
	$(MAKE) daydreamer \
	    CXXFLAGS="$(PGO2FLAGS) $(GITFLAGS) $(PGOCOMPILESTR)"

all: default

daydreamer: gtb $(OBJFILES)
	$(CXX) $(LDFLAGS) $(OBJFILES) -o daydreamer

gtb:
	(cd gtb && $(MAKE) opt)

clean:
	rm -rf .depend daydreamer tags $(OBJFILES)

pgo-clean:
	rm -f $(PROFFILES)

.depend:
	$(CXX) -MM $(DEFAULTFLAGS) $(SRCFILES) > $@

include .depend


