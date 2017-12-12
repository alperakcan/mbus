
include ../../Makefile.conf

target-y += \
	mbus-logger-publish \
	mbus-logger-subscribe

mbus-logger-publish_files-y += \
	publish.c

mbus-logger-publish_includes-y += \
	../../dist/include

mbus-logger-publish_libraries-y += \
	../../dist/lib

mbus-logger-publish_ldflags-y += \
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

mbus-logger-subscribe_files-y += \
	subscribe.c

mbus-logger-subscribe_includes-y += \
	../../dist/include

mbus-logger-subscribe_libraries-y += \
	../../dist/lib

mbus-logger-subscribe_ldflags-y += \
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

include ../../Makefile.lib
