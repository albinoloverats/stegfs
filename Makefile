.PHONY: stegfs clean distclean

STEGFS   = stegfs
MKFS     = mkstegfs
CP       = cp_tree

SOURCE   = src/main.c src/stegfs.c
MKSRC    = src/mkfs.c
CPSRC    = src/cp.c
COMMON   = src/common/error.c src/common/ccrypt.c src/common/tlv.c src/common/list.c src/common/dir.c src/common/cli.c src/common/version.c src/common/config.c
MISC     = src/common/misc.h

CFLAGS   += -Wall -Wextra -std=gnu99 $(shell pkg-config --cflags fuse) -pipe -O2 -I/usr/local/include -Isrc
CPPFLAGS += -D_GNU_SOURCE -DGCRYPT_NO_DEPRECATED -D_FILE_OFFSET_BITS=64 -DGIT_COMMIT=\"`git log | head -n1 | cut -f2 -d' '`\" -DBUILD_OS=\"$(shell grep PRETTY_NAME /etc/os-release | cut -d= -f2)\"

DEBUG_CFLAGS   = -O0 -ggdb
DEBUG_CPPFLAGS = -D__DEBUG__ -DUSE_PROC
PROFILE        = ${DEBUG} -pg -lc

# -lpthread
LIBS     = $(shell libgcrypt-config --libs) -lpthread -lcurl $(shell pkg-config --libs fuse)

all: stegfs mkfs man

stegfs:
	 @echo "#define ALL_CFLAGS   \"$(strip $(subst \",\',"${CFLAGS}"))\""    > ${MISC}
	 @echo "#define ALL_CPPFLAGS \"$(strip $(subst \",\',"${CPPFLAGS}"))\"" >> ${MISC}
	 @${CC} ${LIBS} ${CFLAGS} ${CPPFLAGS} ${SOURCE} ${COMMON} -o ${STEGFS}
	-@echo "built ‘${SOURCE}’ → ‘${STEGFS}’"

mkfs:
	 @echo "#define ALL_CFLAGS   \"$(strip $(subst \",\',"${CFLAGS}"))\""    > ${MISC}
	 @echo "#define ALL_CPPFLAGS \"$(strip $(subst \",\',"${CPPFLAGS}"))\"" >> ${MISC}
	 @${CC} ${LIBS} ${CFLAGS} ${CPPFLAGS} ${MKSRC} ${COMMON} -o ${MKFS}
	-@echo "built ‘${MKSRC} ${COMMON}’ → ‘${MKFS}’"

cp:
	 @echo "#define ALL_CFLAGS   \"$(strip $(subst \",\',"${CFLAGS}"))\""    > ${MISC}
	 @echo "#define ALL_CPPFLAGS \"$(strip $(subst \",\',"${CPPFLAGS}"))\"" >> ${MISC}
	 @${CC} -lcurl -lpthread ${CFLAGS} ${CPPFLAGS} -O0 -ggdb ${CPSRC} src/common/error.c src/common/cli.c src/common/config.c src/common/version.c src/common/fs.c -o ${CP}
	-@echo "built ‘${CPSRC} ${COMMON}’ → ‘${CP}’"

debug: debug-stegfs debug-mkfs

debug-stegfs:
	 @echo "#define ALL_CFLAGS   \"$(strip $(subst \",\',"${CFLAGS}   ${DEBUG_CFLAGS}"))\""    > ${MISC}
	 @echo "#define ALL_CPPFLAGS \"$(strip $(subst \",\',"${CPPFLAGS} ${DEBUG_CPPFLAGS}"))\"" >> ${MISC}
	 @${CC} ${LIBS} ${CFLAGS} ${CPPFLAGS} ${DEBUG_CFLAGS} ${DEBUG_CPPFLAGS} ${SOURCE} ${COMMON} -o ${STEGFS}
	-@echo "built ‘${SOURCE}’ → ‘${STEGFS}’"

debug-mkfs:
	 @echo "#define ALL_CFLAGS   \"$(strip $(subst \",\',"${CFLAGS}   ${DEBUG_CFLAGS}"))\""    > ${MISC}
	 @echo "#define ALL_CPPFLAGS \"$(strip $(subst \",\',"${CPPFLAGS} ${DEBUG_CPPFLAGS}"))\"" >> ${MISC}
	 @${CC} ${LIBS} ${CFLAGS} ${CPPFLAGS} ${DEBUG_CFLAGS} ${DEBUG_CPPFLAGS} ${MKSRC} ${COMMON} -o ${MKFS}
	-@echo "built ‘${MKSRC} ${COMMON}’ → ‘${MKFS}’"

man:
	 @gzip -c docs/${STEGFS}.1 > ${STEGFS}.1.gz
	-@echo -e "compressing ‘docs/${STEGFS}.1’ → ‘${STEGFS}.1.gz"
	 @gzip -c docs/${MKFS}.1 > ${MKFS}.1.gz
	-@echo -e "compressing ‘docs/${MKFS}.1’ → ‘${MKFS}.1.gz"

install:
# install stegfs and mkstegfs
	 @install -c -m 755 -s -D -T ${STEGFS} ${PREFIX}/usr/bin/${STEGFS}
	-@echo -e "installed ‘${STEGFS}’ → ‘${PREFIX}/usr/bin/${STEGFS}’"
	 @install -c -m 755 -s -D -T ${MKFS} ${PREFIX}/usr/bin/${MKFS}
	-@echo -e "installed ‘${MKFS}’ → ‘${PREFIX}/usr/bin/${MKFS}’"
# now the man page{s}
	 @install -c -m 644 -D -T ${STEGFS}.1.gz ${PREFIX}/usr/${LOCAL}/share/man/man1/${STEGFS}.1.gz
	-@echo -e "installed ‘${STEGFS}.1.gz’ → ‘${PREFIX}/usr/${LOCAL}/share/man/man1/${STEGFS}.1.gz’"
	 @install -c -m 644 -D -T ${MKFS}.1.gz ${PREFIX}/usr/${LOCAL}/share/man/man1/${MKFS}.1.gz
	-@echo -e "installed ‘${MKFS}.1.gz’ → ‘${PREFIX}/usr/${LOCAL}/share/man/man1/${MKFS}.1.gz’"

uninstall:
	@rm -fv ${PREFIX}/usr/${LOCAL}/share/man/man1/${STEGFS}.1.gz
	@rm -fv ${PREFIX}/usr/${LOCAL}/share/man/man1/${MKFS}.1.gz
	@rm -fv ${PREFIX}/usr/bin/${MKFS}
	@rm -fv ${PREFIX}/usr/bin/${STEGFS}

clean:
	 @rm -fv ${STEGFS} ${MKFS} ${CP}

distclean: clean
	@rm -fv ${STEGFS}.1.gz
	@rm -fv ${MKFS}.1.gz
	@rm -fvr pkg build
	@rm -fv ${STEGFS}*.pkg.tar.xz
	@rm -fv ${STEGFS}*.tgz
