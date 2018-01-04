
include ../../Makefile.conf

target-y = \
	mbus-command

mbus-command_includes-y = \
	../../dist/include

mbus-command_libraries-y = \
	../../dist/lib

mbus-command_files-y = \
	command.c

mbus-command_ldflags-y = \
	-lmbus-client \
	-lmbus-socket \
	-lmbus-json \
	-lmbus-version \
	-lmbus-clock \
	-lmbus-buffer \
	-lmbus-json-cJSON \
	-lmbus-compress \
	-lmbus-debug

mbus-command_ldflags-${SSL_ENABLE} += \
	${ssl_ldflags-y}

mbus-command_ldflags-${ZLIB_ENABLE} += \
	${zlib_ldflags-y}

mbus-command_ldflags-y += \
	-lpthread \
	-lm \
	-ldl

distdir = ../../dist

dist.bin-y = \
	mbus-command \
	mbus-command.py \
	mbus-command.rb

include ../../Makefile.lib
