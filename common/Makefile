.PHONY: clean distclean

object   = common.o list.o
module   = clib.a

CFLAGS   = -Wall -Wextra -O2 -std=gnu99 -pipe -c -o # -ggdb
CPPFLAGS = -I. -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64
LDFLAGS  = -r -o # -s

all: $(module) language

$(module): $(object)
	@$(LD) $(LDFLAGS) $(module) $(object)
	@echo "linked \`$(object)' --> \`$(module)'"

%.o: %.c
	@$(CC) $(CPPFLAGS) $(CFLAGS) $@ $<
	@echo "compiled \`$<' --> \`$@'"

language:
	@$(MAKE) -C po

clean:
	@rm -fv $(object)

distclean: clean
	@rm -fv $(module)
	@$(MAKE) -C po distclean
