
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>

#define MBUS_DEBUG_NAME	"app-command"

#include "mbus/debug.h"
#include "mbus/json.h"
#include "mbus/method.h"
#include "mbus/client.h"
#include "mbus/server.h"

#define OPTION_HELP		0x100
#define OPTION_DESTINATION	0x101
#define OPTION_COMMAND		0x102
#define OPTION_PAYLOAD		0x103
static struct option longopts[] = {
	{ "help",			no_argument,		NULL,	OPTION_HELP },
	{ "destination",		required_argument,	NULL,	OPTION_DESTINATION },
	{ "command",			required_argument,	NULL,	OPTION_COMMAND },
	{ "payload",			required_argument,	NULL,	OPTION_PAYLOAD },
	{ NULL,				0,			NULL,	0 },
};

static void usage (void)
{
	fprintf(stdout, "mbus command arguments:\n");
	fprintf(stdout, "  --destination            : destination identifier\n");
	fprintf(stdout, "  --command                : command identifier\n");
	fprintf(stdout, "  --payload                : payload json\n");
	fprintf(stdout, "  --help                   : this text\n");
	fprintf(stdout, "  --mbus-help              : mbus help text\n");
	mbus_client_usage();
}

struct arg {
	const char *destination;
	const char *command;
	struct mbus_json *payload;
	int finished;
	int result;
};

static void mbus_client_callback_command (struct mbus_client *client, void *context, struct mbus_client_message *message, enum mbus_client_command_status status)
{
	struct arg *arg = context;
	char *string;
	(void) client;
	(void) status;
	if (mbus_client_message_command_response_result(message) == 0) {
		string = mbus_json_print(mbus_client_message_command_response_payload(message));
		if (string == NULL) {
			return;
		}
		fprintf(stdout, "%s\n", string);
		free(string);
	}
	arg->result = mbus_client_message_command_response_result(message);
	arg->finished = 1;
}

static void mbus_client_callback_connect (struct mbus_client *client, void *context, enum mbus_client_connect_status status)
{
	int rc;
	struct arg *arg = context;
	if (status == mbus_client_connect_status_success) {
		rc = mbus_client_command(client, arg->destination, arg->command, arg->payload, mbus_client_callback_command, arg);
		if (rc != 0) {
			arg->result = -1;
			arg->finished = 1;
		}
	} else {
		arg->result = -1;
		arg->finished = 1;
	}
}

int main (int argc, char *argv[])
{
	int rc;

	int c;
	int _argc;
	char **_argv;

	struct arg arg;
	struct mbus_client *client;
	struct mbus_client_options options;

	client = NULL;
	memset(&arg, 0, sizeof(struct arg));

	_argc = 0;
	_argv = NULL;

	_argv = malloc(sizeof(char *) * argc);
	if (_argv == NULL) {
		fprintf(stderr, "can not allocate memory\n");
		goto bail;
	}
	for (_argc = 0; _argc < argc; _argc++) {
		_argv[_argc] = argv[_argc];
	}

	while ((c = getopt_long(_argc, _argv, ":", longopts, NULL)) != -1) {
		switch (c) {
			case OPTION_DESTINATION:
				arg.destination = optarg;
				break;
			case OPTION_COMMAND:
				arg.command = optarg;
				break;
			case OPTION_PAYLOAD:
				arg.payload = mbus_json_parse(optarg);
				if (arg.payload == NULL) {
					fprintf(stderr, "invalid payload: %s\n", optarg);
					goto bail;
				}
				break;
			case OPTION_HELP:
				usage();
				goto bail;
		}
	}
	if (arg.destination == NULL) {
		fprintf(stderr, "destination is invalid\n");
		goto bail;
	}
	if (arg.command == NULL) {
		fprintf(stderr, "command is invalid\n");
		goto bail;
	}

	rc = mbus_client_options_from_argv(&options, argc, argv);
	if (rc != 0) {
		fprintf(stderr, "can not parse options\n");
		goto bail;
	}
	options.callbacks.connect = mbus_client_callback_connect;
	options.callbacks.context = &arg;
	client = mbus_client_create(&options);
	if (client == NULL) {
		fprintf(stderr, "can not create client\n");
		goto bail;
	}
	rc = mbus_client_connect(client);
	if (rc != 0) {
		fprintf(stderr, "can not connect client\n");
		goto bail;
	}

	while (1) {
		rc = mbus_client_run(client, MBUS_CLIENT_DEFAULT_RUN_TIMEOUT);
		if (rc != 0) {
			fprintf(stderr, "client run failed\n");
			goto bail;
		}
		if (arg.finished == 1 &&
		    mbus_client_has_pending(client) == 0) {
			break;
		}
	}

	mbus_json_delete(arg.payload);
	mbus_client_destroy(client);
	free(_argv);
	return arg.result;
bail:	if (client != NULL) {
		mbus_client_destroy(client);
	}
	if (arg.payload != NULL) {
		mbus_json_delete(arg.payload);
	}
	if (_argv != NULL) {
		free(_argv);
	}
	return -1;
}
