
/*
 * Copyright (c) 2014-2018, Alper Akcan <alper.akcan@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *    * Neither the name of the copyright holder nor the
 *      names of its contributors may be used to endorse or promote products
 *      derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(ZLIB_ENABLE) && (ZLIB_ENABLE == 1)
#include <zlib.h>
#endif

#include "mbus/debug.h"
#include "compress.h"

#if defined(ZLIB_ENABLE) && (ZLIB_ENABLE == 1)

static int zlib_compress_data (void **dst, int *dstlen, const void *src, int srclen)
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

static int zlib_uncompress_data (void **dst, int *dstlen, const void *src, int srclen)
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

#endif

int mbus_compress_data (enum mbus_compress_method compression, void **dst, int *dstlen, const void *src, int srclen)
{
#if defined(ZLIB_ENABLE) && (ZLIB_ENABLE == 1)
	if (compression == mbus_compress_method_zlib) {
		return zlib_compress_data(dst, dstlen, src, srclen);
	}
#else
	(void) compression;
	(void) dst;
	(void) dstlen;
	(void) src;
	(void) srclen;
#endif
	return -1;
}

int mbus_uncompress_data (enum mbus_compress_method compression, void **dst, int *dstlen, const void *src, int srclen)
{
#if defined(ZLIB_ENABLE) && (ZLIB_ENABLE == 1)
	if (compression == mbus_compress_method_zlib) {
		return zlib_uncompress_data(dst, dstlen, src, srclen);
	}
#else
	(void) compression;
	(void) dst;
	(void) dstlen;
	(void) src;
	(void) srclen;
#endif
	return -1;
}

const char * mbus_compress_method_string (enum mbus_compress_method compression)
{
	if (compression == mbus_compress_method_none) return "none";
#if defined(ZLIB_ENABLE) && (ZLIB_ENABLE == 1)
	if (compression == mbus_compress_method_zlib) return "zlib";
#else
	(void) compression;
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
