
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

distdir = ../../dist

dist.base = mbus

dist.include-y = \
	server.h

dist.lib-y = \
	libmbus-server.so

include ../../Makefile.lib
