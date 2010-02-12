
ARCHFLAGS = -m64
OPTFLAGS = -O3 -msse
override INCLUDE = -Isysport/ -Icompression/ -Icompression/liblzf/ \
	   -Icompression/zlib/ -Icompression/lzma/ -Icompression/huffman/
override DEFINE = -DZ_PREFIX -DNDEBUG
override CFLAGS = -Wall -Wextra $(INCLUDE) $(DEFINE) $(ARCHFLAGS) $(OPTFLAGS)

SRCFILES = gtb-probe.c decode.c possatt.c sysport/sysport.c \
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

.PHONY: all clean gtb
.DEFAULT_GOAL := gtb

all: gtb

gtb: $(OBJFILES)
	$(AR) rcs libgtb.a $(OBJFILES)

obj/%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJFILES) libgtb.a

