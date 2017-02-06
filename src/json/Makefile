
subdir-y = \
	cJSON

target.a-y = \
	libmbus-json.a

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
	-lmbus-json-cJSON

libmbus-json.so_depends-y = \
	cJSON

libmbus-json.a_includes-y = \
	${libmbus-json.so_includes-y}

libmbus-json.a_files-y = \
	${libmbus-json.so_files-y}

distdir = ../../dist

dist.base = mbus

dist.include-y = \
	json.h

dist.lib-y = \
	libmbus-json.a \
	libmbus-json.so

include ../../Makefile.lib
