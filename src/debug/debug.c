
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

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>

#include "debug.h"

enum mbus_debug_level mbus_debug_level = mbus_debug_level_error;

const char * mbus_debug_level_to_string (enum mbus_debug_level level)
{
	switch (level) {
		case mbus_debug_level_silent: return "silent";
		case mbus_debug_level_error: return "error";
		case mbus_debug_level_info: return "info";
		case mbus_debug_level_debug: return "debug";
	}
	return "unknown";
}

enum mbus_debug_level mbus_debug_level_from_string (const char *string)
{
	if (string == NULL) {
		return mbus_debug_level_error;
	}
	if (strcmp(string, "silent") == 0) {
		return mbus_debug_level_silent;
	}
	if (strcmp(string, "error") == 0) {
		return mbus_debug_level_error;
	}
	if (strcmp(string, "info") == 0) {
		return mbus_debug_level_info;
	}
	if (strcmp(string, "debug") == 0) {
		return mbus_debug_level_debug;
	}
	return mbus_debug_level_error;
}

int mbus_debug_printf (enum mbus_debug_level level, const char *name, const char *function, const char *file, int line, const char *fmt, ...)
{
	char *str;
	va_list ap;

	struct timeval timeval;
	struct tm *tm;
	int milliseconds;
	char date[80];

	str = NULL;
	va_start(ap, fmt);
	if (level < mbus_debug_level) {
		goto out;
	}
	vasprintf(&str, fmt, ap);

	gettimeofday(&timeval, NULL);

	milliseconds = (int) ((timeval.tv_usec / 1000.0) + 0.5);
	if (milliseconds >= 1000) {
		milliseconds -= 1000;
		timeval.tv_sec++;
	}
	tm = localtime(&timeval.tv_sec);
	strftime(date, sizeof(date), "%x-%H:%M:%S", tm);

	fprintf(stderr, "mbus:%s.%03d:%s:%s: %s (%s %s:%d)\n", date, milliseconds, name, mbus_debug_level_to_string(level), str, function, file, line);

out:	va_end(ap);
	if (str != NULL) {
		free(str);
	}
	return 0;
}
