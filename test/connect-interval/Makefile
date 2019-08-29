
include ../../Makefile.conf

target-y = \
	mbus-test-connect-interval

mbus-test-connect-interval_files-y = \
	main.c

mbus-test-connect-interval_cflags-y = \
	-I../../dist/include

mbus-test-connect-interval_ldflags-y = \
	-L../../dist/lib \
	-lmbus-client \
	-lmbus-socket \
	-lmbus-json \
	-lmbus-version \
	-lmbus-clock \
	-lmbus-buffer \
	-lmbus-json-cJSON \
	-lmbus-compress \
	-lmbus-debug

mbus-test-connect-interval_ldflags-${SSL_ENABLE} += \
	${ssl_ldflags-y}

mbus-test-connect-interval_ldflags-${ZLIB_ENABLE} += \
	${zlib_ldflags-y}

mbus-test-connect-interval_ldflags-y += \
	-lm \
	-ldl

dist.dir = ../../dist

dist.bin-y = \
	mbus-test-connect-interval

include ../../Makefile.lib
