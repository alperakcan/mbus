
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

struct mbus_buffer;

struct mbus_buffer * mbus_buffer_create (void);
void mbus_buffer_destroy (struct mbus_buffer *buffer);
int mbus_buffer_reset (struct mbus_buffer *buffer);
unsigned int mbus_buffer_get_size (struct mbus_buffer *buffer);
unsigned int mbus_buffer_get_length (struct mbus_buffer *buffer);
int mbus_buffer_set_length (struct mbus_buffer *buffer, unsigned int length);
uint8_t * mbus_buffer_get_base (struct mbus_buffer *buffer);
int mbus_buffer_reserve (struct mbus_buffer *buffer, unsigned int length);
int mbus_buffer_push (struct mbus_buffer *buffer, const void *data, unsigned int length);
int mbus_buffer_push_string (struct mbus_buffer *buffer, enum mbus_compress_method compression, const char *string);
int mbus_buffer_shift (struct mbus_buffer *buffer, unsigned int length);
