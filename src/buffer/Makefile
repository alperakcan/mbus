
include ../../Makefile.conf

target.a-y = \
	libmbus-buffer.a

target.so-${SHARED_ENABLE} = \
	libmbus-buffer.so

libmbus-buffer.so_includes-y = \
	../../dist/include

libmbus-buffer.so_libraries-y = \
	../../dist/lib

libmbus-buffer.so_files-y = \
	buffer.c

libmbus-buffer.so_ldflags-y = \
	-lmbus-debug \
	-lmbus-compress

libmbus-buffer.a_includes-y = \
	${libmbus-buffer.so_includes-y}

libmbus-buffer.a_files-y = \
	${libmbus-buffer.so_files-y}

dist.dir = ../../dist

dist.base = mbus

dist.include-y = \
	buffer.h

dist.lib-y = \
	libmbus-buffer.a

dist.lib-${SHARED_ENABLE} += \
	libmbus-buffer.so

include ../../Makefile.lib
