
include ../../Makefile.conf

target-y = \
	mbus-controller

mbus-controller_includes-y = \
	../../dist/include

mbus-controller_libraries-y = \
	../../dist/lib

mbus-controller_files-y = \
	controller.c

mbus-controller_ldflags-y = \
	-lmbus-debug \
	-lmbus-buffer \
	-lmbus-server \
	-lmbus-clock \
	-lmbus-compress \
	-lmbus-socket \
	-lmbus-json \
	-lmbus-json-cJSON \
	-lmbus-version

mbus-controller_ldflags-${WS_ENABLE} += \
	${ws_ldflags-y}

mbus-controller_ldflags-${SSL_ENABLE} += \
	${ssl_ldflags-y}

mbus-controller_ldflags-${ZLIB_ENABLE} += \
	${zlib_ldflags-y}

distdir = ../../dist

dist.bin-y = \
	mbus-controller

include ../../Makefile.lib
