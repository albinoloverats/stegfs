.PHONY: stegfs clean distclean

STEGFS   = stegfs
MKFS	 = mkstegfs

SOURCE   = src/main.c src/dir.c src/stegfs.c
MKSRC    = src/mkfs.c
COMMON   = src/common/error.c src/common/tlv.c

CFLAGS   = -Wall -Wextra -Werror -Wno-unused-parameter -std=gnu99 -I/usr/local/include -pipe -O0 -ggdb
CPPFLAGS = -Isrc -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64

LIBS     = -lmhash -lmcrypt -lpthread -L/usr/local/lib -lfuse

all: stegfs mkfs

stegfs:
	 @$(CC) $(LIBS) $(CFLAGS) $(CPPFLAGS) $(SOURCE) $(COMMON) -o $(STEGFS)
	-@echo "built ‘$(SOURCE)’ --> ‘$(STEGFS)’"

mkfs:
	 @$(CC) $(LIBS) $(CFLAGS) $(CPPFLAGS) $(MKSRC) $(COMMON) -o $(MKFS)
	-@echo "built ‘$(MKSRC) $(COMMON)’ --> ‘$(MKFS)’"

clean:
	 @rm -fv $(STEGFS) $(MKFS)

distclean: clean
