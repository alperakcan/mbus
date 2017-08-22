
include ../../Makefile.conf

target.a-y = \
	libmbus-compress.a

target.so-${SHARED_ENABLE} = \
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
	${zlib_cflags-y}

libmbus-compress.so_ldflags-${ZLIB_ENABLE} += \
	${zlib_ldflags-y}

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
	libmbus-compress.a

dist.lib-${SHARED_ENABLE} += \
	libmbus-compress.so

include ../../Makefile.lib
