
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
	-lmbus-debug \
	-lmbus-buffer \
	-lmbus-clock \
	-lmbus-compress \
	-lmbus-socket \
	-lmbus-client \
	-lmbus-json \
	-lmbus-json-cJSON \
	-lmbus-version

mbus-listener_ldflags-${SSL_ENABLE} += \
	${ssl_ldflags-y}

mbus-listener_ldflags-${ZLIB_ENABLE} += \
	${zlib_ldflags-y}

distdir = ../../dist

dist.bin-y = \
	mbus-listener

include ../../Makefile.lib
