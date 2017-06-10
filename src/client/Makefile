
target.a-y = \
	libmbus-client.a

target.so-y = \
	libmbus-client.so

libmbus-client.so_includes-y = \
	../../dist/include

libmbus-client.so_libraries-y = \
	../../dist/lib

libmbus-client.so_files-y = \
	client.c

libmbus-client.so_cflags-y += \
	$(shell pkg-config --cflags libssl)

libmbus-client.so_ldflags-y = \
	$(shell pkg-config --libs libssl) \
	$(shell pkg-config --libs libcrypto) \
	-lmbus-debug \
	-lmbus-clock \
	-lmbus-buffer \
	-lmbus-socket \
	-lmbus-json \
	-lpthread

libmbus-client.a_includes-y = \
	${libmbus-client.so_includes-y}

libmbus-client.a_cflags-y = \
	${libmbus-client.so_cflags-y}

libmbus-client.a_files-y = \
	${libmbus-client.so_files-y}

distdir = ../../dist

dist.base = mbus

dist.include-y = \
	client.h

dist.lib-y = \
	libmbus-client.a \
	libmbus-client.so

include ../../Makefile.lib
