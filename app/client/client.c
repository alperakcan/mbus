
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
#include <unistd.h>
#include <getopt.h>

#include <ctype.h>
#include <poll.h>
#include <readline/readline.h>
#include <readline/history.h>

#define MBUS_DEBUG_NAME	"app-client"

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
	{ "help",		no_argument,	NULL,	OPTION_HELP },
	{ NULL,			0,		NULL,	0 },
};

static int g_running;
static struct mbus_client *g_mbus_client;

static void usage (void)
{
	fprintf(stdout, "mbus client arguments:\n");
	fprintf(stdout, "  --help     : this text\n");
	fprintf(stdout, "  --mbus-help: mbus help text\n");
	mbus_client_usage();
}

static void mbus_client_callback_connect (struct mbus_client *client, void *context, enum mbus_client_connect_status status)
{
	(void) client;
	(void) context;
	fprintf(stdout, "\033[0G** connect: %d, %s\n", status, mbus_client_connect_status_string(status));
	rl_on_new_line();
}

static void mbus_client_callback_disconnect (struct mbus_client *client, void *context, enum mbus_client_disconnect_status status)
{
	(void) client;
	(void) context;
	fprintf(stdout, "\033[0G** disconnect: %d, %s\n", status, mbus_client_disconnect_status_string(status));
	rl_on_new_line();
}

static void mbus_client_callback_message (struct mbus_client *client, void *context, struct mbus_client_message_event *message)
{
	char *string;
	(void) client;
	(void) context;
	string = mbus_json_print(mbus_client_message_event_payload(message));
	fprintf(stdout, "\033[0G** message: %s.%s: %s\n", mbus_client_message_event_source(message), mbus_client_message_event_identifier(message), string);
	free(string);
	rl_on_new_line();
}

static void mbus_client_callback_result (struct mbus_client *client, void *context, struct mbus_client_message_command *message, enum mbus_client_command_status status)
{
	char *request_string;
	char *response_string;
	(void) client;
	(void) context;
	fprintf(stdout, "\033[0G** command status: %d, %s\n", status, mbus_client_command_status_string(status));
	request_string = mbus_json_print(mbus_client_message_command_request_payload(message));
	response_string = mbus_json_print(mbus_client_message_command_response_payload(message));
	fprintf(stdout, "request: %s.%s: %s\n", mbus_client_message_command_request_destination(message), mbus_client_message_command_request_identifier(message), request_string);
	fprintf(stdout, "response: %d, %s\n", mbus_client_message_command_response_status(message), response_string);
	free(request_string);
	free(response_string);
	rl_on_new_line();
}

static int mbus_client_callback_routine (struct mbus_client *client, void *context, struct mbus_client_message_routine *message)
{
	int rc;
	char *request_string;
	char *response_string;
	struct mbus_json *response_payload;
	(void) client;
	(void) context;
	request_string = NULL;
	response_string = NULL;
	response_payload = NULL;
	fprintf(stdout, "\033[0G** routine\n");
	request_string = mbus_json_print(mbus_client_message_routine_request_payload(message));
	if (request_string == NULL) {
		fprintf(stderr, "can not print request payload");
		goto bail;
	}
	fprintf(stdout, "request: %s.%s: %s\n", mbus_client_message_routine_request_source(message), mbus_client_message_routine_request_identifier(message), request_string);
	response_payload = mbus_json_create_object();
	if (response_payload == NULL) {
		fprintf(stderr, "can not create payload object'n");
		goto bail;
	}
	rc = mbus_json_add_string_to_object_cs(response_payload, "source", mbus_client_message_routine_request_source(message));
	if (rc != 0) {
		fprintf(stderr, "can not add string to payload\n");
		goto bail;
	}
	rc = mbus_json_add_string_to_object_cs(response_payload, "identifier", mbus_client_message_routine_request_identifier(message));
	if (rc != 0) {
		fprintf(stderr, "can not add string to payload\n");
		goto bail;
	}
	response_string = mbus_json_print(response_payload);
	if (response_string == NULL) {
		fprintf(stderr, "can not print response payload");
		goto bail;
	}
	fprintf(stdout, "response: %s\n", response_string);
	rc = mbus_client_message_routine_set_response_payload(message, response_payload);
	if (rc != 0) {
		fprintf(stderr, "can not set response payload");
		goto bail;
	}
	free(request_string);
	free(response_string);
	mbus_json_delete(response_payload);
	rl_on_new_line();
	return 0;
bail:	if (request_string != NULL) {
		free(request_string);
	}
	if (response_string != NULL) {
		free(response_string);
	}
	if (response_payload != NULL) {
		mbus_json_delete(response_payload);
	}
	rl_on_new_line();
	return -1;
}

static void mbus_client_callback_publish (struct mbus_client *client, void *context, struct mbus_client_message_event *message, enum mbus_client_publish_status status)
{
	char *string;
	(void) client;
	(void) context;
	(void) message;
	string = mbus_json_print(mbus_client_message_event_payload(message));
	fprintf(stdout, "\033[0G** publish status: %d, %s message: %s.%s: %s\n", status, mbus_client_publish_status_string(status), mbus_client_message_event_destination(message), mbus_client_message_event_identifier(message), string);
	free(string);
	rl_on_new_line();
}

static void mbus_client_callback_subscribe (struct mbus_client *client, void *context, const char *source, const char *event, enum mbus_client_subscribe_status status)
{
	(void) client;
	(void) context;
	fprintf(stdout, "\033[0G** subscribe status: %d, %s, source: %s, event: %s\n", status, mbus_client_subscribe_status_string(status), source, event);
	rl_on_new_line();
}

static void mbus_client_callback_unsubscribe (struct mbus_client *client, void *context, const char *source, const char *event, enum mbus_client_unsubscribe_status status)
{
	(void) client;
	(void) context;
	fprintf(stdout, "\033[0G** unsubscribe status: %d, %s, source: %s, event: %s\n", status, mbus_client_unsubscribe_status_string(status), source, event);
	rl_on_new_line();
}

static void mbus_client_callback_registered (struct mbus_client *client, void *context, const char *command, enum mbus_client_register_status status)
{
	(void) client;
	(void) context;
	fprintf(stdout, "\033[0G** registered status: %d, %s, command: %s\n", status, mbus_client_register_status_string(status), command);
	rl_on_new_line();
}

static void mbus_client_callback_unregistered (struct mbus_client *client, void *context, const char *command, enum mbus_client_unregister_status status)
{
	(void) client;
	(void) context;
	fprintf(stdout, "\033[0G** unregistered status: %d, %s, command: %s\n", status, mbus_client_unregister_status_string(status), command);
	rl_on_new_line();
}

static void mbus_client_callback_message_callback (struct mbus_client *client, void *context, struct mbus_client_message_event *message)
{
	char *string;
	(void) client;
	(void) context;
	string = mbus_json_print(mbus_client_message_event_payload(message));
	fprintf(stdout, "\033[0G** message callback: %s.%s: %s\n", mbus_client_message_event_source(message), mbus_client_message_event_identifier(message), string);
	free(string);
	rl_on_new_line();
}

static void mbus_client_callback_command_callback (struct mbus_client *client, void *context, struct mbus_client_message_command *message, enum mbus_client_command_status status)
{
	char *request_string;
	char *response_string;
	(void) client;
	(void) context;
	fprintf(stdout, "\033[0G** command callback status: %d, %s\n", status, mbus_client_command_status_string(status));
	request_string = mbus_json_print(mbus_client_message_command_request_payload(message));
	response_string = mbus_json_print(mbus_client_message_command_response_payload(message));
	fprintf(stdout, "request: %s.%s: %s\n", mbus_client_message_command_request_destination(message), mbus_client_message_command_request_identifier(message), request_string);
	fprintf(stdout, "response: %d, %s\n", mbus_client_message_command_response_status(message), response_string);
	free(request_string);
	free(response_string);
	rl_on_new_line();
}

static int mbus_client_callback_routine_callback (struct mbus_client *client, void *context, struct mbus_client_message_routine *message)
{
	int rc;
	char *request_string;
	char *response_string;
	struct mbus_json *response_payload;
	(void) client;
	(void) context;
	request_string = NULL;
	response_string = NULL;
	response_payload = NULL;
	fprintf(stdout, "\033[0G** routine callback\n");
	request_string = mbus_json_print(mbus_client_message_routine_request_payload(message));
	if (request_string == NULL) {
		fprintf(stderr, "can not print request payload");
		goto bail;
	}
	fprintf(stdout, "request: %s.%s: %s\n", mbus_client_message_routine_request_source(message), mbus_client_message_routine_request_identifier(message), request_string);
	response_payload = mbus_json_create_object();
	if (response_payload == NULL) {
		fprintf(stderr, "can not create payload object'n");
		goto bail;
	}
	rc = mbus_json_add_string_to_object_cs(response_payload, "source", mbus_client_message_routine_request_source(message));
	if (rc != 0) {
		fprintf(stderr, "can not add string to payload\n");
		goto bail;
	}
	rc = mbus_json_add_string_to_object_cs(response_payload, "identifier", mbus_client_message_routine_request_identifier(message));
	if (rc != 0) {
		fprintf(stderr, "can not add string to payload\n");
		goto bail;
	}
	response_string = mbus_json_print(response_payload);
	if (response_string == NULL) {
		fprintf(stderr, "can not print response payload");
		goto bail;
	}
	fprintf(stdout, "response: %s\n", response_string);
	rc = mbus_client_message_routine_set_response_payload(message, response_payload);
	if (rc != 0) {
		fprintf(stderr, "can not set response payload");
		goto bail;
	}
	free(request_string);
	free(response_string);
	mbus_json_delete(response_payload);
	rl_on_new_line();
	return 0;
bail:	if (request_string != NULL) {
		free(request_string);
	}
	if (response_string != NULL) {
		free(response_string);
	}
	if (response_payload != NULL) {
		mbus_json_delete(response_payload);
	}
	rl_on_new_line();
	return -1;
}

static char * readline_strip (char *buf)
{
	char *start;
	if (buf == NULL) {
		return NULL;
	}
	while ((*buf != '\0') && (buf[strlen(buf) - 1] < 33)) {
		buf[strlen(buf) - 1] = '\0';
	}
	start = buf;
	while (*start && (*start < 33)) {
		start++;
	}
	return start;
}

struct command {
	char *name;
	int (*func)(int argc, char *argv[]);
	char *help;
};

static int command_quit (int argc, char *argv[])
{
	(void) argc;
	(void) argv;
	g_running = 0;
	if (g_mbus_client != NULL) {
		mbus_client_break_run(g_mbus_client);
	}
	return 1;
}

static int command_create (int argc, char *argv[])
{
	int rc;
	struct mbus_client_options options;

	int _argc;
	char **_argv;

	int c;
	struct option long_options[] = {
		{ "help",	no_argument,	0,	'h' },
		{ NULL,		0,		NULL,	0 }
	};

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

	optind = 0;
	while ((c = getopt_long(_argc, _argv, ":h", long_options, NULL)) != -1) {
		switch (c) {
			case 'h':
				fprintf(stdout, "create mbus client\n");
				fprintf(stdout, "  -h, --help    : this text\n");;
				mbus_client_usage();
				return 0;
		}
	}

	if (g_mbus_client != NULL) {
		fprintf(stderr, "mbus client already exists\n");
		goto bail;
	}

	rc = mbus_client_options_default(&options);
	if (rc != 0) {
		fprintf(stderr, "can not get default options\n");
		goto bail;
	}
	rc = mbus_client_options_from_argv(&options, argc, argv);
	if (rc != 0) {
		fprintf(stderr, "can not parse options\n");
		goto bail;
	}
	options.callbacks.connect     = mbus_client_callback_connect;
	options.callbacks.disconnect  = mbus_client_callback_disconnect;
	options.callbacks.message     = mbus_client_callback_message;
	options.callbacks.result      = mbus_client_callback_result;
	options.callbacks.routine     = mbus_client_callback_routine;
	options.callbacks.publish     = mbus_client_callback_publish;
	options.callbacks.subscribe   = mbus_client_callback_subscribe;
	options.callbacks.unsubscribe = mbus_client_callback_unsubscribe;
	options.callbacks.registered  = mbus_client_callback_registered;
	options.callbacks.unregistered= mbus_client_callback_unregistered;

	g_mbus_client = mbus_client_create(&options);
	if (g_mbus_client == NULL) {
		fprintf(stderr, "can not create client\n");
		goto bail;
	}

	free(_argv);
	return 0;
bail:	if (_argv != NULL) {
		free(_argv);
	}
	return -1;
}

static int command_destroy (int argc, char *argv[])
{
	int c;
	struct option long_options[] = {
		{ "help",	no_argument,	0,	'h' },
		{ NULL,		0,		NULL,	0 }
	};

	optind = 0;
	while ((c = getopt_long(argc, argv, "h", long_options, NULL)) != -1) {
		switch (c) {
			case 'h':
				fprintf(stdout, "destroy mbus client\n");
				fprintf(stdout, "  -h, --help    : this text\n");;
				return 0;
			default:
				fprintf(stderr, "invalid parameter\n");
				return -1;
		}
	}
	if (g_mbus_client == NULL) {
		return 0;
	}
	mbus_client_destroy(g_mbus_client);
	g_mbus_client = NULL;
	return 0;
}

static int command_connect (int argc, char *argv[])
{
	int rc;

	int c;
	struct option long_options[] = {
		{ "help",	no_argument,	0,	'h' },
		{ NULL,		0,		NULL,	0 }
	};

	optind = 0;
	while ((c = getopt_long(argc, argv, "h", long_options, NULL)) != -1) {
		switch (c) {
			case 'h':
				fprintf(stdout, "connect to mbus server\n");
				fprintf(stdout, "  -h, --help    : this text\n");;
				return 0;
			default:
				fprintf(stderr, "invalid parameter\n");
				return -1;
		}
	}

	if (g_mbus_client == NULL) {
		fprintf(stderr, "mbus client is invalid\n");
		return -1;
	}

	rc = mbus_client_connect(g_mbus_client);
	if (rc != 0) {
		fprintf(stderr, "can not connect client\n");
		return -1;
	}
	return 0;
}

static int command_disconnect (int argc, char *argv[])
{
	int rc;

	int c;
	struct option long_options[] = {
		{ "help",	no_argument,	0,	'h' },
		{ NULL,		0,		NULL,	0 }
	};

	optind = 0;
	while ((c = getopt_long(argc, argv, "h", long_options, NULL)) != -1) {
		switch (c) {
			case 'h':
				fprintf(stdout, "disconnect from mbus server\n");
				fprintf(stdout, "  -h, --help    : this text\n");;
				return 0;
			default:
				fprintf(stderr, "invalid parameter\n");
				return -1;
		}
	}

	if (g_mbus_client == NULL) {
		fprintf(stderr, "mbus client is invalid\n");
		return -1;
	}

	rc = mbus_client_disconnect(g_mbus_client);
	if (rc != 0) {
		fprintf(stderr, "can not disconnect client\n");
		return -1;
	}
	return 0;
}

static int command_get_state (int argc, char *argv[])
{
	int c;
	struct option long_options[] = {
		{ "help",	no_argument,	0,	'h' },
		{ NULL,		0,		NULL,	0 }
	};

	optind = 0;
	while ((c = getopt_long(argc, argv, "h", long_options, NULL)) != -1) {
		switch (c) {
			case 'h':
				fprintf(stdout, "get state\n");
				fprintf(stdout, "  -h, --help    : this text\n");;
				return 0;
			default:
				fprintf(stderr, "invalid parameter\n");
				return -1;
		}
	}

	if (g_mbus_client == NULL) {
		fprintf(stderr, "mbus client is invalid\n");
		return -1;
	}

	fprintf(stdout, "state: %s\n", mbus_client_state_string(mbus_client_get_state(g_mbus_client)));
	return 0;
}

static int command_subscribe (int argc, char *argv[])
{
	int rc;
	const char *source;
	const char *event;
	enum mbus_client_qos qos;
	int callback;
	int timeout;
	struct mbus_client_subscribe_options options;

	int c;
	struct option long_options[] = {
		{ "help",	no_argument,		0,	'h' },
		{ "source",	required_argument,	0,	's' },
		{ "event",	required_argument,	0,	'e' },
		{ "qos",	required_argument,	0,	'q' },
		{ "callback",	required_argument,	0,	'c' },
		{ "timeout",	required_argument,	0,	't' },
		{ NULL,		0,			NULL,	0 }
	};

	source = NULL;
	event = NULL;
	qos = mbus_client_qos_at_most_once;
	callback = 0;
	timeout = -1;

	optind = 0;
	while ((c = getopt_long(argc, argv, "s:e:q:c:t:h", long_options, NULL)) != -1) {
		switch (c) {
			case 's':
				source = optarg;
				break;
			case 'e':
				event = optarg;
				break;
			case 'q':
				qos = atoi(optarg);
				break;
			case 'c':
				callback = atoi(optarg);
				break;
			case 't':
				timeout = atoi(optarg);
				break;
			case 'h':
				fprintf(stdout, "subscribe to source/event\n");
				fprintf(stdout, "  -s, --source  : event source to subscribe (default: %s)\n", source);
				fprintf(stdout, "  -e, --event   : event identifier to subscribe (default: %s)\n", event);
				fprintf(stdout, "  -q, --qos     : event qos (default: %d)\n", qos);
				fprintf(stdout, "  -c, --callback: subscribe with callback (default: %d)\n", callback);
				fprintf(stdout, "  -t, --timeout : subscribe timeout (default: %d)\n", timeout);
				fprintf(stdout, "  -h, --help    : this text\n");;
				fprintf(stdout, "\n");
				fprintf(stdout, "special identifiers\n");
				fprintf(stdout, "  source all: %s\n", MBUS_METHOD_EVENT_SOURCE_ALL);
				fprintf(stdout, "  event all : %s\n", MBUS_METHOD_EVENT_IDENTIFIER_ALL);
				return 0;
			default:
				fprintf(stderr, "invalid parameter\n");
				return -1;
		}
	}

	if (g_mbus_client == NULL) {
		fprintf(stderr, "mbus client is invalid\n");
		return -1;
	}
	if (event == NULL) {
		fprintf(stderr, "event is invalid\n");
		return -1;
	}

	rc = mbus_client_subscribe_options_default(&options);
	if (rc != 0) {
		fprintf(stderr, "can not get default subscribe options\n");
		return -1;
	}
	options.source = source;
	options.event = event;
	options.qos = qos;
	if (callback != 0) {
		options.callback = mbus_client_callback_message_callback;
		options.context = NULL;
	}
	options.timeout = timeout;
	rc = mbus_client_subscribe_with_options(g_mbus_client, &options);
	if (rc != 0) {
		fprintf(stderr, "can not subscribe to source: %s, event: %s\n", source, event);
		return -1;
	}
	return 0;
}

static int command_unsubscribe (int argc, char *argv[])
{
	int rc;
	const char *source;
	const char *event;
	int timeout;
	struct mbus_client_unsubscribe_options options;

	int c;
	struct option long_options[] = {
		{ "help",	no_argument,		0,	'h' },
		{ "source",	required_argument,	0,	's' },
		{ "event",	required_argument,	0,	'e' },
		{ "timeout",	required_argument,	0,	't' },
		{ NULL,		0,			NULL,	0 }
	};

	source = NULL;
	event = NULL;
	timeout = -1;

	optind = 0;
	while ((c = getopt_long(argc, argv, "s:e:t:h", long_options, NULL)) != -1) {
		switch (c) {
			case 's':
				source = optarg;
				break;
			case 'e':
				event = optarg;
				break;
			case 't':
				timeout = atoi(optarg);
				break;
			case 'h':
				fprintf(stdout, "unsubscribe from source/event\n");
				fprintf(stdout, "  -s, --source : event source to unsubscribe (default: %s)\n", source);
				fprintf(stdout, "  -e, --event  : event identifier to unsubscribe (default: %s)\n", event);
				fprintf(stdout, "  -t, --timeout: unsubscribe timeout (default: %d)\n", timeout);
				fprintf(stdout, "  -h, --help   : this text\n");;
				fprintf(stdout, "\n");
				fprintf(stdout, "special identifiers\n");
				fprintf(stdout, "  source all: %s\n", MBUS_METHOD_EVENT_SOURCE_ALL);
				fprintf(stdout, "  event all : %s\n", MBUS_METHOD_EVENT_IDENTIFIER_ALL);
				return 0;
			default:
				fprintf(stderr, "invalid parameter\n");
				return -1;
		}
	}

	if (g_mbus_client == NULL) {
		fprintf(stderr, "mbus client is invalid\n");
		return -1;
	}
	if (event == NULL) {
		fprintf(stderr, "event is invalid\n");
		return -1;
	}

	rc = mbus_client_unsubscribe_options_default(&options);
	if (rc != 0) {
		fprintf(stderr, "can not get default unsubscribe options\n");
		return -1;
	}
	options.source = source;
	options.event = event;
	options.timeout = timeout;
	rc = mbus_client_unsubscribe_with_options(g_mbus_client, &options);
	if (rc != 0) {
		fprintf(stderr, "can not unsubscribe from source: %s, event: %s\n", source, event);
		return -1;
	}
	return 0;
}

static int command_publish (int argc, char *argv[])
{
	int rc;
	const char *destination;
	const char *event;
	const char *payload;
	int qos;
	int timeout;
	struct mbus_json *jpayload;

	struct mbus_client_publish_options options;

	int c;
	struct option long_options[] = {
		{ "help",	no_argument,		0,	'h' },
		{ "destination",required_argument,	0,	'd' },
		{ "event",	required_argument,	0,	'e' },
		{ "payload",	required_argument,	0,	'p' },
		{ "qos",	required_argument,	0,	'q' },
		{ "timeout",	required_argument,	0,	't' },
		{ NULL,		0,			NULL,	0 }
	};

	destination = NULL;
	event = NULL;
	payload = NULL;
	qos = mbus_client_qos_at_most_once;
	timeout = -1;

	jpayload = NULL;

	optind = 0;
	while ((c = getopt_long(argc, argv, "d:e:p:q:t:h", long_options, NULL)) != -1) {
		switch (c) {
			case 'd':
				destination = optarg;
				break;
			case 'e':
				event = optarg;
				break;
			case 'p':
				payload = optarg;
				break;
			case 'q':
				qos = atoi(optarg);
				break;
			case 't':
				timeout = atoi(optarg);
				break;
			case 'h':
				fprintf(stdout, "publish to a destination/event\n");
				fprintf(stdout, "  -d, --destination: event destination to publish (default: %s)\n", destination);
				fprintf(stdout, "  -e, --event      : event identifier to publish (default: %s)\n", event);
				fprintf(stdout, "  -p, --payload    : event payload to publish (default: %s)\n", payload);
				fprintf(stdout, "  -q, --qos        : event qos (default: %d)\n", qos);
				fprintf(stdout, "  -t, --timeout    : publish timeout (default: %d)\n", timeout);
				fprintf(stdout, "  -h, --help       : this text\n");;
				fprintf(stdout, "\n");
				fprintf(stdout, "special identifiers\n");
				fprintf(stdout, "  destination all        : %s\n", MBUS_METHOD_EVENT_DESTINATION_ALL);
				fprintf(stdout, "  destination subscribers: %s\n", MBUS_METHOD_EVENT_DESTINATION_SUBSCRIBERS);
				return 0;
			default:
				fprintf(stderr, "invalid parameter\n");
				goto bail;
		}
	}

	if (g_mbus_client == NULL) {
		fprintf(stderr, "mbus client is invalid\n");
		goto bail;
	}
	if (event == NULL) {
		fprintf(stderr, "event is invalid\n");
		goto bail;
	}
	if (payload != NULL) {
		jpayload = mbus_json_parse(payload);
		if (jpayload == NULL) {
			fprintf(stderr, "payload is invalid\n");
			goto bail;
		}
	}

	rc = mbus_client_publish_options_default(&options);
	if (rc != 0) {
		fprintf(stderr, "can not get default publish options\n");
		return -1;
	}
	options.destination = destination;
	options.event = event;
	options.payload = jpayload;
	options.qos = qos;
	options.timeout = timeout;
	rc = mbus_client_publish_with_options(g_mbus_client, &options);
	if (rc != 0) {
		fprintf(stderr, "can not publish to destination: %s, event: %s, payload: %s\n", destination, event, payload);
		goto bail;
	}

	if (jpayload != NULL) {
		mbus_json_delete(jpayload);
	}
	return 0;
bail:	if (jpayload != NULL) {
		mbus_json_delete(jpayload);
	}
	return -1;
}

static int command_command (int argc, char *argv[])
{
	int rc;
	const char *destination;
	const char *command;
	const char *payload;
	int callback;
	int timeout;
	struct mbus_json *jpayload;

	int c;
	struct option long_options[] = {
		{ "help",	no_argument,		0,	'h' },
		{ "destination",required_argument,	0,	'd' },
		{ "command",	required_argument,	0,	'c' },
		{ "payload",	required_argument,	0,	'p' },
		{ "callback",	required_argument,	0,	'b' },
		{ "timeout",	required_argument,	0,	't' },
		{ NULL,		0,			NULL,	0 }
	};

	destination = NULL;
	command = NULL;
	payload = NULL;
	callback = 0;
	timeout = -1;

	jpayload = NULL;

	optind = 0;
	while ((c = getopt_long(argc, argv, "d:c:p:b:t:h", long_options, NULL)) != -1) {
		switch (c) {
			case 'd':
				destination = optarg;
				break;
			case 'c':
				command = optarg;
				break;
			case 'p':
				payload = optarg;
				break;
			case 'b':
				callback = atoi(optarg);
				break;
			case 't':
				timeout = atoi(optarg);
				break;
			case 'h':
				fprintf(stdout, "command to a destination/event\n");
				fprintf(stdout, "  -d, --destination: destination to execute command (default: %s)\n", destination);
				fprintf(stdout, "  -c, --command    : command identifier to execute (default: %s)\n", command);
				fprintf(stdout, "  -p, --payload    : command payload (default: %s)\n", payload);
				fprintf(stdout, "  -b, --callback: register with callback (default: %d)\n", callback);
				fprintf(stdout, "  -t, --timeout    : command timeout (default: %d)\n", timeout);
				fprintf(stdout, "  -h, --help       : this text\n");;
				return 0;
			default:
				fprintf(stderr, "invalid parameter\n");
				goto bail;
		}
	}

	if (g_mbus_client == NULL) {
		fprintf(stderr, "mbus client is invalid\n");
		goto bail;
	}
	if (destination == NULL) {
		fprintf(stderr, "destination is invalid\n");
		goto bail;
	}
	if (command == NULL) {
		fprintf(stderr, "command is invalid\n");
		goto bail;
	}
	if (payload != NULL) {
		jpayload = mbus_json_parse(payload);
		if (jpayload == NULL) {
			fprintf(stderr, "payload is invalid\n");
			goto bail;
		}
	}

	struct mbus_client_command_options options;
	rc = mbus_client_command_options_default(&options);
	if (rc != 0) {
		mbus_errorf("can not get default command options");
		goto bail;
	}
	options.destination = destination;
	options.command = command;
	options.payload = jpayload;
	if (callback != 0) {
		options.callback = mbus_client_callback_command_callback;
		options.context = NULL;
	}
	options.timeout = timeout;
	rc = mbus_client_command_with_options_unlocked(g_mbus_client, &options);
	if (rc != 0) {
		fprintf(stderr, "can not execute destination: %s, command: %s, payload: %s\n", destination, command, payload);
		goto bail;
	}

	if (jpayload != NULL) {
		mbus_json_delete(jpayload);
	}
	return 0;
bail:	if (jpayload != NULL) {
		mbus_json_delete(jpayload);
	}
	return -1;
}

static int command_register (int argc, char *argv[])
{
	int rc;
	const char *command;
	int callback;
	int timeout;

	struct mbus_client_register_options options;

	int c;
	struct option long_options[] = {
		{ "help",	no_argument,		0,	'h' },
		{ "command",	required_argument,	0,	'c' },
		{ "callback",	required_argument,	0,	'b' },
		{ "timeout",	required_argument,	0,	't' },
		{ NULL,		0,			NULL,	0 }
	};

	command = NULL;
	callback = 0;
	timeout = -1;

	optind = 0;
	while ((c = getopt_long(argc, argv, "c:t:b:h", long_options, NULL)) != -1) {
		switch (c) {
			case 'c':
				command = optarg;
				break;
			case 'b':
				callback = atoi(optarg);
				break;
			case 't':
				timeout = atoi(optarg);
				break;
			case 'h':
				fprintf(stdout, "command to a destination/event\n");
				fprintf(stdout, "  -c, --command : command identifier to execute (default: %s)\n", command);
				fprintf(stdout, "  -b, --callback: register with callback (default: %d)\n", callback);
				fprintf(stdout, "  -t, --timeout : command timeout (default: %d)\n", timeout);
				fprintf(stdout, "  -h, --help    : this text\n");;
				return 0;
			default:
				fprintf(stderr, "invalid parameter\n");
				goto bail;
		}
	}

	if (g_mbus_client == NULL) {
		fprintf(stderr, "mbus client is invalid\n");
		goto bail;
	}
	if (command == NULL) {
		fprintf(stderr, "command is invalid\n");
		goto bail;
	}

	rc = mbus_client_register_options_default(&options);
	if (rc != 0) {
		fprintf(stderr, "can not get default register options\n");
		goto bail;
	}
	options.command = command;
	if (callback != 0) {
		options.callback = mbus_client_callback_routine_callback;
		options.context = NULL;
	}
	options.timeout = timeout;
	rc = mbus_client_register_with_options(g_mbus_client, &options);
	if (rc != 0) {
		fprintf(stderr, "can not register command: %s\n", command);
		goto bail;
	}

	return 0;
bail:	return -1;
}

static int command_unregister (int argc, char *argv[])
{
	int rc;
	const char *command;
	int timeout;

	struct mbus_client_unregister_options options;

	int c;
	struct option long_options[] = {
		{ "help",	no_argument,		0,	'h' },
		{ "command",	required_argument,	0,	'c' },
		{ "timeout",	required_argument,	0,	't' },
		{ NULL,		0,			NULL,	0 }
	};

	command = NULL;
	timeout = -1;

	optind = 0;
	while ((c = getopt_long(argc, argv, "c:t:h", long_options, NULL)) != -1) {
		switch (c) {
			case 'c':
				command = optarg;
				break;
			case 't':
				timeout = atoi(optarg);
				break;
			case 'h':
				fprintf(stdout, "command to a destination/event\n");
				fprintf(stdout, "  -c, --command : command identifier to execute (default: %s)\n", command);
				fprintf(stdout, "  -t, --timeout : command timeout (default: %d)\n", timeout);
				fprintf(stdout, "  -h, --help    : this text\n");;
				return 0;
			default:
				fprintf(stderr, "invalid parameter\n");
				goto bail;
		}
	}

	if (g_mbus_client == NULL) {
		fprintf(stderr, "mbus client is invalid\n");
		goto bail;
	}
	if (command == NULL) {
		fprintf(stderr, "command is invalid\n");
		goto bail;
	}

	rc = mbus_client_unregister_options_default(&options);
	if (rc != 0) {
		fprintf(stderr, "can not get unregister default options\n");
		goto bail;
	}
	options.command = command;
	options.timeout = timeout;
	rc = mbus_client_unregister_with_options(g_mbus_client, &options);
	if (rc != 0) {
		fprintf(stderr, "can not unregister command: %s\n", command);
		goto bail;
	}

	return 0;
bail:	return -1;
}

static struct command *commands[] = {
	&(struct command) {
		"quit",
		command_quit,
		"quit application"
	},
	&(struct command) {
		"create",
		command_create,
		"ccreate mbus client"
	},
	&(struct command) {
		"destroy",
		command_destroy,
		"destroy mbus client"
	},
	&(struct command) {
		"connect",
		command_connect,
		"connect to mbus server"
	},
	&(struct command) {
		"disconnect",
		command_disconnect,
		"disconnect from mbus server"
	},
	&(struct command) {
		"get-state",
		command_get_state,
		"get mbus client state"
	},
	&(struct command) {
		"subscribe",
		command_subscribe,
		"subscribe to  source/event"
	},
	&(struct command) {
		"unsubscribe",
		command_unsubscribe,
		"unsubscribe from source/event"
	},
	&(struct command) {
		"publish",
		command_publish,
		"publish an event w/o payload"
	},
	&(struct command) {
		"command",
		command_command,
		"execute command on destination/command w/o payload"
	},
	&(struct command) {
		"register",
		command_register,
		"register a command"
	},
	&(struct command) {
		"unregister",
		command_unregister,
		"unregister command"
	},
	NULL,
};

static int readline_process (char *command)
{
	int ret;
	char *b;
	char *p;
	int argc;
	char **argv;
	struct command **pc;

	if (command == NULL) {
		return 0;
	}
	if (strlen(command) == 0) {
		return 0;
	}

	{
		HIST_ENTRY *hist;
		hist = current_history();
		if (hist == NULL ||
		    strcmp(hist->line, command) != 0) {
			add_history(command);
		}
	}

	ret = 0;
	argc = 0;
	argv = NULL;
	b = strdup(command);
	p = b;

	while (*p) {
		while (isspace(*p)) {
			p++;
		}

		if (*p == '"' || *p == '\'') {
			char const delim = *p;
			char *const begin = ++p;

			while (*p && *p != delim) {
				p++;
			}
			if (*p) {
				*p++ = '\0';
				argv = (char **) realloc(argv, sizeof(char *) * (argc + 1));
				argv[argc] = begin;
				argc++;
			} else {
				goto out;
			}
		} else {
			char *const begin = p;

			while (*p && !isspace(*p)) {
				p++;
			}
			if (*p) {
				*p++ = '\0';
				argv = (char **) realloc(argv, sizeof(char *) * (argc + 1));
				argv[argc] = begin;
				argc++;
			} else if (p != begin) {
				argv = (char **) realloc(argv, sizeof(char *) * (argc + 1));
				argv[argc] = begin;
				argc++;
			}
		}
	}

	argv = (char **) realloc(argv, sizeof(char *) * (argc + 1));
	argv[argc] = NULL;

	if (strcmp(argv[0], "help") == 0) {
		fprintf(stdout, "mbus test client cli\n");
		fprintf(stdout, "\n");
		fprintf(stdout, "commands:\n");
		for (pc = commands; *pc; pc++) {
			int l;
			const char *h;
			const char *e;
			fprintf(stdout, "  %-15s - ", (*pc)->name);
			l = 0;
			h = (*pc)->help;
			while (h != NULL && *h != '\0') {
				e = strchr(h, '\n');
				if (e == NULL) {
					e = h + strlen(h);
				} else {
					e += 1;
				}
				if (l == 0) {
					fprintf(stdout, "%.*s", (int) (e - h), h);
				} else {
					fprintf(stdout, "  %-15s   %.*s", "", (int) (e - h), h);
				}
				h = e;
				l += 1;
			}
			fprintf(stdout, "\n");
		}
		fprintf(stdout, "\n");
		fprintf(stdout, "%-15s   - command specific help\n", "command --help");
	} else {
		for (pc = commands; *pc; pc++) {
			if (strcmp((*pc)->name, argv[0]) == 0) {
				ret = (*pc)->func(argc, &argv[0]);
				if (ret < 0) {
					fprintf(stderr, "command: %s failed: %s\n", argv[0], (ret == -2) ? "invalid arguments" : "internal error");
				}
				break;
			}
		}
		if (*pc == NULL) {
			fprintf(stderr, "command: %s not found\n", argv[0]);
		}
	}

out:
	free(argv);
	free(b);
	return ret;
}

static void process_line (char *line)
{
	if (line != NULL) {
		readline_strip(line);
		readline_process(line);
		free(line);
	}
}

int main (int argc, char *argv[])
{
	int rc;

	int c;
	int _argc;
	char **_argv;

	int timeout;
	int npollfd;
	struct pollfd pollfd[3];

	g_mbus_client = NULL;
	rl_initialize();

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
			case OPTION_HELP:
				usage();
				goto bail;
		}
	}

	g_running = 1;
	g_mbus_client = NULL;
	rl_callback_handler_install("client> ", process_line);

	while (g_running != 0) {
		timeout = -1;
		npollfd = 0;
		memset(pollfd, 0, sizeof(pollfd));

		pollfd[npollfd].fd = 0;
		pollfd[npollfd].events = POLLIN;
		pollfd[npollfd].revents = 0;
		npollfd += 1;

		if (g_mbus_client != NULL) {
			if (mbus_client_get_wakeup_fd(g_mbus_client) >= 0) {
				pollfd[npollfd].fd = mbus_client_get_wakeup_fd(g_mbus_client);
				pollfd[npollfd].events = mbus_client_get_wakeup_fd_events(g_mbus_client);
				pollfd[npollfd].revents = 0;
				npollfd += 1;
			}

			if (mbus_client_get_connection_fd(g_mbus_client) >= 0) {
				pollfd[npollfd].fd = mbus_client_get_connection_fd(g_mbus_client);
				pollfd[npollfd].events = mbus_client_get_connection_fd_events(g_mbus_client);
				pollfd[npollfd].revents = 0;
				npollfd += 1;
			}
			timeout = mbus_client_get_run_timeout(g_mbus_client);
		}

		rc = poll(pollfd, npollfd, timeout);
		if (rc < 0) {
			fprintf(stderr, "poll failed with: %d\n", rc);
			goto bail;
		}

		if (pollfd[0].revents & POLLIN) {
			rl_callback_read_char();
		}

		if (g_mbus_client != NULL) {
			rc = mbus_client_run(g_mbus_client, 0);
			if (rc != 0) {
				fprintf(stderr, "client run failed\n");
				goto bail;
			}
		}
	}

	rl_cleanup_after_signal();
	clear_history();

	mbus_client_destroy(g_mbus_client);
	free(_argv);
	return 0;
bail:	if (g_mbus_client != NULL) {
		mbus_client_destroy(g_mbus_client);
	}
	if (_argv != NULL) {
		free(_argv);
	}
	rl_cleanup_after_signal();
	clear_history();
	return -1;
}
