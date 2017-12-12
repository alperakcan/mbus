
include ../../Makefile.conf

target-y += \
	mbus-test-logger-publish \
	mbus-test-logger-subscribe

mbus-test-logger-publish_files-y += \
	publish.c

mbus-test-logger-publish_includes-y += \
	../../dist/include

mbus-test-logger-publish_libraries-y += \
	../../dist/lib

mbus-test-logger-publish_ldflags-y += \
	-lmbus-client \
	-lmbus-socket \
	-lmbus-json \
	-lmbus-version \
	-lmbus-clock \
	-lmbus-buffer \
	-lmbus-json-cJSON \
	-lmbus-compress \
	-lmbus-debug

mbus-file-publish_ldflags-${SSL_ENABLE} += \
	${ssl_ldflags-y}

mbus-file-publish_ldflags-${ZLIB_ENABLE} += \
	${zlib_ldflags-y}

mbus-file-publish_ldflags-y += \
	-lpthread \
	-lm \
	-ldl

mbus-test-logger-subscribe_files-y += \
	subscribe.c

mbus-test-logger-subscribe_includes-y += \
	../../dist/include

mbus-test-logger-subscribe_libraries-y += \
	../../dist/lib

mbus-test-logger-subscribe_ldflags-y += \
	-lmbus-client \
	-lmbus-socket \
	-lmbus-json \
	-lmbus-version \
	-lmbus-clock \
	-lmbus-buffer \
	-lmbus-json-cJSON \
	-lmbus-compress \
	-lmbus-debug

mbus-file-subscribe_ldflags-${SSL_ENABLE} += \
	${ssl_ldflags-y}

mbus-file-subscribe_ldflags-${ZLIB_ENABLE} += \
	${zlib_ldflags-y}

mbus-file-subscribe_ldflags-y += \
	-lpthread \
	-lm \
	-ldl

distdir = ../../dist

dist.bin-y = \
	mbus-test-logger-publish \
	mbus-test-logger-subscribe

include ../../Makefile.lib
