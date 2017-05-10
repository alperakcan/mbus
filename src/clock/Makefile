
target.a-y = \
	libmbus-clock.a

target.so-y = \
	libmbus-clock.so

libmbus-clock.so_includes-y = \
	../../dist/include

libmbus-clock.so_libraries-y = \
	../../dist/lib

libmbus-clock.so_files-y = \
	clock.c

libmbus-clock.a_includes-y = \
	${libmbus-clock.so_includes-y}

libmbus-clock.a_files-y = \
	${libmbus-clock.so_files-y}

dist.base = mbus

distdir = ../../dist

dist.include-y = \
	clock.h

dist.lib-y = \
	libmbus-clock.a \
	libmbus-clock.so

include ../../Makefile.lib
