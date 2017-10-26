
include ../../Makefile.conf

target-y = \
	mbus-execute-command

mbus-execute-command_files-y = \
	clock.c \
	exec.c \
	execute-command.c

mbus-execute-command_cflags-y = \
	-I../../dist/include

mbus-execute-command_ldflags-y = \
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

mbus-execute-command_ldflags-${SSL_ENABLE} += \
	${ssl_ldflags-y}

mbus-execute-command_ldflags-${ZLIB_ENABLE} += \
	${zlib_ldflags-y}

mbus-execute-command_ldflags-y += \
	-lpthread \
	-lm \
	-ldl

include ../../Makefile.lib
