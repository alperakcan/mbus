
target.a-y = \
	libmbus-server.a

target.so-y = \
	libmbus-server.so

libmbus-server.so_includes-y = \
	../../dist/include

libmbus-server.so_libraries-y = \
	../../dist/lib

libmbus-server.so_files-y = \
	server.c

libmbus-server.so_ldflags-y = \
	-lmbus-debug \
	-lmbus-socket \
	-lmbus-json

libmbus-server.so_ldflags-y += \
	-lwebsockets

libmbus-server.a_files-y = \
	${libmbus-server.so_files-y}

libmbus-server.a_includes-y = \
	${libmbus-server.so_includes-y}

distdir = ../../dist

dist.base = mbus

dist.include-y = \
	server.h

dist.lib-y = \
	libmbus-server.a \
	libmbus-server.so

include ../../Makefile.lib
