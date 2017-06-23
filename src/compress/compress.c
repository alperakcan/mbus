
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <zlib.h>

#include "mbus/debug.h"
#include "compress.h"

int mbus_compress_data (void **dst, int *dstlen, const void *src, int srclen)
{
	int rc;
	Bytef *compressed;
	uLongf compressedlen;
	compressed = NULL;
	compressedlen = 0;
	if (dst == NULL) {
		mbus_errorf("dst is invalid");
		goto bail;
	}
	if (dstlen == NULL) {
		mbus_errorf("dstlen is invalid");
		goto bail;
	}
	if (src == NULL) {
		mbus_errorf("dst is invalid");
		goto bail;
	}
	if (srclen <= 0) {
		mbus_errorf("srclen is invalid");
		goto bail;
	}
	compressedlen = compressBound(srclen);
	compressed = malloc(compressedlen);
	if (compressed == NULL) {
		mbus_errorf("can not allocate memory");
		goto bail;
	}
	rc = compress2(compressed, &compressedlen, src, srclen, Z_DEFAULT_COMPRESSION);
	if (rc != Z_OK) {
		mbus_errorf("can not compress data");
		goto bail;
	}
	*dst = compressed;
	*dstlen = compressedlen;
	return 0;
bail:	if (compressed != NULL) {
		free(compressed);
	}
	return -1;
}

int mbus_uncompress_data (void **dst, int *dstlen, const void *src, int srclen)
{
	int rc;
	Bytef *uncompressed;
	uLongf uncompressedlen;
	uncompressed = NULL;
	uncompressedlen = 0;
	if (dst == NULL) {
		mbus_errorf("dst is invalid");
		goto bail;
	}
	if (dstlen == NULL) {
		mbus_errorf("dstlen is invalid");
		goto bail;
	}
	if (src == NULL) {
		mbus_errorf("dst is invalid");
		goto bail;
	}
	if (*dstlen <= 0) {
		mbus_errorf("dstlen is invalid");
		goto bail;
	}
	if (srclen <= 0) {
		mbus_errorf("srclen is invalid");
		goto bail;
	}
	uncompressedlen = *dstlen;
	uncompressed = malloc(uncompressedlen);
	if (uncompressed == NULL) {
		mbus_errorf("can not allocate memory");
		goto bail;
	}
	rc = uncompress(uncompressed, &uncompressedlen, src, srclen);
	if (rc != Z_OK) {
		mbus_errorf("can not compress data");
		goto bail;
	}
	*dst = uncompressed;
	*dstlen = uncompressedlen;
	return 0;
bail:	if (uncompressed != NULL) {
		free(uncompressed);
	}
	return -1;
}

const char * mbus_compress_method_string (enum mbus_compress_method compression)
{
	if (compression == mbus_compress_method_none) return "none";
#if defined(ZLIB_ENABLE) && (ZLIB_ENABLE == 1)
	if (compression == mbus_compress_method_zlib) return "zlib";
#endif
	return "none";
}

enum mbus_compress_method mbus_compress_method_value (const char *string)
{
	if (string == NULL) {
		return mbus_compress_method_none;
	}
	if (strcmp(string, "none") == 0) {
		return mbus_compress_method_none;
	}
#if defined(ZLIB_ENABLE) && (ZLIB_ENABLE == 1)
	if (strcmp(string, "zlib") == 0) {
		return mbus_compress_method_zlib;
	}
#endif
	return mbus_compress_method_none;
}
