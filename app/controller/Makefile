
target-y = \
	mbus-controller

mbus-controller_includes-y = \
	../../dist/include

mbus-controller_libraries-y = \
	../../dist/lib

mbus-controller_files-y = \
	controller.c

mbus-controller_ldflags-y = \
	-lmbus-debug \
	-lmbus-buffer \
	-lmbus-server

distdir = ../../dist

dist.bin-y = \
	mbus-controller

include ../../Makefile.lib
