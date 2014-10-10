.PHONY: clean distclean

FS_APP   = stegfs
MKFS_APP = mkstegfs

SOURCE   = src/dir.c src/lib-stegfs.c
COMMON   = src/common/error.c src/common/logging.c src/common/tlv.c

CFLAGS   = -Wall -Wextra -Werror -Wno-unused-parameter -std=gnu99 `libgcrypt-config --cflags` -pipe -O0 -ggdb
CPPFLAGS = -Isrc -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -DLOG_DEFAULT=LOG_ERROR -D__DEBUG__

LIBS     = `pkg-config fuse --libs-only-l` -lmhash -lmcrypt -lpthread

all: stegfs mkstegfs

stegfs:
	@$(CC) $(LIBS) $(CFLAGS) $(CPPFLAGS) $(SOURCE) src/fuse-stegfs.c $(COMMON) -o $(FS_APP)
	-@echo "built \`$(SOURCE) $(COMMON)' --> \`$(FS_APP)'"

mkstegfs:
	@$(CC) $(LIBS) $(CFLAGS) $(CPPFLAGS) $(SOURCE) src/mkfs.c $(COMMON) -o $(MKFS_APP)
	-@echo "built \`$(SOURCE) $(COMMON)' --> \`$(MKFS_APP)'"

#man:
#	@gzip -c doc/stegfs.1 > doc/stegfs.1.gz
#
#documentation:
#	@doxygen
#
#language:
#	@$(MAKE) -C po
#
#install:
#	 @install -c -m 755 -s -D -T stegfs $(PREFIX)/usr/bin/stegfs
#	-@echo "installed \`stegfs' --> \`$(PREFIX)/usr/bin/stegfs'"
#	 @install -c -m 755 -s -D -T mkstegfs $(PREFIX)/usr/bin/mkstegfs
#	-@echo "installed \`mkstegfs' --> \`$(PREFIX)/usr/bin/stegfs'"
#	 @install -c -m 644 -D -T doc/stegfs.1.gz $(PREFIX)/usr/share/man/man1/stegfs.1.gz
#	-@echo "installed \`doc/stegfs.1.gz' --> \`$(PREFIX)/usr/share/man/man1/stegfs.1.gz'"
#
#uninstall:
#	-@echo "TODO!!!"

clean:
	@rm -fv stegfs mkstegfsfs

distclean: clean
