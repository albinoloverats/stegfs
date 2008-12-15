.PHONY: vstegfs mkfs all install clean distclean uninstall

REVISION := -D'REVISION="$(shell svnversion -n .)"'

COMPOPT := `pkg-config fuse --cflags --libs` -std=gnu99 -O2 -Wall -pipe -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE $(REVISION) -o

all: vstegfs mkfs

vstegfs:
# build the main fuse executable
	 @gcc $(COMPOPT) vstegfs src/fuse.c src/vstegfs.c src/dir.c src/tiger.c src/sboxes.c src/serpent.c
	-@echo "compiled \`src/fuse.c srv/vstegfs.c src/dir.c src/tiger.c src/sboxes.c src/serpent.c' --> \`vstegfs'"

mkfs:
# build the mkfs utility
	 @gcc $(COMPOPT) mkvstegfs src/mkfs.c src/vstegfs.c src/dir.c src/tiger.c src/sboxes.c src/serpent.c
	-@echo "compiled \`src/mkfs.c src/vstegfs.c src/dir.c /src/tiger.c src/sboxes.c src/serpent.c' --> \`mkvstegfs'"

install:
# install vstegfs
	 @install -c -m 755 -s -D -T vstegfs /usr/bin/vstegfs
	-@echo "installed \`vstegfs' --> \`/usr/bin/vstegfs'"
# install the mkfs
	 @install -c -m 755 -s -D -T mkvstegfs /usr/bin/mkvstegfs
	-@echo "installed \`mkvstegfs' --> \`/usr/bin/vstegfs'"
# finally the man page
	 @install -c -m 644 -D -T doc/vstegfs.1.gz /usr/share/man/man1/vstegfs.1.gz
	-@echo "installed \`doc/vstegfs.1.gz' --> \`/usr/share/man/man1/vstegfs.1.gz'"

clean:
	-@rm -fv vstegfs mkvstegfs

distclean: clean

uninstall:
	 @rm -fv /usr/share/man/man1/vstegfs.1.gz
	 @rm -fv /usr/bin/mkvstegfs
	 @rm -fv /usr/bin/vstegfs
