
include ../../Makefile.conf

target-y = \
	mbus-publish

mbus-publish_includes-y = \
	../../dist/include

mbus-publish_libraries-y = \
	../../dist/lib

mbus-publish_files-y = \
	publish.c

mbus-publish_ldflags-y = \
	-lmbus-client \
	-lmbus-socket \
	-lmbus-json \
	-lmbus-version \
	-lmbus-clock \
	-lmbus-buffer \
	-lmbus-json-cJSON \
	-lmbus-compress \
	-lmbus-debug

mbus-publish_ldflags-${SSL_ENABLE} += \
	${ssl_ldflags-y}

mbus-publish_ldflags-${ZLIB_ENABLE} += \
	${zlib_ldflags-y}

mbus-publish_ldflags-y += \
	-lpthread \
	-lm \
	-ldl

distdir = ../../dist

dist.bin-y = \
	mbus-publish \
	mbus-publish.py \
	mbus-publish.rb

include ../../Makefile.lib
