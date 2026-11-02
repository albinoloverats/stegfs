.PHONY: stegfs test test-common test-stegfs coverage coverage-report clean distclean

STEGFS   = stegfs
MKFS     = mkstegfs
CP       = cp_tree
TEST_STEGFS = test-stegfs

SOURCE   = src/main.c src/stegfs.c
TEST_SOURCE = test/test_stegfs.c src/stegfs.c
MKSRC    = src/mkfs.c
CPSRC    = src/cp.c
COMMON   = common/src/error.c common/src/mem.c common/src/ccrypt.c common/src/tlv.c common/src/list.c common/src/dir.c common/src/cli.c common/src/version.c common/src/config.c
MISC     = common/misc.h

CFLAGS   += -Wall -Wextra -std=gnu99 $(shell pkg-config --cflags fuse libgcrypt) -pipe -O2 -I/usr/local/include -Isrc
CPPFLAGS += -Icommon/src -D_GNU_SOURCE -DGCRYPT_NO_DEPRECATED -DUSE_GCRYPT -D_FILE_OFFSET_BITS=64 -DGIT_COMMIT=\"`git log | head -n1 | cut -f2 -d' '`\" -DBUILD_OS=\"$(shell grep PRETTY_NAME /etc/os-release | cut -d= -f2)\"

DEBUG_CFLAGS   = -O0 -ggdb
DEBUG_CPPFLAGS = -D__DEBUG__ -DUSE_PROC
PROFILE        = ${DEBUG} -pg -lc

# -lpthread
LIBS     = -lpthread -lcurl $(shell pkg-config --libs fuse libgcrypt)
TEST_LIBS = $(shell pkg-config --libs cmocka) $(LIBS)

all: stegfs mkfs man

test:
	@${MAKE} test-common
	@${MAKE} test-stegfs

test-common:
	@${MAKE} -C common test

test-stegfs:
	@${CC} ${CFLAGS} ${CPPFLAGS} $(shell pkg-config --cflags cmocka) ${TEST_SOURCE} ${COMMON} ${TEST_LIBS} -o ${TEST_STEGFS}
	@./${TEST_STEGFS}

coverage: coverage-report

coverage-report:
	@${MAKE} -C common coverage
	@rm -f test-stegfs-*.gcda test-stegfs-*.gcno
	@${CC} ${CFLAGS} ${CPPFLAGS} $(shell pkg-config --cflags cmocka) --coverage ${TEST_SOURCE} ${COMMON} ${TEST_LIBS} -o ${TEST_STEGFS}
	@./${TEST_STEGFS}
	@rm -fvr coverage
	@mkdir -p coverage
	@gcovr --root . --gcov-executable "$$(gcc -print-prog-name=gcov)" --filter '^(src/|common/src/)' --xml-pretty --output coverage/coverage.xml --html-details coverage/index.html --print-summary

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
	 @${CC} -lcurl -lpthread ${CFLAGS} ${CPPFLAGS} -O0 -ggdb ${CPSRC} common/src/error.c common/src/cli.c common/src/config.c common/src/version.c common/src/fs.c -o ${CP}
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
	 @rm -fv ${STEGFS} ${MKFS} ${CP} ${TEST_STEGFS}
	 @rm -fv ${MISC}
	 @rm -fvr coverage
	 @rm -fv test-stegfs-*.gcda test-stegfs-*.gcno
	 @${MAKE} -C common clean

distclean: clean
	@rm -fv ${STEGFS}.1.gz
	@rm -fv ${MKFS}.1.gz
	@rm -fvr pkg build
	@rm -fv ${STEGFS}*.pkg.tar.xz
	@rm -fv ${STEGFS}*.tgz
	@rm -fv ${MISC}
