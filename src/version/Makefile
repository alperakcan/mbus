
GIT_COMMIT := $(shell git log -1 --format="%ci")
GIT_VERSION := $(shell git describe --long --dirty --always --tags)

target.a-y = \
	libmbus-version.a

target.so-y = \
	libmbus-version.so

libmbus-version.so_includes-y = \
	../../dist/include

libmbus-version.so_libraries-y = \
	../../dist/lib

libmbus-version.so_files-y = \
	version.c

libmbus-version.so_cflags-y = \
	-DGIT_COMMIT="\"$(GIT_COMMIT)\"" \
	-DGIT_VERSION="\"$(GIT_VERSION)\""

libmbus-version.so_ldflags-y = \
	-lmbus-debug

libmbus-version.a_includes-y = \
	${libmbus-version.so_includes-y}

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
