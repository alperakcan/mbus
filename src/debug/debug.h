
/*
 * Copyright (c) 2014, Alper Akcan <alper.akcan@gmail.com>
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

#if !defined(MBUS_DEBUG_NAME)
#define MBUS_DEBUG_NAME	"unknown"
#endif

enum mbus_debug_level {
	mbus_debug_level_silent,
	mbus_debug_level_error,
	mbus_debug_level_info,
	mbus_debug_level_debug,
};

extern enum mbus_debug_level mbus_debug_level;

#define mbus_debugf(a...) { \
	if (mbus_debug_level >= mbus_debug_level_debug) { \
		fprintf(stderr, "mbus:%s:debug: ", MBUS_DEBUG_NAME); \
		fprintf(stderr, a); \
		fprintf(stderr, " (%s %s:%d)\n", __FUNCTION__, __FILE__, __LINE__); \
		fflush(stderr); \
	} \
}

#define mbus_infof(a...) { \
	if (mbus_debug_level >= mbus_debug_level_info) { \
		fprintf(stderr, "mbus:%s:info: ", MBUS_DEBUG_NAME); \
		fprintf(stderr, a); \
		fprintf(stderr, " (%s %s:%d)\n", __FUNCTION__, __FILE__, __LINE__); \
		fflush(stderr); \
	} \
}

#define mbus_errorf(a...) { \
	if (mbus_debug_level >= mbus_debug_level_error) { \
		fprintf(stderr, "mbus:%s:error: ", MBUS_DEBUG_NAME); \
		fprintf(stderr, a); \
		fprintf(stderr, " (%s %s:%d)\n", __FUNCTION__, __FILE__, __LINE__); \
		fflush(stderr); \
	} \
}

const char * mbus_debug_level_to_string (enum mbus_debug_level level);
enum mbus_debug_level mbus_debug_level_from_string (const char *string);