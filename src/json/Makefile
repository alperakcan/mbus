
subdir-y = \
	cJSON

target.so-y = \
	libmbus-json.so

libmbus-json.so_includes-y = \
	../../dist/include

libmbus-json.so_libraries-y = \
	../../dist/lib

libmbus-json.so_files-y = \
	json.c

libmbus-json.so_ldflags-y = \
	-lmbus-debug \
	-lmbus-socket \
	-lmbus-json-cJSON \
	-lpthread

distdir = ../../dist

dist.base = mbus

dist.include-y = \
	json.h

dist.lib-y = \
	libmbus-json.so

include ../../Makefile.lib
