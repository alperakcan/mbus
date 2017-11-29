
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
	mbus_debug_level_warning,
	mbus_debug_level_notice,
	mbus_debug_level_info,
	mbus_debug_level_debug,
};

extern enum mbus_debug_level mbus_debug_level;

#define mbus_debugf(a...) { \
	mbus_debug_printf(mbus_debug_level_debug, MBUS_DEBUG_NAME, __FUNCTION__, __FILE__, __LINE__, a); \
}

#define mbus_warningf(a...) { \
	mbus_debug_printf(mbus_debug_level_warning, MBUS_DEBUG_NAME, __FUNCTION__, __FILE__, __LINE__, a); \
}

#define mbus_noticef(a...) { \
	mbus_debug_printf(mbus_debug_level_notice, MBUS_DEBUG_NAME, __FUNCTION__, __FILE__, __LINE__, a); \
}

#define mbus_infof(a...) { \
	mbus_debug_printf(mbus_debug_level_info, MBUS_DEBUG_NAME, __FUNCTION__, __FILE__, __LINE__, a); \
}

#define mbus_errorf(a...) { \
	mbus_debug_printf(mbus_debug_level_error, MBUS_DEBUG_NAME, __FUNCTION__, __FILE__, __LINE__, a); \
}

const char * mbus_debug_level_to_string (enum mbus_debug_level level);
enum mbus_debug_level mbus_debug_level_from_string (const char *string);
int mbus_debug_printf (enum mbus_debug_level level, const char *name, const char *function, const char *file, int line, const char *fmt, ...);
