
include ../../Makefile.conf

target.a-y = \
	libmbus-clock.a

target.so-${SHARED_ENABLE} = \
	libmbus-clock.so

libmbus-clock.so_includes-y = \
	../../dist/include

libmbus-clock.so_libraries-y = \
	../../dist/lib

libmbus-clock.so_files-y = \
	clock.c

libmbus-clock.so_ldflags-${__LINUX__} += \
	-lrt

libmbus-clock.a_includes-y = \
	${libmbus-clock.so_includes-y}

libmbus-clock.a_files-y = \
	${libmbus-clock.so_files-y}

dist.base = mbus

dist.dir = ../../dist

dist.include-y = \
	clock.h

dist.lib-y = \
	libmbus-clock.a

dist.lib-${SHARED_ENABLE} += \
	libmbus-clock.so

include ../../Makefile.lib
