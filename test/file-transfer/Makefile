
include ../../Makefile.conf

target-y = \
	mbus-file-transfer

mbus-file-transfer_files-y = \
	base64.c \
	file-transfer.c

mbus-file-transfer_cflags-y = \
	-I../../dist/include

mbus-file-transfer_ldflags-y = \
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

mbus-file-transfer_ldflags-${SSL_ENABLE} += \
	${ssl_ldflags-y}

mbus-file-transfer_ldflags-${ZLIB_ENABLE} += \
	${zlib_ldflags-y}

mbus-file-transfer_ldflags-y += \
	-lpthread \
	-lm \
	-ldl

include ../../Makefile.lib
