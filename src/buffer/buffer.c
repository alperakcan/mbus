
/*
 * Copyright (c) 2017, Alper Akcan <alper.akcan@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *    * Neither the name of the <Alper Akcan> nor the
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
#include <stdint.h>
#include <arpa/inet.h>

#define MBUS_DEBUG_NAME	"mbus-buffer"

#include "mbus/debug.h"
#include "buffer.h"

struct mbus_buffer {
	unsigned int length;
	unsigned int size;
	uint8_t *buffer;
};

void mbus_buffer_destroy (struct mbus_buffer *buffer)
{
	if (buffer == NULL) {
		return;
	}
	if (buffer->buffer != NULL) {
		free(buffer->buffer);
	}
	free(buffer);
}

struct mbus_buffer * mbus_buffer_create (void)
{
	struct mbus_buffer *buffer;
	buffer = malloc(sizeof(struct mbus_buffer));
	if (buffer == NULL) {
		mbus_errorf("can not allocate memory");
		goto bail;
	}
	memset(buffer, 0, sizeof(struct mbus_buffer));
	return buffer;
bail:	if (buffer != NULL) {
		mbus_buffer_destroy(buffer);
	}
	return NULL;
}

unsigned int mbus_buffer_size (struct mbus_buffer *buffer)
{
	return buffer->size;
}

unsigned int mbus_buffer_length (struct mbus_buffer *buffer)
{
	return buffer->length;
}

int mbus_buffer_set_length (struct mbus_buffer *buffer, unsigned int length)
{
	if (length > buffer->size) {
		return -1;
	}
	buffer->length = length;
	return 0;
}

uint8_t * mbus_buffer_base (struct mbus_buffer *buffer)
{
	return buffer->buffer;
}

int mbus_buffer_reserve (struct mbus_buffer *buffer, unsigned int length)
{
	uint8_t *tmp;
	if (buffer->size >= length) {
		return 0;
	}
	while (buffer->size < length) {
		buffer->size += 4096;
	}
	tmp = realloc(buffer->buffer, buffer->size);
	if (tmp == NULL) {
		tmp = malloc(buffer->size);
		if (tmp == NULL) {
			mbus_errorf("can not allocate memory");
			return -1;
		}
		memcpy(tmp, buffer->buffer, buffer->length);
		free(buffer->buffer);
	}
	buffer->buffer = tmp;
	return 0;
}

int mbus_buffer_push (struct mbus_buffer *buffer, const void *data, unsigned int length)
{
	int rc;
	rc = mbus_buffer_reserve(buffer, buffer->length + length);
	if (rc != 0) {
		mbus_errorf("can not reserve buffer");
		return -1;
	}
	memcpy(buffer->buffer + buffer->length, data, length);
	buffer->length += length;
	return 0;
}

int mbus_buffer_push_string (struct mbus_buffer *buffer, const char *string)
{
	int rc;
	uint32_t length;
	if (string == NULL) {
		mbus_errorf("string is invalid");
		return -1;
	}
	length = strlen(string);
	length = htonl(length);
	rc = mbus_buffer_push(buffer, &length, sizeof(length));
	if (rc != 0) {
		mbus_errorf("can not push length");
		return -1;
	}
	length = ntohl(length);
	rc = mbus_buffer_push(buffer, string, length);
	if (rc != 0) {
		mbus_errorf("can not push string");
		return -1;
	}
	return 0;
}

int mbus_buffer_shift (struct mbus_buffer *buffer, unsigned int length)
{
	if (length == 0) {
		return 0;
	}
	if (length > buffer->length) {
		mbus_errorf("invalid length");
		return -1;
	}
	memmove(buffer->buffer, buffer->buffer + length, buffer->length - length);
	buffer->length -= length;
	return 0;
}

