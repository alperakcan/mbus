
target-y = \
	mbus-listener

mbus-listener_includes-y = \
	../../dist/include

mbus-listener_libraries-y = \
	../../dist/lib

mbus-listener_files-y = \
	listener.c

mbus-listener_ldflags-y = \
	-lmbus-debug \
	-lmbus-client \
	-lmbus-json

distdir = ../../dist

dist.bin-y = \
	mbus-listener

include ../../Makefile.lib
