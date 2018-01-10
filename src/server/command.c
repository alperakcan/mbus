
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

#include "mbus/debug.h"
#include "mbus/tailq.h"
#include "command.h"

struct private {
	struct command command;
	char *identifier;
};

const char * command_get_identifier (const struct command *command)
{
	const struct private *private;
	if (command == NULL) {
		return NULL;
	}
	private = (const struct private *) command;
	return private->identifier;
}

void command_destroy (struct command *command)
{
	struct private *private;
	if (command == NULL) {
		return;
	}
	private = (struct private *) command;
	if (private->identifier != NULL) {
		free(private->identifier);
	}
	free(private);
}

struct command * command_create (const char *identifier)
{
	struct private *private;
	private = NULL;
	if (identifier == NULL) {
		mbus_errorf("identifier is null");
		goto bail;
	}
	private = malloc(sizeof(struct private));
	if (private == NULL) {
		mbus_errorf("can not allocate memory");
		goto bail;
	}
	memset(private, 0, sizeof(struct private));
	private->identifier = strdup(identifier);
	if (private->identifier == NULL) {
		mbus_errorf("can not allocate memory");
		goto bail;
	}
	return &private->command;
bail:	if (private != NULL) {
		command_destroy(&private->command);
	}
	return NULL;
}
