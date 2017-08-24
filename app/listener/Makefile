
include ../../Makefile.conf

target-y = \
	mbus-listener

mbus-listener_includes-y = \
	../../dist/include

mbus-listener_libraries-y = \
	../../dist/lib

mbus-listener_files-y = \
	listener.c

mbus-listener_ldflags-y = \
	-lmbus-client \
	-lmbus-socket \
	-lmbus-json \
	-lmbus-version \
	-lmbus-clock \
	-lmbus-buffer \
	-lmbus-json-cJSON \
	-lmbus-compress \
	-lmbus-debug

mbus-listener_ldflags-${SSL_ENABLE} += \
	${ssl_ldflags-y}

mbus-listener_ldflags-${ZLIB_ENABLE} += \
	${zlib_ldflags-y}

mbus-listener_ldflags-y += \
	-lpthread \
	-lm

distdir = ../../dist

dist.bin-y = \
	mbus-listener

include ../../Makefile.lib
