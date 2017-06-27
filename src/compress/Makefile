
ZLIB_ENABLE ?= y

target.a-y = \
	libmbus-compress.a

target.so-y = \
	libmbus-compress.so

libmbus-compress.so_includes-y = \
	../../dist/include

libmbus-compress.so_libraries-y = \
	../../dist/lib

libmbus-compress.so_files-y = \
	compress.c

libmbus-compress.so_ldflags-y = \
	-lmbus-debug

libmbus-compress.so_cflags-${ZLIB_ENABLE} += \
	-DZLIB_ENABLE=1

libmbus-compress.so_ldflags-${ZLIB_ENABLE} += \
	-lz

libmbus-compress.a_includes-y += \
	${libmbus-compress.so_includes-y}

libmbus-compress.a_files-y += \
	${libmbus-compress.so_files-y}

libmbus-compress.a_cflags-y += \
	${libmbus-compress.so_cflags-y}

distdir = ../../dist

dist.base = mbus

dist.include-y = \
	compress.h

dist.lib-y = \
	libmbus-compress.a \
	libmbus-compress.so

include ../../Makefile.lib
