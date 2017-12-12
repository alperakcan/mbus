
include ../../Makefile.conf

target-y = \
	mbus-client

mbus-client_includes-y = \
	../../dist/include

mbus-client_libraries-y = \
	../../dist/lib

mbus-client_files-y = \
	client.c

mbus-client_ldflags-y = \
	-lmbus-client \
	-lmbus-socket \
	-lmbus-json \
	-lmbus-version \
	-lmbus-clock \
	-lmbus-buffer \
	-lmbus-json-cJSON \
	-lmbus-compress \
	-lmbus-debug

mbus-client_ldflags-${SSL_ENABLE} += \
	${ssl_ldflags-y}

mbus-client_ldflags-${ZLIB_ENABLE} += \
	${zlib_ldflags-y}

mbus-client_ldflags-y += \
	-lpthread \
	-lm \
	-ldl \
	-lreadline

distdir = ../../dist

dist.bin-y = \
	mbus-client

include ../../Makefile.lib
