.PHONY: stegfs mkfs testfs most all install clean distclean uninstall

PO_MAKE      = 
PO_INSTALL   = 
PO_CLEAN     = 
PO_UNINSTALL = 

OPTIONS := -DDEBUGGING `pkg-config fuse --cflags --libs` -lmhash -lmcrypt -lpthread -std=gnu99 -O2 -Wall -Wextra -Wno-unused-parameter -pipe -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE -I ./ -o
COMMON  := src/lib-stegfs.c src/dir.c common/common.c common/list.c

most: a
-include po/*.mk

stegfs:
# build the main fuse executable
	 @gcc $(OPTIONS) stegfs $(COMMON) src/fuse-stegfs.c
	-@echo "compiled \`src/fuse-stegfs.c $(COMMON)' --> \`stegfs'"

mkfs:
# build the mkfs utility
	 @gcc $(OPTIONS) mkstegfs $(COMMON) src/mkfs.c
	-@echo "compiled \`src/mkfs.c $(COMMON)' --> \`mkstegfs'"

testfs:
# build the testfs utility
	 @gcc -lncurses $(OPTIONS) teststegfs $(COMMON) src/testfs.c
	-@echo "compiled \`src/testfs.c $(COMMON)' --> \`teststegfs'"

a: stegfs mkfs $(PO_MAKE)

all: a testfs

install: $(PO_INSTALL)
# install stegfs
	 @install -c -m 755 -s -D -T stegfs $(PREFIX)/usr/bin/stegfs
	-@echo "installed \`stegfs' --> \`$(PREFIX)/usr/bin/stegfs'"
# install the mkfs
	 @install -c -m 755 -s -D -T mkstegfs $(PREFIX)/usr/bin/mkstegfs
	-@echo "installed \`mkstegfs' --> \`$(PREFIX)/usr/bin/stegfs'"
# finally the man page
	 @install -c -m 644 -D -T doc/stegfs.1.gz $(PREFIX)/usr/man/man1/stegfs.1.gz
	-@echo "installed \`doc/stegfs.1.gz' --> \`$(PREFIX)/usr/man/man1/stegfs.1.gz'"

clean: $(PO_CLEAN)
	-@/bin/rm -fv stegfs mkstegfs

distclean: clean
	-@/bin/rm -fv teststegfs

uninstall: $(PO_UNINSTALL)
	 @/bin/rm -fv $(PREFIX)/usr/man/man1/stegfs.1.gz
	 @/bin/rm -fv $(PREFIX)/usr/bin/mkstegfs
	 @/bin/rm -fv $(PREFIX)/usr/bin/stegfs
