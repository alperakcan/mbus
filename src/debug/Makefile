
target.a-y = \
	libmbus-debug.a

target.so-y = \
	libmbus-debug.so

libmbus-debug.so_files-y = \
	debug.c

libmbus-debug.a_files-y = \
	${libmbus-debug.so_files-y}

distdir = ../../dist

dist.base = mbus

dist.include-y = \
	debug.h

dist.lib-y = \
	libmbus-debug.a \
	libmbus-debug.so

include ../../Makefile.lib
