
include ../../Makefile.conf

target.a-y = \
	libmbus-client.a

target.so-${SHARED_ENABLE} = \
	libmbus-client.so

libmbus-client.so_files-y = \
	client.c

libmbus-client.so_cflags-y += \
	-I../../dist/include

libmbus-client.so_ldflags-y += \
	-L../../dist/lib

libmbus-client.so_cflags-${SSL_ENABLE} += \
	${ssl_cflags-y}

libmbus-client.so_ldflags-y += \
	-lmbus-debug \
	-lmbus-clock \
	-lmbus-compress \
	-lmbus-buffer \
	-lmbus-socket \
	-lmbus-json \
	-lmbus-version \
	-lpthread

libmbus-client.so_ldflags-${SSL_ENABLE} += \
	${ssl_ldflags-y} \
	-lpthread

libmbus-client.so_cflags-${ZLIB_ENABLE} += \
	${zlib_cflags-y}

libmbus-client.a_cflags-y = \
	${libmbus-client.so_cflags-y}

libmbus-client.a_files-y = \
	${libmbus-client.so_files-y}

distdir = ../../dist

dist.base = mbus

dist.include-y = \
	client.h

dist.lib-y = \
	libmbus-client.a

dist.lib-${SHARED_ENABLE} += \
	libmbus-client.so

include ../../Makefile.lib

clean:
	${Q}${RM} *.pyc
