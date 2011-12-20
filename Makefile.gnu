.PHONY: clean distclean

vpath %.c src

object   = lib-stegfs.o dir.o
common	 = common/clib.a
fuseob   = fuse-stegfs.a
mkfsob   = mkstegfs.a
app      = stegfs
mkfs     = mkstegfs

CFLAGS   = -Wall -Wextra -Wno-unused-parameter -O2 -std=gnu99 -c -o # -ggdb
CPPFLAGS = -I. -Isrc -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 `pkg-config fuse --cflags` -DDEBUGGING
LDFLAGS  = -r -s -o
LIBS     = `pkg-config fuse --libs-only-l` -lmhash -lmcrypt -lpthread

all: $(fuseob) $(mkfsob) $(common) man language
	@$(CC) $(LIBS) -o $(app) $(fuseob) $(common)
	@echo "built \`$(fuseob) $(common)' --> \`$(app)'"
	@$(CC) $(LIBS) -o $(mkfs) $(mkfsob) $(common)
	@echo "built \`$(mkfsob) $(common)' --> \`$(mkfs)'"

$(fuseob): $(object) fuse-stegfs.o
	@$(LD) $(LDFLAGS) $(fuseob) $^
	@echo "linked \`$^' --> \`$(fuseob)'"

$(mkfsob): $(object) mkfs.o
	@$(LD) $(LDFLAGS) $(mkfsob) $^
	@echo "linked \`$^' --> \`$(mkfsob)'"

%.o: %.c
	@$(CC) $(CPPFLAGS) $(CFLAGS) $@ $<
	@echo "compiled \`$<' --> \`$@'"

$(common):
	@$(MAKE) -C common

man:
	@gzip -c doc/stegfs.1 > doc/stegfs.1.gz

documentation:
	@doxygen

language:
	@$(MAKE) -C po

install:
	 @install -c -m 755 -s -D -T stegfs $(PREFIX)/usr/bin/stegfs
	-@echo "installed \`stegfs' --> \`$(PREFIX)/usr/bin/stegfs'"
	 @install -c -m 755 -s -D -T mkstegfs $(PREFIX)/usr/bin/mkstegfs
	-@echo "installed \`mkstegfs' --> \`$(PREFIX)/usr/bin/stegfs'"
	 @install -c -m 644 -D -T doc/stegfs.1.gz $(PREFIX)/usr/share/man/man1/stegfs.1.gz
	-@echo "installed \`doc/stegfs.1.gz' --> \`$(PREFIX)/usr/share/man/man1/stegfs.1.gz'"

uninstall:
	-@echo "TODO!!!"

clean:
	@rm -fv $(object) fuse-stegfs.o mkfs.o
	@$(MAKE) -C common clean

distclean: clean
	@rm -f $(app) $(mkfs) $(fuseob) $(mkfsob) $(common)
	@$(MAKE) -C common distclean
	@$(MAKE) -C po distclean
	@rm -frv doc/{html,latex} doc/stegfs.1.gz
