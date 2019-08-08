
include ../../Makefile.conf

target.a-y = \
	libmbus-debug.a

target.so-${SHARED_ENABLE} = \
	libmbus-debug.so

libmbus-debug.so_files-y = \
	debug.c

libmbus-debug.a_files-y = \
	${libmbus-debug.so_files-y}

dist.dir = ../../dist

dist.base = mbus

dist.include-y = \
	debug.h

dist.lib-y = \
	libmbus-debug.a

dist.lib-${SHARED_ENABLE} += \
	libmbus-debug.so

include ../../Makefile.lib
