
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

#define MBUS_DEBUG_NAME	"app-event"

#include "mbus/debug.h"
#include "mbus/json.h"
#include "mbus/method.h"
#include "mbus/client.h"
#include "mbus/server.h"

#define OPTION_HELP		0x100
#define OPTION_DESTINATION	0x101
#define OPTION_EVENT		0x102
#define OPTION_PAYLOAD		0x103
#define OPTION_FLOOD		0x104
static struct option longopts[] = {
	{ "help",			no_argument,		NULL,	OPTION_HELP },
	{ "destination",		required_argument,	NULL,	OPTION_DESTINATION },
	{ "event",			required_argument,	NULL,	OPTION_EVENT },
	{ "payload",			required_argument,	NULL,	OPTION_PAYLOAD },
	{ "flood",			required_argument,	NULL,	OPTION_FLOOD },
	{ NULL,				0,			NULL,	0 },
};

static void usage (void)
{
	fprintf(stdout, "mbus event arguments:\n");
	fprintf(stdout, "  --destination            : destination identifier\n");
	fprintf(stdout, "  --event                  : event identifier\n");
	fprintf(stdout, "  --payload                : payload json\n");
	fprintf(stdout, "  --flood                  : flood event n times\n");
	fprintf(stdout, "  --help                   : this text\n");
	fprintf(stdout, "  --mbus-help              : mbus help text\n");
	mbus_client_usage();
}

struct arg {
	const char *destination;
	const char *event;
	struct mbus_json *payload;
	int flood;
	int published;
	int finished;
	int result;
};

static void mbus_client_callback_publish (struct mbus_client *client, void *context, struct mbus_client_message *message, enum mbus_client_publish_status status)
{
	struct arg *arg = context;
	(void) client;
	(void) message;
	arg->published += 1;
	if (status == mbus_client_publish_status_success) {
		arg->result = 0;
		if (arg->published == arg->flood) {
			arg->finished = 1;
		}
	} else {
		arg->result = -1;
		arg->finished = 1;
	}
}

static void mbus_client_callback_connect (struct mbus_client *client, void *context, enum mbus_client_connect_status status)
{
	int p;
	int rc;
	struct arg *arg = context;
	if (status == mbus_client_connect_status_success) {
		for (p = 0; p < arg->flood; p++) {
			rc = mbus_client_publish_sync_to(client, arg->destination, arg->event, arg->payload);
			if (rc != 0) {
				break;
			}
		}
		if (rc != 0) {
			arg->result = rc;
			arg->finished = 1;
		}
	} else {
		arg->result = 0;
		arg->finished = 1;
	}
}

int main (int argc, char *argv[])
{
	int c;
	int rc;

	int _argc;
	char **_argv;

	struct arg arg;
	struct mbus_client *client;
	struct mbus_client_options options;

	client = NULL;

	memset(&arg, 0, sizeof(struct arg));
	arg.destination = MBUS_METHOD_EVENT_DESTINATION_SUBSCRIBERS;
	arg.flood = 1;

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
			case OPTION_EVENT:
				arg.event = optarg;
				break;
			case OPTION_PAYLOAD:
				arg.payload = mbus_json_parse(optarg);
				if (arg.payload == NULL) {
					fprintf(stderr, "invalid payload\n");
					goto bail;
				}
				break;
			case OPTION_FLOOD:
				arg.flood = atoi(optarg);
				if (arg.flood <= 0) {
					fprintf(stderr, "invalid flood\n");
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
	if (arg.event == NULL) {
		fprintf(stderr, "event is invalid\n");
		goto bail;
	}

	rc = mbus_client_options_from_argv(&options, argc, argv);
	if (rc != 0) {
		fprintf(stderr, "can not parse options\n");
		goto bail;
	}
	options.callbacks.connect = mbus_client_callback_connect;
	options.callbacks.publish = mbus_client_callback_publish;
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
