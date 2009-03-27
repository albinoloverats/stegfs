.PHONY: vstegfs mkfs all install clean distclean uninstall

OPTIONS := `pkg-config fuse --cflags --libs` -lmhash -lmcrypt -std=gnu99 -O2 -Wall -pipe -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE -I ./ -o
COMMON  := src/vstegfs.c src/dir.c common/common.c

all: vstegfs mkfs

vstegfs:
# build the main fuse executable
	 @gcc $(OPTIONS) vstegfs $(COMMON) src/fuse.c
	-@echo "compiled \`src/fuse.c srv/vstegfs.c src/dir.c common/common.c' --> \`vstegfs'"

mkfs:
# build the mkfs utility
	 @gcc $(OPTIONS) mkvstegfs $(COMMON) src/mkfs.c
	-@echo "compiled \`src/mkfs.c src/vstegfs.c src/dir.c common/common.c' --> \`mkvstegfs'"

install:
# install vstegfs
	 @install -c -m 755 -s -D -T vstegfs $(PREFIX)/usr/bin/vstegfs
	-@echo "installed \`vstegfs' --> \`$(PREFIX)/usr/bin/vstegfs'"
# install the mkfs
	 @install -c -m 755 -s -D -T mkvstegfs $(PREFIX)/usr/bin/mkvstegfs
	-@echo "installed \`mkvstegfs' --> \`$(PREFIX)/usr/bin/vstegfs'"
# finally the man page
	 @install -c -m 644 -D -T doc/vstegfs.1.gz $(PREFIX)/usr/man/man1/vstegfs.1.gz
	-@echo "installed \`doc/vstegfs.1.gz' --> \`$(PREFIX)/usr/man/man1/vstegfs.1.gz'"

clean:
	-@rm -fv vstegfs mkvstegfs

distclean: clean

uninstall:
	 @rm -fv $(PREFIX)/usr/man/man1/vstegfs.1.gz
	 @rm -fv $(PREFIX)/usr/bin/mkvstegfs
	 @rm -fv $(PREFIX)/usr/bin/vstegfs

