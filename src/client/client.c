
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
#include <stdint.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <sys/time.h>
#include <arpa/inet.h>

#if defined(SSL_ENABLE) && (SSL_ENABLE == 1)
#include <openssl/ssl.h>
#include <openssl/err.h>
#endif

#define MBUS_DEBUG_NAME	"mbus-client"

#include "mbus/json.h"
#include "mbus/debug.h"
#include "mbus/compress.h"
#include "mbus/buffer.h"
#include "mbus/clock.h"
#include "mbus/tailq.h"
#include "mbus/method.h"
#include "mbus/socket.h"
#include "mbus/server.h"
#include "mbus/version.h"
#include "client.h"

#define OPTION_HELP			0x100
#define OPTION_DEBUG_LEVEL		0x101
#define OPTION_SERVER_PROTOCOL		0x201
#define OPTION_SERVER_ADDRESS		0x202
#define OPTION_SERVER_PORT		0x203
#define OPTION_CLIENT_NAME		0x301
#define OPTION_CLIENT_COMMAND_TIMEOUT	0x302
#define OPTION_CLIENT_PUBLISH_TIMEOUT	0x303
#define OPTION_PING_INTERVAL		0x401
#define OPTION_PING_TIMEOUT		0x402
#define OPTION_PING_THRESHOLD		0x403
static struct option longopts[] = {
	{ "mbus-help",				no_argument,		NULL,	OPTION_HELP },
	{ "mbus-debug-level",			required_argument,	NULL,	OPTION_DEBUG_LEVEL },
	{ "mbus-server-protocol",		required_argument,	NULL,	OPTION_SERVER_PROTOCOL },
	{ "mbus-server-address"	,		required_argument,	NULL,	OPTION_SERVER_ADDRESS },
	{ "mbus-server-port",			required_argument,	NULL,	OPTION_SERVER_PORT },
	{ "mbus-client-name",			required_argument,	NULL,	OPTION_CLIENT_NAME },
	{ "mbus-client-command-timeout",		required_argument,	NULL,	OPTION_CLIENT_COMMAND_TIMEOUT },
	{ "mbus-client-publish-timeout",		required_argument,	NULL,	OPTION_CLIENT_PUBLISH_TIMEOUT },
	{ "mbus-ping-interval",			required_argument,	NULL,	OPTION_PING_INTERVAL },
	{ "mbus-ping-timeout",			required_argument,	NULL,	OPTION_PING_TIMEOUT },
	{ "mbus-ping-threshold",			required_argument,	NULL,	OPTION_PING_THRESHOLD },
	{ NULL,					0,			NULL,	0 },
};

TAILQ_HEAD(requests, request);
struct request {
	TAILQ_ENTRY(request) requests;
	char *string;
	void (*callback) (struct mbus_client *client, void *context, int result, struct mbus_json *payload);
	void *context;
};

TAILQ_HEAD(commands, command);
struct command {
	TAILQ_ENTRY(command) commands;
};

struct mbus_client {
	enum mbus_client_state state;
	struct mbus_socket *socket;
	struct requests requests;
	struct requests pendings;
	struct commands commands;
	struct mbus_buffer *incoming;
	struct mbus_buffer *outgoing;
	char *name;
	int command_timeout;
	int publish_timeout;
	int ping_interval;
	int ping_timeout;
	int ping_threshold;
	unsigned long ping_send_tsms;
	unsigned long pong_recv_tsms;
	int ping_wait_pong;
	int pong_missed_count;
	int sequence;
	pthread_mutex_t mutex;
};

struct mbus_client_message {
	struct mbus_json *payload;
};

static void command_destroy (struct command *command)
{
	if (command == NULL) {
		return;
	}
	free(command);
}

static void request_destroy (struct request *request)
{
	if (request == NULL) {
		return;
	}
	if (request->string != NULL) {
		free(request->string);
	}
	free(request);
}

static struct request * request_create (const char *type, const char *destination, const char *identifier, int sequence, struct mbus_json *payload, void (*callback) (struct mbus_client *client, void *context, int result, struct mbus_json *payload), void *context)
{
	int rc;
	struct mbus_json *json;
	struct request *request;
	json = NULL;
	request = NULL;
	if (type == NULL) {
		mbus_errorf("type is null");
		goto bail;
	}
	if (destination == NULL) {
		mbus_errorf("destination is null");
		goto bail;
	}
	if (identifier == NULL) {
		mbus_errorf("identifier is null");
		goto bail;
	}
	if (sequence < MBUS_METHOD_SEQUENCE_START ||
	    sequence > MBUS_METHOD_SEQUENCE_END) {
		mbus_errorf("sequence is invalid");
		goto bail;
	}
	json = mbus_json_create_object();
	if (json == NULL) {
		mbus_errorf("can not create json object");
		goto bail;
	}
	rc = mbus_json_add_string_to_object_cs(json, "type", type);
	if (rc != 0) {
		mbus_errorf("can not add string to json object");
		goto bail;
	}
	rc = mbus_json_add_string_to_object_cs(json, "destination", destination);
	if (rc != 0) {
		mbus_errorf("can not add string to json object");
		goto bail;
	}
	rc = mbus_json_add_string_to_object_cs(json, "identifier", identifier);
	if (rc != 0) {
		mbus_errorf("can not add string to json object");
		goto bail;
	}
	rc = mbus_json_add_number_to_object_cs(json, "sequence", sequence);
	if (rc != 0) {
		mbus_errorf("can not add number to json object");
		goto bail;
	}
	if (payload != NULL) {
		struct mbus_json *dup;
		dup = mbus_json_duplicate(payload, 1);
		if (dup == NULL) {
			mbus_errorf("can not duplicate json object");
			goto bail;
		}
		rc = mbus_json_add_item_to_object_cs(json, "payload", dup);
		if (rc != 0) {
			mbus_errorf("can not add item to json object");
			goto bail;
		}
	}
	request = malloc(sizeof(struct request));
	if (request == NULL) {
		mbus_errorf("can not allocate memory");
		goto bail;
	}
	memset(request, 0, sizeof(struct request));
	request->string = mbus_json_print_unformatted(json);
	if (request->string == NULL) {
		mbus_errorf("can not print json object");
		goto bail;
	}
	request->callback = callback;
	request->context = context;
	return request;
bail:	if (request != NULL) {
		request_destroy(request);
	}
	return NULL;
}

void mbus_client_usage (void)
{
	fprintf(stdout, "mbus client arguments:\n");
	fprintf(stdout, "  --mbus-debug-level           : debug level (default: %s)\n", mbus_debug_level_to_string(mbus_debug_level));
	fprintf(stdout, "  --mbus-server-protocol       : server protocol (default: %s)\n", MBUS_SERVER_PROTOCOL);
	fprintf(stdout, "  --mbus-server-address        : server address (default: %s)\n", MBUS_SERVER_ADDRESS);
	fprintf(stdout, "  --mbus-server-port           : server port (default: %d)\n", MBUS_SERVER_PORT);
	fprintf(stdout, "  --mbus-client-name           : client name (overrides api parameter)\n");
	fprintf(stdout, "  --mbus-client-command-timeout: client command timeout (default: %d)\n", MBUS_CLIENT_DEFAULT_COMMAND_TIMEOUT);
	fprintf(stdout, "  --mbus-client-publish-timeout: client publish timeout (default: %d)\n", MBUS_CLIENT_DEFAULT_PUBLISH_TIMEOUT);
	fprintf(stdout, "  --mbus-ping-interval         : ping interval (overrides api parameter) (default: %d)\n", MBUS_CLIENT_DEFAULT_PING_INTERVAL);
	fprintf(stdout, "  --mbus-ping-timeout          : ping timeout (overrides api parameter) (default: %d)\n", MBUS_CLIENT_DEFAULT_PING_TIMEOUT);
	fprintf(stdout, "  --mbus-ping-threshold        : ping threshold (overrides api parameter) (default: %d)\n", MBUS_CLIENT_DEFAULT_PING_THRESHOLD);
	fprintf(stdout, "  --mbus-help                  : this text\n");
}

int mbus_client_options_default (struct mbus_client_options *options)
{
	if (options == NULL) {
		mbus_errorf("options is invalid");
		goto bail;
	}
	memset(options, 0, sizeof(struct mbus_client_options));
	return 0;
bail:	return -1;
}

int mbus_client_options_from_argv (struct mbus_client_options *options, int argc, char *argv[])
{
	int ch;
	int o_optind;

	int a;
	char **_argv;

	_argv = NULL;
	o_optind = optind;

	if (options == NULL) {
		mbus_errorf("options is invalid");
		goto bail;
	}
	mbus_client_options_default(options);

	optind = 1;
	_argv = malloc(sizeof(char *) * argc);
	for (a = 0; a < argc; a++) {
		_argv[a] = argv[a];
	}

	while ((ch = getopt_long(argc, _argv, ":", longopts, NULL)) != -1) {
		switch (ch) {
			case OPTION_DEBUG_LEVEL:
				mbus_debug_level = mbus_debug_level_from_string(optarg);
				break;
			case OPTION_SERVER_PROTOCOL:
				options->server.protocol = optarg;
				break;
			case OPTION_SERVER_ADDRESS:
				options->server.address = optarg;
				break;
			case OPTION_SERVER_PORT:
				options->server.port = atoi(optarg);
				break;
			case OPTION_CLIENT_NAME:
				options->client.name = optarg;
				break;
			case OPTION_CLIENT_COMMAND_TIMEOUT:
				options->client.command_timeout = atoi(optarg);
				break;
			case OPTION_CLIENT_PUBLISH_TIMEOUT:
				options->client.publish_timeout = atoi(optarg);
				break;
			case OPTION_PING_INTERVAL:
				options->ping.interval = atoi(optarg);
				break;
			case OPTION_PING_TIMEOUT:
				options->ping.timeout = atoi(optarg);
				break;
			case OPTION_PING_THRESHOLD:
				options->ping.threshold = atoi(optarg);
				break;
			case OPTION_HELP:
				mbus_client_usage();
				goto bail;
		}
	}

	optind = o_optind;
	free(_argv);
	return 0;
bail:	if (_argv != NULL) {
		free(_argv);
	}
	return -1;
}

struct mbus_client * mbus_client_create (const struct mbus_client_options *_options)
{
	struct mbus_client *client;
	struct mbus_client_options options;

	client = NULL;
	mbus_client_options_default(&options);
	if (_options != NULL) {
		memcpy(&options, _options, sizeof(struct mbus_client_options));
	}

#if defined(SSL_ENABLE) && (SSL_ENABLE == 1)
	SSL_library_init();
	SSL_load_error_strings();
#endif

	if (options.client.name == NULL) {
		options.client.name = "";
	}
	if (options.client.command_timeout == 0) {
		options.client.command_timeout = MBUS_CLIENT_DEFAULT_COMMAND_TIMEOUT;
	}
	if (options.client.publish_timeout == 0) {
		options.client.publish_timeout = MBUS_CLIENT_DEFAULT_PUBLISH_TIMEOUT;
	}

	if (options.server.protocol == NULL) {
		options.server.protocol = MBUS_SERVER_PROTOCOL;
	}

	if (options.ping.interval == 0) {
		options.ping.interval = MBUS_CLIENT_DEFAULT_PING_INTERVAL;
	}
	if (options.ping.timeout == 0) {
		options.ping.timeout = MBUS_CLIENT_DEFAULT_PING_TIMEOUT;
	}
	if (options.ping.threshold == 0) {
		options.ping.threshold = MBUS_CLIENT_DEFAULT_PING_THRESHOLD;
	}
	if (options.ping.timeout > (options.ping.interval * 2) / 3) {
		options.ping.timeout = (options.ping.interval * 2) / 3;
	}

	if (strcmp(options.server.protocol, MBUS_SERVER_TCP_PROTOCOL) == 0) {
		if (options.server.port <= 0) {
			options.server.port = MBUS_SERVER_TCP_PORT;
		}
		if (options.server.address == NULL) {
			options.server.address = MBUS_SERVER_TCP_ADDRESS;
		}
	} else if (strcmp(options.server.protocol, MBUS_SERVER_UDS_PROTOCOL) == 0) {
		if (options.server.port <= 0) {
			options.server.port = MBUS_SERVER_UDS_PORT;
		}
		if (options.server.address == NULL) {
			options.server.address = MBUS_SERVER_UDS_ADDRESS;
		}
	} else if (strcmp(options.server.protocol, MBUS_SERVER_TCPS_PROTOCOL) == 0) {
		if (options.server.port <= 0) {
			options.server.port = MBUS_SERVER_TCPS_PORT;
		}
		if (options.server.address == NULL) {
			options.server.address = MBUS_SERVER_TCPS_ADDRESS;
		}
	} else if (strcmp(options.server.protocol, MBUS_SERVER_UDSS_PROTOCOL) == 0) {
		if (options.server.port <= 0) {
			options.server.port = MBUS_SERVER_UDSS_PORT;
		}
		if (options.server.address == NULL) {
			options.server.address = MBUS_SERVER_UDSS_ADDRESS;
		}
	} else {
		mbus_errorf("invalid server protocol: %s", options.server.protocol);
		goto bail;
	}

	mbus_infof("creating client: '%s'", options.client.name);
	mbus_infof("using mbus version '%s, %s'", mbus_git_commit(), mbus_git_revision());

	client = malloc(sizeof(struct mbus_client));
	if (client == NULL) {
		mbus_errorf("can not allocate memory");
		goto bail;
	}
	memset(client, 0, sizeof(struct mbus_client));

	pthread_mutex_init(&client->mutex,NULL);
	client->state = mbus_client_state_initial;
	client->socket = NULL;
	TAILQ_INIT(&client->requests);
	TAILQ_INIT(&client->pendings);
	TAILQ_INIT(&client->commands);
	client->incoming = mbus_buffer_create();
	if (client->incoming == NULL) {
		mbus_errorf("can not create incoming buffer");
		goto bail;
	}
	client->outgoing = mbus_buffer_create();
	if (client->outgoing == NULL) {
		mbus_errorf("can not create outgoing buffer");
		goto bail;
	}
	client->sequence = MBUS_METHOD_SEQUENCE_START;

	return client;
bail:	if (client != NULL) {
		mbus_client_destroy(client);
	}
	return NULL;
}

void mbus_client_destroy (struct mbus_client *client)
{
	struct request *request;
	struct request *nrequest;
	struct command *command;
	struct command *ncommand;
	if (client == NULL) {
		return;
	}
	if (client->socket != NULL) {
		mbus_socket_destroy(client->socket);
	}
	if (client->incoming != NULL) {
		mbus_buffer_destroy(client->incoming);
	}
	if (client->outgoing != NULL) {
		mbus_buffer_destroy(client->outgoing);
	}
	TAILQ_FOREACH_SAFE(request, &client->requests, requests, nrequest) {
		TAILQ_REMOVE(&client->requests, request, requests);
		request_destroy(request);
	}
	TAILQ_FOREACH_SAFE(request, &client->pendings, requests, nrequest) {
		TAILQ_REMOVE(&client->pendings, request, requests);
		request_destroy(request);
	}
	TAILQ_FOREACH_SAFE(command, &client->commands, commands, ncommand) {
		TAILQ_REMOVE(&client->commands, command, commands);
		command_destroy(command);
	}
	if (client->name != NULL) {
		free(client->name);
	}
	pthread_mutex_destroy(&client->mutex);
	free(client);
}

enum mbus_client_state mbus_client_state (struct mbus_client *client)
{
	enum mbus_client_state state;
	if (client == NULL) {
		mbus_errorf("client is invalid");
		goto bail;
	}
	pthread_mutex_lock(&client->mutex);
	state = client->state;
	pthread_mutex_unlock(&client->mutex);
	return state;
bail:	if (client != NULL) {
		pthread_mutex_unlock(&client->mutex);
	}
	return mbus_client_state_unknown;
}

const char * mbus_client_name (struct mbus_client *client)
{
	const char *name;
	if (client == NULL) {
		mbus_errorf("client is invalid");
		goto bail;
	}
	pthread_mutex_lock(&client->mutex);
	name = client->name;
	pthread_mutex_unlock(&client->mutex);
	return name;
bail:	if (client != NULL) {
		pthread_mutex_unlock(&client->mutex);
	}
	return NULL;
}

int mbus_client_connect (struct mbus_client *client)
{
	if (client == NULL) {
		mbus_errorf("client is invalid");
		goto bail;
	}
	pthread_mutex_lock(&client->mutex);
	client->state = mbus_client_state_connecting;
	pthread_mutex_unlock(&client->mutex);
bail:	if (client != NULL) {
		pthread_mutex_unlock(&client->mutex);
	}
	return -1;
}

int mbus_client_disconnect (struct mbus_client *client)
{
	if (client == NULL) {
		mbus_errorf("client is invalid");
		goto bail;
	}
	pthread_mutex_lock(&client->mutex);
	client->state = mbus_client_state_disconnecting;
	pthread_mutex_unlock(&client->mutex);
bail:	if (client != NULL) {
		pthread_mutex_unlock(&client->mutex);
	}
	return -1;
}

int mbus_client_subscribe (struct mbus_client *client, const char *source, const char *event)
{
	int rc;
	struct request *request;
	struct mbus_json *payload;
	request = NULL;
	payload = NULL;
	if (client == NULL) {
		mbus_errorf("client is invalid");
		goto bail;
	}
	pthread_mutex_lock(&client->mutex);
	if (source == NULL) {
		mbus_debugf("source is invalid, using: %s", MBUS_METHOD_EVENT_SOURCE_ALL);
		source = MBUS_METHOD_EVENT_SOURCE_ALL;
	}
	if (event == NULL) {
		mbus_errorf("event is invalid");
		goto bail;
	}
	payload = mbus_json_create_object();
	if (payload == NULL) {
		mbus_errorf("can not create json object");
		goto bail;
	}
	rc = mbus_json_add_string_to_object_cs(payload, "source", source);
	if (rc != 0) {
		mbus_errorf("can not add string to json object");
		goto bail;
	}
	rc = mbus_json_add_string_to_object_cs(payload, "event", event);
	if (rc != 0) {
		mbus_errorf("can not add string to json object");
		goto bail;
	}
	request = request_create(MBUS_METHOD_TYPE_COMMAND, MBUS_SERVER_NAME, MBUS_SERVER_COMMAND_SUBSCRIBE, client->sequence, payload, NULL, NULL);
	if (request == NULL) {
		mbus_errorf("can not create request");
		goto bail;
	}
	client->sequence += 1;
	if (client->sequence > MBUS_METHOD_SEQUENCE_END) {
		client->sequence = MBUS_METHOD_SEQUENCE_START;
	}
	TAILQ_INSERT_TAIL(&client->requests, request, requests);
	mbus_json_delete(payload);
	pthread_mutex_unlock(&client->mutex);
bail:	if (payload != NULL) {
		mbus_json_delete(payload);
	}
	if (client != NULL) {
		pthread_mutex_unlock(&client->mutex);
	}
	return -1;
}

int mbus_client_publish (struct mbus_client *client, const char *event, const struct mbus_json *payload)
{
	return mbus_client_publish_to(client, NULL, event, payload);
}

int mbus_client_publish_to (struct mbus_client *client, const char *destination, const char *event, const struct mbus_json *payload)
{
	(void) client;
	(void) destination;
	(void) event;
	(void) payload;
	return -1;
}

int mbus_client_publish_sync (struct mbus_client *client, const char *event, const struct mbus_json *payload)
{
	return mbus_client_publish_sync_to(client, NULL, event, payload);
}

int mbus_client_publish_sync_to (struct mbus_client *client, const char *destination, const char *event, const struct mbus_json *payload)
{
	(void) client;
	(void) destination;
	(void) event;
	(void) payload;
	return -1;
}

int mbus_client_command (struct mbus_client *client, const char *destination, const char *command, struct mbus_json *payload, struct mbus_json **result)
{
	return mbus_client_command_timeout(client, destination, command, payload, result, client->command_timeout);
}

int mbus_client_command_timeout (struct mbus_client *client, const char *destination, const char *command, struct mbus_json *payload, struct mbus_json **result, int milliseconds)
{
	(void) client;
	(void) destination;
	(void) command;
	(void) payload;
	(void) result;
	(void) milliseconds;
	return -1;
}

int mbus_client_run (struct mbus_client *client, int milliseconds)
{
	(void) client;
	(void) milliseconds;
	return -1;
}

const struct mbus_json * mbus_client_message_payload (struct mbus_client_message *message)
{
	if (message == NULL) {
		mbus_errorf("message is invalid");
		goto bail;
	}
	return message->payload;
bail:	return NULL;
}
