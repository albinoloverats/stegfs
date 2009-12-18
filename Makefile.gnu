.PHONY: vstegfs mkfs testfs all install clean distclean uninstall

PO_MAKE      = 
PO_INSTALL   = 
PO_CLEAN     = 
PO_UNINSTALL = 

OPTIONS := `pkg-config fuse --cflags --libs` -lmhash -lmcrypt -lpthread -std=gnu99 -O2 -Wall -Wextra -Wno-unused-parameter -pipe -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE -I ./ -o
COMMON  := src/vstegfs.c src/dir.c common/common.c

all: a
-include po/*.mk

vstegfs:
# build the main fuse executable
	 @gcc $(OPTIONS) vstegfs $(COMMON) src/fuse.c
	-@echo "compiled \`src/fuse.c $(COMMON)' --> \`vstegfs'"

mkfs:
# build the mkfs utility
	 @gcc $(OPTIONS) mkvstegfs $(COMMON) src/mkfs.c
	-@echo "compiled \`src/mkfs.c $(COMMON)' --> \`mkvstegfs'"

test:
# build the testfs utility
	 @gcc -lncurses $(OPTIONS) testvstegfs $(COMMON) src/testfs.c
	-@echo "compiled \`src/testfs.c $(COMMON)' --> \`testvstegfs'"

a: vstegfs mkfs $(PO_MAKE)

install: $(PO_INSTALL)
# install vstegfs
	 @install -c -m 755 -s -D -T vstegfs $(PREFIX)/usr/bin/vstegfs
	-@echo "installed \`vstegfs' --> \`$(PREFIX)/usr/bin/vstegfs'"
# install the mkfs
	 @install -c -m 755 -s -D -T mkvstegfs $(PREFIX)/usr/bin/mkvstegfs
	-@echo "installed \`mkvstegfs' --> \`$(PREFIX)/usr/bin/vstegfs'"
# finally the man page
	 @install -c -m 644 -D -T doc/vstegfs.1.gz $(PREFIX)/usr/man/man1/vstegfs.1.gz
	-@echo "installed \`doc/vstegfs.1.gz' --> \`$(PREFIX)/usr/man/man1/vstegfs.1.gz'"

clean: $(PO_CLEAN)
	-@/bin/rm -fv vstegfs mkvstegfs testvstegfs

distclean: clean

uninstall: $(PO_UNINSTALL)
	 @/bin/rm -fv $(PREFIX)/usr/man/man1/vstegfs.1.gz
	 @/bin/rm -fv $(PREFIX)/usr/bin/mkvstegfs
	 @/bin/rm -fv $(PREFIX)/usr/bin/vstegfs
