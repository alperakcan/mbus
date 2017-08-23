
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
	-lmbus-debug \
	-lmbus-buffer \
	-lmbus-clock \
	-lmbus-compress \
	-lmbus-socket \
	-lmbus-client \
	-lmbus-json \
	-lmbus-json-cJSON \
	-lmbus-version

mbus-command_ldflags-${SSL_ENABLE} += \
	${ssl_ldflags-y}

mbus-command_ldflags-${ZLIB_ENABLE} += \
	${zlib_ldflags-y}

distdir = ../../dist

dist.bin-y = \
	mbus-command

include ../../Makefile.lib
