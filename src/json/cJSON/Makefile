
include ../../../Makefile.conf

target.a-y = \
	libmbus-json-cJSON.a

target.so-${SHARED_ENABLE} = \
	libmbus-json-cJSON.so

libmbus-json-cJSON.so_files-y = \
	cJSON.c

libmbus-json-cJSON.so_cflags-y = \
	-Wno-implicit-fallthrough

libmbus-json-cJSON.so_ldflags-y = \
	-lm

libmbus-json-cJSON.a_files-y = \
	${libmbus-json-cJSON.so_files-y}

libmbus-json-cJSON.a_cflags-y = \
	${libmbus-json-cJSON.so_cflags-y}

distdir = ../../../dist

dist.base = mbus

dist.lib-y = \
	libmbus-json-cJSON.a

dist.lib-${SHARED_ENABLE} += \
	libmbus-json-cJSON.so

include ../../../Makefile.lib
