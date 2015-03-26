
target.so-y = \
	libmbus-client.so

libmbus-client.so_includes-y = \
	../../dist/include

libmbus-client.so_libraries-y = \
	../../dist/lib

libmbus-client.so_files-y = \
	client.c

libmbus-client.so_ldflags-y = \
	-lmbus-debug \
	-lmbus-socket \
	-lmbus-cJSON \
	-lpthread

distdir = ../../dist

dist.include-y = \
	client.h

dist.lib-y = \
	libmbus-client.so

include ../../Makefile.lib
