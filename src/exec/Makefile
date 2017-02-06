
target.a-y = \
	libmbus-exec.a

target.so-y = \
	libmbus-exec.so

libmbus-exec.so_includes-y = \
	../../dist/include

libmbus-exec.so_libraries-y = \
	../../dist/lib

libmbus-exec.so_files-y = \
	exec.c

libmbus-exec.so_ldflags-y = \
	-lmbus-debug

libmbus-exec.a_includes-y = \
	${libmbus-exec.so_includes-y}

libmbus-exec.a_files-y = \
	${libmbus-exec.so_files-y}

dist.base = mbus

distdir = ../../dist

dist.include-y = \
	exec.h

dist.lib-y = \
	libmbus-exec.a \
	libmbus-exec.so

include ../../Makefile.lib
