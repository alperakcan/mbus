
include ../../Makefile.conf

target-y = \
	mbus-broker

mbus-broker_includes-y = \
	../../dist/include

mbus-broker_libraries-y = \
	../../dist/lib

mbus-broker_files-y = \
	broker.c

mbus-broker_ldflags-y = \
	-lmbus-server \
	-lmbus-socket \
	-lmbus-json \
	-lmbus-version \
	-lmbus-clock \
	-lmbus-buffer \
	-lmbus-json-cJSON \
	-lmbus-compress \
	-lmbus-debug

mbus-broker_ldflags-${WS_ENABLE} += \
	${ws_ldflags-y}

mbus-broker_ldflags-${SSL_ENABLE} += \
	${ssl_ldflags-y}

mbus-broker_ldflags-${ZLIB_ENABLE} += \
	${zlib_ldflags-y}

mbus-broker_ldflags-y += \
	-lpthread \
	-lm \
	-ldl

dist.dir = ../../dist

dist.bin-y = \
	mbus-broker

include ../../Makefile.lib
