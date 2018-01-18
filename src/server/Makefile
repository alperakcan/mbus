
include ../../Makefile.conf

target.a-y = \
	libmbus-server.a

target.so-${SHARED_ENABLE} = \
	libmbus-server.so

libmbus-server.so_files-y = \
	command.c \
	subscription.c \
	method.c \
	listener.c \
	server.c

libmbus-server.so_cflags-y += \
	-fvisibility=hidden \
	-I../../dist/include \
	-Wno-unused-parameter

libmbus-server.so_ldflags-y += \
	-L../../dist/lib

libmbus-server.so_ldflags-y += \
	-lmbus-debug \
	-lmbus-buffer \
	-lmbus-clock \
	-lmbus-compress \
	-lmbus-socket \
	-lmbus-json \
	-lmbus-version

libmbus-server.so_cflags-${SSL_ENABLE} += \
	${ssl_cflags-y}

libmbus-server.so_ldflags-${SSL_ENABLE} += \
	${ssl_ldflags-y}

libmbus-server.so_cflags-${WS_ENABLE} += \
	${ws_cflags-y}

libmbus-server.so_ldflags-${WS_ENABLE} += \
	${ws_ldflags-y}

libmbus-server.so_cflags-${ZLIB_ENABLE} += \
	${zlib_cflags-y}

libmbus-server.so_ldflags-${ZLIB_ENABLE} += \
	${zlib_ldflags-y}

libmbus-server.so_ldflags-${ZLIB_ENABLE} += \
	-lz

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
	libmbus-server.a

dist.lib-${SHARED_ENABLE} += \
	libmbus-server.so

include ../../Makefile.lib
