
WEBSOCKET_ENABLE ?= y

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
	-lmbus-buffer \
	-lmbus-clock \
	-lmbus-socket \
	-lmbus-json

libmbus-server.so_cflags-y += \
	$(shell pkg-config --cflags libssl)

libmbus-server.so_ldflags-y += \
	$(shell pkg-config --libs libssl) \
	$(shell pkg-config --libs libcrypto) \

libmbus-server.so_cflags-${WEBSOCKET_ENABLE} += \
	-DWEBSOCKET_ENABLE=1 \
	$(shell pkg-config --cflags libwebsockets)

libmbus-server.so_ldflags-${WEBSOCKET_ENABLE} += \
	$(shell pkg-config --libs libwebsockets)

libmbus-server.a_files-y = \
	${libmbus-server.so_files-y}

libmbus-server.a_includes-y = \
	${libmbus-server.so_includes-y}

libmbus-server.a_cflags-y += \
	${libmbus-server.so_cflags-y}

distdir = ../../dist

dist.base = mbus

dist.include-y = \
	server.h

dist.lib-y = \
	libmbus-server.a \
	libmbus-server.so

include ../../Makefile.lib
