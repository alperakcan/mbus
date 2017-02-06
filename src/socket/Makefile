
target.a-y = \
	libmbus-socket.a

target.so-y = \
	libmbus-socket.so

libmbus-socket.so_includes-y = \
	../../dist/include

libmbus-socket.so_libraries-y = \
	../../dist/lib

libmbus-socket.so_files-y = \
	socket.c

libmbus-socket.so_ldflags-y = \
	-lmbus-debug

libmbus-socket.a_includes-y = \
	${libmbus-socket.so_includes-y}

libmbus-socket.a_files-y = \
	${libmbus-socket.so_files-y}

distdir = ../../dist

dist.base = mbus

dist.include-y = \
	socket.h

dist.lib-y = \
	libmbus-socket.a \
	libmbus-socket.so

include ../../Makefile.lib
