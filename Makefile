.PHONY: clean distclean

STEGFS   = stegfs
MKFS	 = mkstegfs

SOURCE   = src/main.c src/stegfs.c src/help.c
MKSRC    = src/mkfs.c
COMMON   = src/common/error.c src/common/crypt.c src/common/tlv.c src/common/dir.c src/common/non-gnu.c

CFLAGS   = -Wall -Wextra -Werror -std=gnu99 `pkg-config --cflags fuse` -pipe -O0 -ggdb -pg -lc -I/usr/local/include
CPPFLAGS = -Isrc -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -DGIT_COMMIT=\"`git log | head -n1 | cut -f2 -d' '`\"

# -lpthread
LIBS     = -lgcrypt `pkg-config --libs fuse`

all: sfs mkfs man

sfs:
	 @$(CC) $(LIBS) $(CFLAGS) $(CPPFLAGS) $(SOURCE) $(COMMON) -o $(STEGFS)
	-@echo "built ‘$(SOURCE)’ --> ‘$(STEGFS)’"

debug:
	 @$(CC) $(LIBS) $(CFLAGS) $(CPPFLAGS) -DUSE_PROC $(SOURCE) $(COMMON) -o $(STEGFS)
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
# now the man page(s)
	 @install -c -m 644 -D -T $(STEGFS).1.gz $(PREFIX)/usr/$(LOCAL)/share/man/man1/$(STEGFS).1.gz
	-@echo -e "installed ‘$(STEGFS).1.gz’ → ‘$(PREFIX)/usr/$(LOCAL)/share/man/man1/$(STEGFS).1.gz’"
	 @ln -f $(PREFIX)/usr/$(LOCAL)/share/man/man1/$(STEGFS).1.gz $(PREFIX)/usr/$(LOCAL)/share/man/man1/$(MKFS).1.gz
	-@echo -e "linked ‘$(PREFIX)/usr/$(LOCAL)/share/man/man1/$(MKFS).1.gz’ → ‘$(PREFIX)/usr/$(LOCAL)/share/man/man1/$(STEGFS).1.gz’"

uninstall:
	@rm -fv $(PREFIX)/usr/$(LOCAL)/share/man/man1/$(STEGFS).1.gz
	@rm -fv $(PREFIX)/usr/$(LOCAL)/share/man/man1/$(MKFS).1.gz
	@rm -fv $(PREFIX)/usr/bin/$(MKFS)
	@rm -fv $(PREFIX)/usr/bin/$(STEGFS)

clean:
	 @rm -fv $(STEGFS) $(MKFS)

distclean: clean
	@rm -fv $(STEGFS).1.gz
	@rm -fvr pkg build
	@rm -fv $(STEGFS)*.pkg.tar.xz
	@rm -fv $(STEGFS)*.tgz
