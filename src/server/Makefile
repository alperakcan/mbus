
WS_ENABLE   ?= y
SSL_ENABLE  ?= y
ZLIB_ENABLE ?= y

target.a-y = \
	libmbus-server.a

target.so-y = \
	libmbus-server.so

libmbus-server.so_files-y = \
	server.c

libmbus-server.so_cflags-y += \
	-I../../dist/include

libmbus-server.so_ldflags-y += \
	-L../../dist/lib

libmbus-server.so_ldflags-y += \
	-lmbus-debug \
	-lmbus-buffer \
	-lmbus-clock \
	-lmbus-compress \
	-lmbus-socket \
	-lmbus-json

libmbus-server.so_cflags-${SSL_ENABLE} += \
	-DSSL_ENABLE=1 \
	$(shell pkg-config --cflags libssl)

libmbus-server.so_ldflags-${SSL_ENABLE} += \
	$(shell pkg-config --libs libssl) \
	$(shell pkg-config --libs libcrypto) \

libmbus-server.so_cflags-${WS_ENABLE} += \
	-DWS_ENABLE=1 \
	$(shell pkg-config --cflags libwebsockets)

libmbus-server.so_ldflags-${WS_ENABLE} += \
	$(shell pkg-config --libs libwebsockets)

libmbus-server.so_cflags-${ZLIB_ENABLE} += \
	-DZLIB_ENABLE=1

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
