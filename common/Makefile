.PHONY: clean distclean

DOXYGEN  = doxygen

object   = common.o logging.o list.o tlv.o
module   = clib.a

CFLAGS   = -Wall -Wextra -O0 -std=gnu99 -pipe -c -o
CPPFLAGS = -I. -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64
LDFLAGS  = -r -o

all: $(module) language

$(module): $(object)
	@$(LD) $(LDFLAGS) $(module) $(object)
	@echo "linked \`$(object)' --> \`$(module)'"

%.o: %.c
	@$(CC) $(CPPFLAGS) $(CFLAGS) $@ $<
	@echo "compiled \`$<' --> \`$@'"

documentation:
	@$(DOXYGEN)
	@$(MAKE) -C doc/latex

language:
	@$(MAKE) -C po

clean:
	@rm -fv $(object)

distclean: clean
	@rm -fv $(module)
	@$(MAKE) -C po distclean
	@rm -frv doc/{html,latex}
