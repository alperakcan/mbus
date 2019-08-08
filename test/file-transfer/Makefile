
include ../../Makefile.conf

target-y = \
	mbus-test-file-transfer

mbus-test-file-transfer_files-y = \
	base64.c \
	file-transfer.c

mbus-test-file-transfer_cflags-y = \
	-I../../dist/include

mbus-test-file-transfer_ldflags-y = \
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

mbus-test-file-transfer_ldflags-${SSL_ENABLE} += \
	${ssl_ldflags-y}

mbus-test-file-transfer_ldflags-${ZLIB_ENABLE} += \
	${zlib_ldflags-y}

mbus-test-file-transfer_ldflags-y += \
	-lpthread \
	-lm \
	-ldl

dist.dir = ../../dist

dist.bin-y = \
	mbus-test-file-transfer

include ../../Makefile.lib
