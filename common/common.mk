.PHONY: common clean distclean

COMMON_PO_MAKE      = 
COMMON_PO_INSTALL   = 
COMMON_PO_CLEAN     = 
COMMON_PO_UNINSTALL = 

OPTIONS := -std=gnu99 -O2 -Wall -Wextra -pipe -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE -I ./ -c

all: common list
-include po/*.mk

common: $(COMMON_PO_MAKE)
# build the main fuse executable
	 @gcc $(OPTIONS) common.c
	-@echo "compiled \`common.c' --> \`common.o'"

list:
	 @gcc $(OPTIONS) list.c
	-@echo "compiled \`list.c' -> \`list.o'"

clean: $(COMMON_PO_CLEAN)
	-@/bin/rm -fv common.o
	-@/bin/rm -fv list.o

distclean: clean
