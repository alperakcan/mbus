
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
	-lmbus-client \
	-lmbus-socket \
	-lmbus-json \
	-lmbus-version \
	-lmbus-clock \
	-lmbus-buffer \
	-lmbus-json-cJSON \
	-lmbus-compress \
	-lmbus-debug

mbus-event_ldflags-${SSL_ENABLE} += \
	${ssl_ldflags-y}

mbus-event_ldflags-${ZLIB_ENABLE} += \
	${zlib_ldflags-y}

mbus-event_ldflags-y += \
	-lpthread \
	-lm

distdir = ../../dist

dist.bin-y = \
	mbus-event

include ../../Makefile.lib
