
ARCHFLAGS = -m64
OPTFLAGS = -fast -msse
INCLUDE = -Isysport/ -Icompression/ -Icompression/liblzf/ \
	   -Icompression/zlib/ -Icompression/lzma/ -Icompression/huffman/
DEFINE = -DZ_PREFIX -DNDEBUG
CFLAGS = -Wall -Wextra $(INCLUDE) $(DEFINE) $(ARCHFLAGS) $(OPTFLAGS)

PGO1FLAGS = $(OPTFLAGS) -fprofile-generate
PGO2FLAGS = $(OPTFLAGS) -fprofile-use


SRCFILES := gtb-probe.c decode.c possatt.c sysport/sysport.c \
	   compression/wrap.c compression/huffman/hzip.c \
	   compression/lzma/LzmaEnc.c compression/lzma/LzmaDec.c \
	   compression/lzma/Alloc.c compression/lzma/LzFind.c \
	   compression/lzma/Lzma86Enc.c compression/lzma/Lzma86Dec.c \
	   compression/lzma/Bra86.c compression/zlib/zcompress.c \
	   compression/zlib/uncompr.c compression/zlib/inflate.c \
	   compression/zlib/deflate.c compression/zlib/adler32.c \
	   compression/zlib/crc32.c compression/zlib/infback.c \
	   compression/zlib/inffast.c compression/zlib/inftrees.c \
	   compression/zlib/trees.c compression/zlib/zutil.c \
	   compression/liblzf/lzf_c.c compression/liblzf/lzf_d.c
OBJFILES := $(patsubst %.c,%.o,$(SRCFILES))
PROFFILES := $(SRCFILES:.c=.gcno) $(SRCFILES:.c=.gcda)
LIBNAME := libgtb.a

.PHONY: all clean
.DEFAULT_GOAL := $(LIBNAME)

all: $(LIBNAME)

$(LIBNAME): $(OBJFILES)
	$(AR) rcs $@ $(OBJFILES)

obj/%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJFILES) $(LIBNAME)

pgo-clean:
	rm -f $(PROFFILES)

pgo-start:
	$(MAKE) $(LIBNAME) CFLAGS="$(PGO1FLAGS) $(CFLAGS)" \
	    LDFLAGS="$(LDFLAGS) -fprofile-generate"

pgo-finish:
	$(MAKE) $(LIBNAME) CFLAGS="$(PGO2FLAGS) $(CFLAGS)"


