.PHONY: clean distclean

STEGFS   = stegfs
MKFS	 = mkstegfs

SOURCE   = src/main.c src/dir.c src/stegfs.c
MKSRC    = src/mkfs.c
COMMON   = src/common/error.c src/common/tlv.c src/common/apple.c

CFLAGS   = -Wall -Wextra -Werror -Wno-unused-parameter -std=gnu99 `pkg-config --cflags fuse` -pipe -O0 -ggdb -I/usr/local/include
CPPFLAGS = -Isrc -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64

LIBS     = -lmhash -lmcrypt -lpthread `pkg-config --libs fuse`

all: stegfs mkfs man

stegfs:
	 @$(CC) $(LIBS) $(CFLAGS) $(CPPFLAGS) $(SOURCE) $(COMMON) -o $(STEGFS)
	-@echo "built ‘$(SOURCE)’ --> ‘$(STEGFS)’"

mkfs:
	 @$(CC) $(LIBS) $(CFLAGS) $(CPPFLAGS) $(MKSRC) $(COMMON) -o $(MKFS)
	-@echo "built ‘$(MKSRC) $(COMMON)’ --> ‘$(MKFS)’"

man:
	 @gzip -c docs/$(STEGFS).1 > $(STEGFS).1.gz
	-@echo -e "compressing ‘docs/$(STEGFS).1’ → ‘$(STEGFS).1.gz"

install:
# install stegfs and mkstegfs
	 @install -c -m 755 -s -D -T $(STEGFS) $(PREFIX)/usr/bin/$(STEGFS)
	-@echo -e "installed ‘$(STEGFS)’ → ‘$(PREFIX)/usr/bin/$(STEGFS)’"
	 @install -c -m 755 -s -D -T $(MKFS) $(PREFIX)/usr/bin/$(MKFS)
	-@echo -e "installed ‘$(MKFS)’ → ‘$(PREFIX)/usr/bin/$(MKFS)’"
# now the man page
	 @install -c -m 644 -D -T $(STEGFS).1.gz $(PREFIX)/usr/$(LOCAL)/share/man/man1/$(STEGFS).1.gz
	-@echo -e "installed ‘$(STEGFS).1.gz’ → ‘$(PREFIX)/usr/$(LOCAL)/share/man/man1/$(STEGFS).1.gz’"

uninstall:
	@rm -fv $(PREFIX)/usr/$(LOCAL)/share/man/man1/$(STEGFS).1.gz
	@rm -fv $(PREFIX)/usr/bin/$(MKFS)
	@rm -fv $(PREFIX)/usr/bin/$(STEGFS)

clean:
	 @rm -fv $(STEGFS) $(MKFS)

distclean: clean
	@rm -fv $(STEGFS).1.gz
	@rm -fvr pkg
	@rm -fv $(STEGFS)*pkg.tar.xz
