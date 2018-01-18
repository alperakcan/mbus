
include ../../Makefile.conf

target-y = \
	mbus-benchmark

mbus-benchmark_includes-y = \
	../../dist/include

mbus-benchmark_libraries-y = \
	../../dist/lib

mbus-benchmark_files-y = \
	benchmark.c

mbus-benchmark_ldflags-y = \
	-lmbus-client \
	-lmbus-socket \
	-lmbus-json \
	-lmbus-version \
	-lmbus-clock \
	-lmbus-buffer \
	-lmbus-json-cJSON \
	-lmbus-compress \
	-lmbus-debug

mbus-benchmark_ldflags-${SSL_ENABLE} += \
	${ssl_ldflags-y}

mbus-benchmark_ldflags-${ZLIB_ENABLE} += \
	${zlib_ldflags-y}

mbus-benchmark_ldflags-y += \
	-lpthread \
	-lm \
	-ldl

distdir = ../../dist

dist.bin-y = \
	mbus-benchmark

include ../../Makefile.lib
