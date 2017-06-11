
OPENSSL_ENABLE ?= y

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

libmbus-client.so_cflags-${OPENSSL_ENABLE} += \
	-DOPENSSL_ENABLE=1 \
	$(shell pkg-config --cflags libssl)

libmbus-client.so_ldflags-y += \
	-lmbus-debug \
	-lmbus-clock \
	-lmbus-buffer \
	-lmbus-socket \
	-lmbus-json

libmbus-client.so_ldflags-${OPENSSL_ENABLE} += \
	$(shell pkg-config --libs libssl) \
	$(shell pkg-config --libs libcrypto)

libmbus-client.so_ldflags-${OPENSSL_ENABLE} += \
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
