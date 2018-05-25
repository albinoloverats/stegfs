.PHONY: stegfs clean distclean

STEGFS   = stegfs
MKFS     = mkstegfs
CP       = cp_tree

SOURCE   = src/main.c src/stegfs.c src/init.c
MKSRC    = src/mkfs.c src/init.c
CPSRC    = src/cp.c
COMMON   = src/common/error.c src/common/ccrypt.c src/common/tlv.c src/common/dir.c src/common/non-gnu.c

CFLAGS   = -Wall -Wextra -Werror -std=gnu99 `pkg-config --cflags fuse` -pipe -I/usr/local/include
CPPFLAGS = -Isrc -D_GNU_SOURCE -DGCRYPT_NO_DEPRECATED -D_FILE_OFFSET_BITS=64 -DGIT_COMMIT=\"`git log | head -n1 | cut -f2 -d' '`\"

PROFILE  = -O0 -ggdb -D__DEBUG__ -pg -lc
DEBUG    = -O0 -ggdb -D__DEBUG__

# -lpthread
LIBS     = -lgcrypt `pkg-config --libs fuse`

all: stegfs mkfs man

stegfs:
	 @$(CC) $(LIBS) $(CFLAGS) $(CPPFLAGS) -O2 $(SOURCE) $(COMMON) -o $(STEGFS)
	-@echo "built ‘$(SOURCE)’ → ‘$(STEGFS)’"

mkfs:
	 @$(CC) $(LIBS) $(CFLAGS) $(CPPFLAGS) -O2 $(MKSRC) $(COMMON) -o $(MKFS)
	-@echo "built ‘$(MKSRC) $(COMMON)’ → ‘$(MKFS)’"

cp:
	 @$(CC) $(CFLAGS) $(CPPFLAGS) -O0 -ggdb $(CPSRC) src/common/error.c src/common/fs.c -o $(CP)
	-@echo "built ‘$(CPSRC) $(COMMON)’ → ‘$(CP)’"

debug: debug-stegfs debug-mkfs

debug-stegfs:
	 @$(CC) $(LIBS) $(CFLAGS) $(CPPFLAGS) $(DEBUG) -DUSE_PROC $(SOURCE) $(COMMON) -o $(STEGFS)
	-@echo "built ‘$(SOURCE)’ → ‘$(STEGFS)’"

debug-mkfs:
	 @$(CC) $(LIBS) $(CFLAGS) $(CPPFLAGS) $(DEBUG) $(MKSRC) $(COMMON) -o $(MKFS)
	-@echo "built ‘$(MKSRC) $(COMMON)’ → ‘$(MKFS)’"

man:
	 @gzip -c docs/$(STEGFS).1 > $(STEGFS).1.gz
	-@echo -e "compressing ‘docs/$(STEGFS).1’ → ‘$(STEGFS).1.gz"
	 @gzip -c docs/$(MKFS).1 > $(MKFS).1.gz
	-@echo -e "compressing ‘docs/$(MKFS).1’ → ‘$(MKFS).1.gz"

install:
# install stegfs and mkstegfs
	 @install -c -m 755 -s -D -T $(STEGFS) $(PREFIX)/usr/bin/$(STEGFS)
	-@echo -e "installed ‘$(STEGFS)’ → ‘$(PREFIX)/usr/bin/$(STEGFS)’"
	 @install -c -m 755 -s -D -T $(MKFS) $(PREFIX)/usr/bin/$(MKFS)
	-@echo -e "installed ‘$(MKFS)’ → ‘$(PREFIX)/usr/bin/$(MKFS)’"
# now the man page(s)
	 @install -c -m 644 -D -T $(STEGFS).1.gz $(PREFIX)/usr/$(LOCAL)/share/man/man1/$(STEGFS).1.gz
	-@echo -e "installed ‘$(STEGFS).1.gz’ → ‘$(PREFIX)/usr/$(LOCAL)/share/man/man1/$(STEGFS).1.gz’"
	 @install -c -m 644 -D -T $(MKFS).1.gz $(PREFIX)/usr/$(LOCAL)/share/man/man1/$(MKFS).1.gz
	-@echo -e "installed ‘$(MKFS).1.gz’ → ‘$(PREFIX)/usr/$(LOCAL)/share/man/man1/$(MKFS).1.gz’"

uninstall:
	@rm -fv $(PREFIX)/usr/$(LOCAL)/share/man/man1/$(STEGFS).1.gz
	@rm -fv $(PREFIX)/usr/$(LOCAL)/share/man/man1/$(MKFS).1.gz
	@rm -fv $(PREFIX)/usr/bin/$(MKFS)
	@rm -fv $(PREFIX)/usr/bin/$(STEGFS)

clean:
	 @rm -fv $(STEGFS) $(MKFS) $(CP)

distclean: clean
	@rm -fv $(STEGFS).1.gz
	@rm -fv $(MKFS).1.gz
	@rm -fvr pkg build
	@rm -fv $(STEGFS)*.pkg.tar.xz
	@rm -fv $(STEGFS)*.tgz
