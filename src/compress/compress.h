
enum mbus_compress_method {
	mbus_compress_method_unknown,
	mbus_compress_method_none,
	mbus_compress_method_zlib
};

const char * mbus_compress_method_string (enum mbus_compress_method compression);
enum mbus_compress_method mbus_compress_method_value (const char *string);

int mbus_compress_data (void **dst, int *dstlen, const void *src, int srclen);
int mbus_uncompress_data (void **dst, int *dstlen, const void *src, int srclen);
