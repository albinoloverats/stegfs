.PHONY: clean distclean

STEGFS   = stegfs
MKFS	 = mkstegfs
COPY	 = cpstegfs

SOURCE   = src/main.c src/dir.c src/stegfs.c src/help.c
MKSRC    = src/mkfs.c
CPSRC	 = src/copy.c src/common/fs.c src/common/error.c
COMMON   = src/common/error.c src/common/tlv.c src/common/apple.c

CFLAGS   = -Wall -Wextra -Werror -Wno-unused-parameter -std=gnu99 `pkg-config --cflags fuse` -pipe -O0 -ggdb -I/usr/local/include
CPPFLAGS = -Isrc -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -DGIT_COMMIT=\"`git log | head -n1 | cut -f2 -d' '`\"

LIBS     = -lmhash -lmcrypt -lpthread `pkg-config --libs fuse`

all: stegfs mkfs copy man

stegfs:
	 @$(CC) $(LIBS) $(CFLAGS) $(CPPFLAGS) $(SOURCE) $(COMMON) -o $(STEGFS)
	-@echo "built ‘$(SOURCE)’ --> ‘$(STEGFS)’"

mkfs:
	 @$(CC) $(LIBS) $(CFLAGS) $(CPPFLAGS) $(MKSRC) $(COMMON) -o $(MKFS)
	-@echo "built ‘$(MKSRC) $(COMMON)’ --> ‘$(MKFS)’"

copy:
	 @$(CC) $(LIBS) $(CFLAGS) $(CPPFLAGS) $(CPSRC) -o $(COPY)
	-@echo "built ‘$(CPSRC)’ --> ‘$(COPY)’"

man:
	 @gzip -c docs/$(STEGFS).1 > $(STEGFS).1.gz
	-@echo -e "compressing ‘docs/$(STEGFS).1’ → ‘$(STEGFS).1.gz"

install:
# install stegfs and mkstegfs
	 @install -c -m 755 -s -D -T $(STEGFS) $(PREFIX)/usr/bin/$(STEGFS)
	-@echo -e "installed ‘$(STEGFS)’ → ‘$(PREFIX)/usr/bin/$(STEGFS)’"
	 @install -c -m 755 -s -D -T $(MKFS) $(PREFIX)/usr/bin/$(MKFS)
	-@echo -e "installed ‘$(MKFS)’ → ‘$(PREFIX)/usr/bin/$(MKFS)’"
	 @install -c -m 755 -s -D -T $(COPY) $(PREFIX)/usr/bin/$(COPY)
	-@echo -e "installed ‘$(COPY)’ → ‘$(PREFIX)/usr/bin/$(COPY)’"
# now the man page(s)
	 @install -c -m 644 -D -T $(STEGFS).1.gz $(PREFIX)/usr/$(LOCAL)/share/man/man1/$(STEGFS).1.gz
	-@echo -e "installed ‘$(STEGFS).1.gz’ → ‘$(PREFIX)/usr/$(LOCAL)/share/man/man1/$(STEGFS).1.gz’"
	 @ln -f $(PREFIX)/usr/$(LOCAL)/share/man/man1/$(STEGFS).1.gz $(PREFIX)/usr/$(LOCAL)/share/man/man1/$(MKFS).1.gz
	-@echo -e "linked ‘$(PREFIX)/usr/$(LOCAL)/share/man/man1/$(MKFS).1.gz’ → ‘$(PREFIX)/usr/$(LOCAL)/share/man/man1/$(STEGFS).1.gz’"

uninstall:
	@rm -fv $(PREFIX)/usr/$(LOCAL)/share/man/man1/$(STEGFS).1.gz
	@rm -fv $(PREFIX)/usr/$(LOCAL)/share/man/man1/$(MKFS).1.gz
	@rm -fv $(PREFIX)/usr/bin/$(COPY)
	@rm -fv $(PREFIX)/usr/bin/$(MKFS)
	@rm -fv $(PREFIX)/usr/bin/$(STEGFS)

clean:
	 @rm -fv $(STEGFS) $(MKFS) $(COPY)

distclean: clean
	@rm -fv $(STEGFS).1.gz
	@rm -fvr pkg build
	@rm -fv $(STEGFS)*.pkg.tar.xz
	@rm -fv $(STEGFS)*.tgz
