
include ../../Makefile.conf

target-y = \
	mbus-event

mbus-event_includes-y = \
	../../dist/include

mbus-event_libraries-y = \
	../../dist/lib

mbus-event_files-y = \
	event.c

mbus-event_ldflags-y = \
	-lmbus-debug \
	-lmbus-buffer \
	-lmbus-clock \
	-lmbus-compress \
	-lmbus-socket \
	-lmbus-client \
	-lmbus-json \
	-lmbus-json-cJSON \
	-lmbus-version

mbus-event_ldflags-${SSL_ENABLE} += \
	${ssl_ldflags-y}

mbus-event_ldflags-${ZLIB_ENABLE} += \
	${zlib_ldflags-y}

distdir = ../../dist

dist.bin-y = \
	mbus-event

include ../../Makefile.lib
