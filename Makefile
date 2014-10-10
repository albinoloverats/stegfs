.PHONY: stegfs clean distclean

STEGFS   = stegfs
MKFS	 = mkstegfs

SOURCE   = src/main.c src/dir.c src/stegfs.c
MKSRC    = src/mkfs.c
COMMON   = src/common/error.c src/common/tlv.c

CFLAGS   = -Wall -Wextra -Werror -Wno-unused-parameter -std=gnu99 `libgcrypt-config --cflags` -pipe -O0 -ggdb
CPPFLAGS = -Isrc -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64

LIBS     = `pkg-config fuse --libs-only-l` -lmhash -lmcrypt -lpthread

all: stegfs mkfs

stegfs:
	 @$(CC) $(LIBS) $(CFLAGS) $(CPPFLAGS) $(SOURCE) -o $(STEGFS)
	-@echo "built ‘$(SOURCE)’ --> ‘$(STEGFS)’"

mkfs:
	 @$(CC) $(LIBS) $(CFLAGS) $(CPPFLAGS) $(MKSRC) $(COMMON) -o $(MKFS)
	-@echo "built ‘$(MKSRC) $(COMMON)’ --> ‘$(MKFS)’"

clean:
	 @rm -fv $(STEGFS) $(MKFS)

distclean: clean
