.PHONY: clean distclean

vpath %.c src

object   = lib-stegfs.o dir.o
common	 = common/clib.a
fuseob   = fuse-stegfs.a
mkfsob   = mkstegfs.a
app      = stegfs
mkfs     = mkstegfs

CFLAGS   = -Wall -Wextra -Wno-unused-parameter -O2 -std=gnu99 -c -o # -ggdb
CPPFLAGS = -I. -Isrc -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 `pkg-config fuse --cflags`
LDFLAGS  = -r -o # -s
LIBS     = `pkg-config fuse --libs-only-l` -lmhash -lmcrypt -lpthread

all: $(fuseob) $(mkfsob) $(common) language
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

documentation:
	@doxygen

language:
	@$(MAKE) -C po

clean:
	@rm -fv $(object) fuse-stegfs.o mkfs.o
	@$(MAKE) -C common clean

distclean: clean
	@rm -f $(app) $(mkfs) $(fuseob) $(mkfsob) $(common)
	@$(MAKE) -C common distclean
	@$(MAKE) -C po distclean
	@rm -frv doc/{html,latex}
