
include ../../Makefile.conf

target-y = \
	mbus-subscribe

mbus-subscribe_includes-y = \
	../../dist/include

mbus-subscribe_libraries-y = \
	../../dist/lib

mbus-subscribe_files-y = \
	subscribe.c

mbus-subscribe_ldflags-y = \
	-lmbus-client \
	-lmbus-socket \
	-lmbus-json \
	-lmbus-version \
	-lmbus-clock \
	-lmbus-buffer \
	-lmbus-json-cJSON \
	-lmbus-compress \
	-lmbus-debug

mbus-subscribe_ldflags-${SSL_ENABLE} += \
	${ssl_ldflags-y}

mbus-subscribe_ldflags-${ZLIB_ENABLE} += \
	${zlib_ldflags-y}

mbus-subscribe_ldflags-y += \
	-lpthread \
	-lm \
	-ldl

dist.dir = ../../dist

dist.bin-y = \
	mbus-subscribe \
	mbus-subscribe.py \
	mbus-subscribe.rb

include ../../Makefile.lib
