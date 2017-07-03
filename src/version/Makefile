
GIT_COMMIT := $(shell git log -1 --format="%ci")
GIT_REVISION := $(shell git describe --long --dirty --always --tags)

target.a-y = \
	libmbus-version.a

target.so-y = \
	libmbus-version.so

libmbus-version.so_files-y = \
	version.c

libmbus-version.so_cflags-y = \
	-DGIT_COMMIT="\"$(GIT_COMMIT)\"" \
	-DGIT_REVISION="\"$(GIT_REVISION)\""

libmbus-version.a_files-y = \
	${libmbus-version.so_files-y}

libmbus-version.a_cflags-y = \
	${libmbus-version.so_cflags-y}

distdir = ../../dist

dist.base = mbus

dist.include-y = \
	version.h

dist.lib-y = \
	libmbus-version.a \
	libmbus-version.so

include ../../Makefile.lib
