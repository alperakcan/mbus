
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
#define OPTION_CLIENT_CONNECT_INTERVAL	0x302
#define OPTION_CLIENT_COMMAND_TIMEOUT	0x303
#define OPTION_CLIENT_PUBLISH_TIMEOUT	0x304
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
	{ "mbus-client-connect-interval",	required_argument,	NULL,	OPTION_CLIENT_CONNECT_INTERVAL },
	{ "mbus-client-command-timeout",		required_argument,	NULL,	OPTION_CLIENT_COMMAND_TIMEOUT },
	{ "mbus-client-publish-timeout",		required_argument,	NULL,	OPTION_CLIENT_PUBLISH_TIMEOUT },
	{ "mbus-ping-interval",			required_argument,	NULL,	OPTION_PING_INTERVAL },
	{ "mbus-ping-timeout",			required_argument,	NULL,	OPTION_PING_TIMEOUT },
	{ "mbus-ping-threshold",			required_argument,	NULL,	OPTION_PING_THRESHOLD },
	{ NULL,					0,			NULL,	0 },
};

enum wakeup_reason {
	wakeup_reason_break,
};

enum method_type {
	method_type_unknown,
	method_type_status,
	method_type_event,
	method_type_command,
	method_type_result,
};

TAILQ_HEAD(requests, request);
struct request {
	TAILQ_ENTRY(request) requests;
	char *string;
	struct mbus_json *json;
	void (*callback) (struct mbus_client *client, void *context, struct mbus_client_message *message);
	void *context;
	int timeout;
};

TAILQ_HEAD(commands, command);
struct command {
	TAILQ_ENTRY(command) commands;
};

struct mbus_client {
	struct mbus_client_options *options;
	enum mbus_client_state state;
	struct mbus_socket *socket;
	struct requests requests;
	struct requests pendings;
	struct commands commands;
	struct mbus_buffer *incoming;
	struct mbus_buffer *outgoing;
	char *name;
	char *client_name;
	int connect_tsms;
	int ping_interval;
	int ping_timeout;
	int ping_threshold;
	unsigned long ping_send_tsms;
	unsigned long pong_recv_tsms;
	int ping_wait_pong;
	int pong_missed_count;
	enum mbus_compress_method compression;
	int sequence;
	int wakeup[2];
	pthread_mutex_t mutex;
};

enum mbus_client_message_type {
	mbus_client_message_type_status,
	mbus_client_message_type_event,
	mbus_client_message_type_command
};

struct mbus_client_message {
	enum mbus_client_message_type type;
	union  {
		struct {
			const struct mbus_json *payload;
		} status;
		struct {
			const struct mbus_json *payload;
		} event;
		struct {
			const struct mbus_json *request;
			const struct mbus_json *response;
		} command;
	} u;
};

static char * _strndup (const char *s, size_t n)
{
	char *result;
	size_t len = strnlen(s, n);
	result = (char *) malloc(len + 1);
	if (result == NULL) {
		return NULL;
	}
	result[len] = '\0';
	return (char *) memcpy(result, s, len);
}

static void command_destroy (struct command *command)
{
	if (command == NULL) {
		return;
	}
	free(command);
}

static int request_get_sequence (struct request *request)
{
	if (request == NULL) {
		return -1;
	}
	return mbus_json_get_int_value(request->json, "sequence", -1);
}

static const char * request_get_type (struct request *request)
{
	if (request == NULL) {
		return NULL;
	}
	return mbus_json_get_string_value(request->json, "type", NULL);
}

static const char * request_get_string (struct request *request)
{
	if (request == NULL) {
		return NULL;
	}
	return request->string;
}

static void request_destroy (struct request *request)
{
	if (request == NULL) {
		return;
	}
	if (request->string != NULL) {
		free(request->string);
	}
	if (request->json != NULL) {
		mbus_json_delete(request->json);
	}
	free(request);
}

static struct request * request_create (const char *type, const char *destination, const char *identifier, int sequence, const struct mbus_json *payload, void (*callback) (struct mbus_client *client, void *context, struct mbus_client_message *message), void *context, int timeout)
{
	int rc;
	struct request *request;
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
	request = malloc(sizeof(struct request));
	if (request == NULL) {
		mbus_errorf("can not allocate memory");
		goto bail;
	}
	memset(request, 0, sizeof(struct request));
	request->json = mbus_json_create_object();
	if (request->json == NULL) {
		mbus_errorf("can not create json object");
		goto bail;
	}
	rc = mbus_json_add_string_to_object_cs(request->json, "type", type);
	if (rc != 0) {
		mbus_errorf("can not add string to json object");
		goto bail;
	}
	rc = mbus_json_add_string_to_object_cs(request->json, "destination", destination);
	if (rc != 0) {
		mbus_errorf("can not add string to json object");
		goto bail;
	}
	rc = mbus_json_add_string_to_object_cs(request->json, "identifier", identifier);
	if (rc != 0) {
		mbus_errorf("can not add string to json object");
		goto bail;
	}
	rc = mbus_json_add_number_to_object_cs(request->json, "sequence", sequence);
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
		rc = mbus_json_add_item_to_object_cs(request->json, "payload", dup);
		if (rc != 0) {
			mbus_errorf("can not add item to json object");
			goto bail;
		}
	} else {
		struct mbus_json *dup;
		dup = mbus_json_create_object();
		if (dup == NULL) {
			mbus_errorf("can not duplicate json object");
			goto bail;
		}
		rc = mbus_json_add_item_to_object_cs(request->json, "payload", dup);
		if (rc != 0) {
			mbus_errorf("can not add item to json object");
			goto bail;
		}
	}
	request->string = mbus_json_print_unformatted(request->json);
	if (request->string == NULL) {
		mbus_errorf("can not print json object");
		goto bail;
	}
	request->callback = callback;
	request->context = context;
	request->timeout = timeout;
	return request;
bail:	if (request != NULL) {
		request_destroy(request);
	}
	return NULL;
}

static void mbus_client_command_create_response (struct mbus_client *client, void *context, struct mbus_client_message *message)
{
	const struct mbus_json *response;
	(void) context;
	mbus_client_lock(client);
	if (mbus_client_message_command_response_result(message) != 0) {
		mbus_errorf("client command create failed");
		goto bail;
	}
	response = mbus_client_message_command_response_payload(message);
	if (response == NULL) {
		mbus_errorf("message response is invalid");
		goto bail;
	}
	{
		const char *name;
		name = mbus_json_get_string_value(response, "name", NULL);
		if (name != NULL) {
			free(client->name);
			client->name = strdup(name);
			if (client->name == NULL) {
				mbus_errorf("can not allocate memory");
				goto bail;
			}
		}
	}
	{
		const char *compression;
		compression = mbus_json_get_string_value(response, "compression", "none");
		client->compression = mbus_compress_method_value(compression);
	}
	{
		client->ping_interval = mbus_json_get_int_value(response, "ping/interval", -1);
		client->ping_timeout = mbus_json_get_int_value(response, "ping/timeout", -1);
		client->ping_threshold = mbus_json_get_int_value(response, "ping/threshold", -1);
	}
	mbus_infof("created");
	mbus_infof("  name       : %s", client->name);
	mbus_infof("  compression: %s", mbus_compress_method_string(client->compression));
	mbus_infof("  ping");
	mbus_infof("    interval : %d", client->ping_interval);
	mbus_infof("    timeout  : %d", client->ping_timeout);
	mbus_infof("    threshold: %d", client->ping_threshold);
	client->state = mbus_client_state_created;
	if (client->options->callbacks.create != NULL) {
		mbus_client_unlock(client);
		client->options->callbacks.create(client, client->options->callbacks.context, mbus_client_create_status_success);
		mbus_client_lock(client);
	}
	mbus_client_unlock(client);
	return;
bail:	mbus_errorf("client command create failed");
	mbus_client_unlock(client);
	return;
}

static int mbus_client_command_create_request (struct mbus_client *client)
{
	int rc;
	struct mbus_json *payload;
	struct mbus_json *payload_ping;
	struct mbus_json *payload_compression;

	payload = NULL;
	payload_ping = NULL;
	payload_compression = NULL;

	payload = mbus_json_create_object();
	if (payload == NULL) {
		mbus_errorf("can not create json object");
		goto bail;
	}

	payload_ping = mbus_json_create_object();
	if (payload_ping == NULL) {
		mbus_errorf("can not create json object");
		goto bail;
	}
	rc = mbus_json_add_number_to_object_cs(payload_ping, "interval", client->options->ping.interval);
	if (rc != 0) {
		mbus_errorf("can not add number to json object");
		goto bail;
	}
	rc = mbus_json_add_number_to_object_cs(payload_ping, "timeout", client->options->ping.timeout);
	if (rc != 0) {
		mbus_errorf("can not add item to json object");
		goto bail;
	}
	rc = mbus_json_add_number_to_object_cs(payload_ping, "threshold", client->options->ping.threshold);
	if (rc != 0) {
		mbus_errorf("can not add item to json object");
		goto bail;
	}
	rc = mbus_json_add_item_to_object_cs(payload, "ping", payload_ping);
	if (rc != 0) {
		mbus_errorf("can not add item to json object");
		goto bail;
	}
	payload_ping = NULL;

	payload_compression = mbus_json_create_array();
	if (payload_compression == NULL) {
		mbus_errorf("can not create json array");
		goto bail;
	}
	rc = mbus_json_add_item_to_array(payload_compression, mbus_json_create_string("none"));
	if (rc != 0) {
		mbus_errorf("can not add item to json array");
		goto bail;
	}
#if defined(ZLIB_ENABLE) && (ZLIB_ENABLE == 1)
	rc = mbus_json_add_item_to_array(payload_compression, mbus_json_create_string("zlib"));
	if (rc != 0) {
		mbus_errorf("can not add item to json array");
		goto bail;
	}
#endif
	rc = mbus_json_add_item_to_object_cs(payload, "compression", payload_compression);
	if (rc != 0) {
		mbus_errorf("can not add item to json array");
		goto bail;
	}
	payload_compression = NULL;

	rc = mbus_client_command_unlocked(client, MBUS_SERVER_NAME, MBUS_SERVER_COMMAND_CREATE, payload, mbus_client_command_create_response, client);
	if (rc != 0) {
		mbus_errorf("can not queue client command");
		goto bail;
	}

	mbus_json_delete(payload);
	return 0;
bail:	if (payload != NULL) {
		mbus_json_delete(payload);
	}
	if (payload_ping != NULL) {
		mbus_json_delete(payload_ping);
	}
	if (payload_compression != NULL) {
		mbus_json_delete(payload_compression);
	}
	return -1;
}

static int mbus_client_run_connect (struct mbus_client *client)
{
	int rc;
	int ret;

	enum mbus_socket_type socket_type;
	enum mbus_socket_domain socket_domain;

	struct request *request;
	struct request *nrequest;
	struct command *command;
	struct command *ncommand;

	ret = mbus_client_connect_status_success;

	if (client->socket != NULL) {
		mbus_socket_shutdown(client->socket, mbus_socket_shutdown_rdwr);
		mbus_socket_destroy(client->socket);
		client->socket = NULL;
	}
	mbus_buffer_reset(client->incoming);
	mbus_buffer_reset(client->outgoing);
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
		client->name = NULL;
	}
	client->ping_interval = 0;
	client->ping_timeout = 0;
	client->ping_threshold = 0;
	client->ping_send_tsms = 0;
	client->pong_recv_tsms = 0;
	client->ping_wait_pong = 0;
	client->pong_missed_count = 0;

	client->compression = mbus_compress_method_none;

	if (strcmp(client->options->server.protocol, MBUS_SERVER_TCP_PROTOCOL) == 0) {
		if (client->options->server.port <= 0) {
			client->options->server.port = MBUS_SERVER_TCP_PORT;
		}
		if (client->options->server.address == NULL) {
			client->options->server.address = MBUS_SERVER_TCP_ADDRESS;
		}
		socket_domain = mbus_socket_domain_af_inet;
		socket_type = mbus_socket_type_sock_stream;
	} else if (strcmp(client->options->server.protocol, MBUS_SERVER_UDS_PROTOCOL) == 0) {
		if (client->options->server.port <= 0) {
			client->options->server.port = MBUS_SERVER_UDS_PORT;
		}
		if (client->options->server.address == NULL) {
			client->options->server.address = MBUS_SERVER_UDS_ADDRESS;
		}
		socket_domain = mbus_socket_domain_af_unix;
		socket_type = mbus_socket_type_sock_stream;
	} else if (strcmp(client->options->server.protocol, MBUS_SERVER_TCPS_PROTOCOL) == 0) {
		if (client->options->server.port <= 0) {
			client->options->server.port = MBUS_SERVER_TCPS_PORT;
		}
		if (client->options->server.address == NULL) {
			client->options->server.address = MBUS_SERVER_TCPS_ADDRESS;
		}
		socket_domain = mbus_socket_domain_af_inet;
		socket_type = mbus_socket_type_sock_stream;
	} else if (strcmp(client->options->server.protocol, MBUS_SERVER_UDSS_PROTOCOL) == 0) {
		if (client->options->server.port <= 0) {
			client->options->server.port = MBUS_SERVER_UDSS_PORT;
		}
		if (client->options->server.address == NULL) {
			client->options->server.address = MBUS_SERVER_UDSS_ADDRESS;
		}
		socket_domain = mbus_socket_domain_af_unix;
		socket_type = mbus_socket_type_sock_stream;
	} else {
		mbus_errorf("invalid server protocol: %s", client->options->server.protocol);
		ret = mbus_client_connect_status_invalid_protocol;
		goto bail;
	}

	mbus_infof("connecting to server: '%s:%s:%d'", client->options->server.protocol, client->options->server.address, client->options->server.port);
	client->socket = mbus_socket_create(socket_domain, socket_type, mbus_socket_protocol_any);
	if (client->socket == NULL) {
		mbus_errorf("can not create event socket");
		ret = mbus_client_connect_status_internal_error;
		goto bail;
	}
	rc = mbus_socket_set_reuseaddr(client->socket, 1);
	if (rc != 0) {
		mbus_errorf("can not reuse event");
		ret = mbus_client_connect_status_internal_error;
		goto bail;
	}
	if (socket_domain == mbus_socket_domain_af_inet &&
	    socket_type == mbus_socket_type_sock_stream) {
		mbus_socket_set_keepalive(client->socket, 1);
#if 0
		mbus_socket_set_keepcnt(client->socket, 5);
		mbus_socket_set_keepidle(client->socket, 180);
		mbus_socket_set_keepintvl(client->socket, 60);
#endif
	}
	rc = mbus_socket_connect(client->socket, client->options->server.address, client->options->server.port);
	if (rc != 0) {
		mbus_errorf("can not connect to server: '%s:%s:%d', rc: %d, %s", client->options->server.protocol, client->options->server.address, client->options->server.port, rc, strerror(-rc));
		if (rc == -ECONNREFUSED) {
			ret = mbus_client_connect_status_connection_refused;
		} else {
			ret = mbus_client_connect_status_server_unavailable;
		}
		goto bail;
	}
	rc = mbus_socket_set_blocking(client->socket, 0);
	if (rc != 0) {
		mbus_errorf("can not set socket to nonblocking");
		ret = mbus_client_connect_status_internal_error;
		goto bail;
	}

	return ret;
bail:	return ret;
}

static int mbus_client_message_handle_result (struct mbus_client *client, const struct mbus_json *json)
{
	int sequence;
	struct request *request;
	struct request *nrequest;
	struct mbus_client_message message;

	sequence = mbus_json_get_int_value(json, "sequence", -1);

	TAILQ_FOREACH_SAFE(request, &client->pendings, requests, nrequest) {
		if (request_get_sequence(request) == sequence) {
			TAILQ_REMOVE(&client->pendings, request, requests);
			break;
		}
	}
	if (request == NULL) {
		mbus_errorf("sequence: %d is invalid", sequence);
		goto bail;
	}

	if (request->callback != NULL) {
		message.type = mbus_client_message_type_command;
		message.u.command.request = request->json;
		message.u.command.response = json;
		mbus_client_unlock(client);
		request->callback(client, request->context, &message);
		mbus_client_lock(client);
	}

	request_destroy(request);
	return 0;
bail:	return -1;
}

static int mbus_client_message_handle_event (struct mbus_client *client, const struct mbus_json *json)
{
	const char *source;
	const char *identifier;
	struct mbus_client_message message;

	source = mbus_json_get_string_value(json, "source", NULL);
	if (source == NULL) {
		mbus_errorf("source is invalid");
		goto bail;
	}
	identifier = mbus_json_get_string_value(json, "identifier", NULL);
	if (identifier == NULL) {
		mbus_errorf("identifier is invalid");
		goto bail;
	}

	if (strcasecmp(MBUS_SERVER_NAME, source) == 0 &&
	    strcasecmp(MBUS_SERVER_EVENT_PONG, identifier) == 0) {
		client->ping_wait_pong = 0;
		client->pong_recv_tsms = mbus_clock_get();
		client->pong_missed_count = 0;
	}

	if (client->options->callbacks.message != NULL) {
		message.type = mbus_client_message_type_event;
		message.u.event.payload = json;
		mbus_client_unlock(client);
		client->options->callbacks.message(client, client->options->callbacks.context, &message);
		mbus_client_lock(client);
	}

	return 0;
bail:	return -1;
}

static void mbus_client_options_destroy (struct mbus_client_options *options)
{
	if (options == NULL) {
		return;
	}
	if (options->server.protocol != NULL) {
		free(options->server.protocol);
	}
	if (options->server.address != NULL) {
		free(options->server.address);
	}
	if (options->client.name != NULL) {
		free(options->client.name);
	}
	free(options);
}

static struct mbus_client_options * mbus_client_options_duplicate (const struct mbus_client_options *options)
{
	int rc;
	struct mbus_client_options *duplicate;
	duplicate = malloc(sizeof(struct mbus_client_options));
	if (duplicate == NULL) {
		mbus_errorf("can not allocate memory");
		goto bail;
	}
	memset(duplicate, 0, sizeof(struct mbus_client_options));
	if (options == NULL) {
		rc = mbus_client_options_default(duplicate);
		if (rc != 0) {
			mbus_errorf("can not set default options");
			goto bail;
		}
	} else {
		if (options->server.protocol != NULL) {
			duplicate->server.protocol = strdup(options->server.protocol);
			if (duplicate->server.protocol == NULL) {
				mbus_errorf("can not allocate memory");
				goto bail;
			}
		}
		if (options->server.address != NULL) {
			duplicate->server.address = strdup(options->server.address);
			if (duplicate->server.address == NULL) {
				mbus_errorf("can not allocate memory");
				goto bail;
			}
		}
		duplicate->server.port = options->server.port;

		if (options->client.name != NULL) {
			duplicate->client.name = strdup(options->client.name);
			if (duplicate->client.name == NULL) {
				mbus_errorf("can not allocate memory");
				goto bail;
			}
		}
		duplicate->client.connect_interval = options->client.connect_interval;
		duplicate->client.command_timeout = options->client.command_timeout;
		duplicate->client.publish_timeout = options->client.publish_timeout;

		duplicate->ping.interval = options->ping.interval;
		duplicate->ping.timeout = options->ping.timeout;
		duplicate->ping.threshold = options->ping.threshold;

		duplicate->callbacks.connect = options->callbacks.connect;
		duplicate->callbacks.disconnect = options->callbacks.disconnect;
		duplicate->callbacks.create = options->callbacks.create;
		duplicate->callbacks.message = options->callbacks.message;
		duplicate->callbacks.publish = options->callbacks.publish;
		duplicate->callbacks.subscribe = options->callbacks.subscribe;
		duplicate->callbacks.unsubscribe = options->callbacks.unsubscribe;
		duplicate->callbacks.context = options->callbacks.context;
	}
	return duplicate;
bail:	if (duplicate != NULL) {
		mbus_client_options_destroy(duplicate);
	}
	return NULL;
}

void mbus_client_usage (void)
{
	fprintf(stdout, "mbus client arguments:\n");
	fprintf(stdout, "  --mbus-debug-level            : debug level (default: %s)\n", mbus_debug_level_to_string(mbus_debug_level));
	fprintf(stdout, "  --mbus-server-protocol        : server protocol (default: %s)\n", MBUS_SERVER_PROTOCOL);
	fprintf(stdout, "  --mbus-server-address         : server address (default: %s)\n", MBUS_SERVER_ADDRESS);
	fprintf(stdout, "  --mbus-server-port            : server port (default: %d)\n", MBUS_SERVER_PORT);
	fprintf(stdout, "  --mbus-client-name            : client name (overrides api parameter)\n");
	fprintf(stdout, "  --mbus-client-connect-interval: client connect interval (default: %d)\n", MBUS_CLIENT_DEFAULT_CONNECT_INTERVAL);
	fprintf(stdout, "  --mbus-client-command-timeout : client command timeout (default: %d)\n", MBUS_CLIENT_DEFAULT_COMMAND_TIMEOUT);
	fprintf(stdout, "  --mbus-client-publish-timeout : client publish timeout (default: %d)\n", MBUS_CLIENT_DEFAULT_PUBLISH_TIMEOUT);
	fprintf(stdout, "  --mbus-ping-interval          : ping interval (overrides api parameter) (default: %d)\n", MBUS_CLIENT_DEFAULT_PING_INTERVAL);
	fprintf(stdout, "  --mbus-ping-timeout           : ping timeout (overrides api parameter) (default: %d)\n", MBUS_CLIENT_DEFAULT_PING_TIMEOUT);
	fprintf(stdout, "  --mbus-ping-threshold         : ping threshold (overrides api parameter) (default: %d)\n", MBUS_CLIENT_DEFAULT_PING_THRESHOLD);
	fprintf(stdout, "  --mbus-help                   : this text\n");
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
			case OPTION_CLIENT_CONNECT_INTERVAL:
				options->client.connect_interval = atoi(optarg);
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
	int rc;
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
	if (options.client.connect_interval == 0) {
		options.client.connect_interval = MBUS_CLIENT_DEFAULT_CONNECT_INTERVAL;
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
	client->wakeup[0] = -1;
	client->wakeup[1] = -1;
	TAILQ_INIT(&client->requests);
	TAILQ_INIT(&client->pendings);
	TAILQ_INIT(&client->commands);

	client->options = mbus_client_options_duplicate(&options);
	if (client->options == NULL) {
		mbus_errorf("can not create options");
		goto bail;
	}
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
	client->compression = mbus_compress_method_none;

	rc = pipe(client->wakeup);
	if (rc != 0) {
		mbus_errorf("can not create wakeup");
		goto bail;
	}

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
	if (client->options != NULL) {
		mbus_client_options_destroy(client->options);
	}
	if (client->wakeup[0] >= 0) {
		close(client->wakeup[0]);
	}
	if (client->wakeup[1] >= 0) {
		close(client->wakeup[1]);
	}
	pthread_mutex_destroy(&client->mutex);
	free(client);
}

int mbus_client_lock (struct mbus_client *client)
{
	if (client == NULL) {
		mbus_errorf("client is invalid");
		return -1;
	}
	pthread_mutex_lock(&client->mutex);
	return 0;
}

int mbus_client_unlock (struct mbus_client *client)
{
	if (client == NULL) {
		mbus_errorf("client is invalid");
		return -1;
	}
	pthread_mutex_unlock(&client->mutex);
	return 0;
}

enum mbus_client_state mbus_client_state (struct mbus_client *client)
{
	enum mbus_client_state state;
	if (client == NULL) {
		mbus_errorf("client is invalid");
		goto bail;
	}
	mbus_client_lock(client);
	state = client->state;
	mbus_client_unlock(client);
	return state;
bail:	if (client != NULL) {
		mbus_client_unlock(client);
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
	mbus_client_lock(client);
	name = client->name;
	mbus_client_unlock(client);
	return name;
bail:	if (client != NULL) {
		mbus_client_unlock(client);
	}
	return NULL;
}

int mbus_client_connect (struct mbus_client *client)
{
	if (client == NULL) {
		mbus_errorf("client is invalid");
		goto bail;
	}
	mbus_client_lock(client);
	client->state = mbus_client_state_connecting;
	mbus_client_unlock(client);
	return 0;
bail:	if (client != NULL) {
		mbus_client_unlock(client);
	}
	return -1;
}

int mbus_client_disconnect (struct mbus_client *client)
{
	if (client == NULL) {
		mbus_errorf("client is invalid");
		goto bail;
	}
	mbus_client_lock(client);
	client->state = mbus_client_state_disconnecting;
	mbus_client_unlock(client);
bail:	if (client != NULL) {
		mbus_client_unlock(client);
	}
	return -1;
}

int mbus_client_subscribe (struct mbus_client *client, const char *source, const char *event)
{
	int rc;
	struct mbus_json *payload;
	payload = NULL;
	if (client == NULL) {
		mbus_errorf("client is invalid");
		goto bail;
	}
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
	rc = mbus_client_command(client, MBUS_SERVER_NAME, MBUS_SERVER_COMMAND_SUBSCRIBE, payload, NULL, NULL);
	if (rc != 0) {
		mbus_errorf("can not execute command");
		goto bail;
	}
	mbus_json_delete(payload);
	return 0;
bail:	if (payload != NULL) {
		mbus_json_delete(payload);
	}
	return -1;
}

int mbus_client_publish (struct mbus_client *client, const char *event, const struct mbus_json *payload)
{
	return mbus_client_publish_to(client, NULL, event, payload);
}

int mbus_client_publish_unlocked (struct mbus_client *client, const char *event, const struct mbus_json *payload)
{
	return mbus_client_publish_to_unlocked(client, NULL, event, payload);
}

int mbus_client_publish_to (struct mbus_client *client, const char *destination, const char *event, const struct mbus_json *payload)
{
	int rc;
	if (client == NULL) {
		mbus_errorf("client is invalid");
		goto bail;
	}
	mbus_client_lock(client);
	rc = mbus_client_publish_to_unlocked(client, destination, event, payload);
	mbus_client_unlock(client);
	return rc;
bail:	if (client != NULL) {
		mbus_client_unlock(client);
	}
	return -1;
}

int mbus_client_publish_to_unlocked (struct mbus_client *client, const char *destination, const char *event, const struct mbus_json *payload)
{
	struct request *request;
	request = NULL;
	if (client == NULL) {
		mbus_errorf("client is invalid");
		goto bail;
	}
	if (destination == NULL) {
		mbus_errorf("destination is invalid");
		goto bail;
	}
	if (event == NULL) {
		mbus_errorf("command is invalid");
		goto bail;
	}
	request = request_create(MBUS_METHOD_TYPE_EVENT, destination, event, client->sequence, payload, NULL, NULL, 0);
	if (request == NULL) {
		mbus_errorf("can not create request");
		goto bail;
	}
	client->sequence += 1;
	if (client->sequence > MBUS_METHOD_SEQUENCE_END) {
		client->sequence = MBUS_METHOD_SEQUENCE_START;
	}
	TAILQ_INSERT_TAIL(&client->requests, request, requests);
	return 0;
bail:	return -1;
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

int mbus_client_command (struct mbus_client *client, const char *destination, const char *command, const struct mbus_json *payload, void (*callback) (struct mbus_client *client, void *context, struct mbus_client_message *message), void *context)
{
	return mbus_client_command_timeout(client, destination, command, payload, callback, context, client->options->client.command_timeout);
}

int mbus_client_command_unlocked (struct mbus_client *client, const char *destination, const char *command, const struct mbus_json *payload, void (*callback) (struct mbus_client *client, void *context, struct mbus_client_message *message), void *context)
{
	return mbus_client_command_timeout_unlocked(client, destination, command, payload, callback, context, client->options->client.command_timeout);
}

int mbus_client_command_timeout (struct mbus_client *client, const char *destination, const char *command, const struct mbus_json *payload, void (*callback) (struct mbus_client *client, void *context, struct mbus_client_message *message), void *context, int timeout)
{
	int rc;
	if (client == NULL) {
		mbus_errorf("client is invalid");
		goto bail;
	}
	mbus_client_lock(client);
	rc = mbus_client_command_timeout_unlocked(client, destination, command, payload, callback, context, timeout);
	mbus_client_unlock(client);
	return rc;
bail:	if (client != NULL) {
		mbus_client_unlock(client);
	}
	return -1;
}

int mbus_client_command_timeout_unlocked (struct mbus_client *client, const char *destination, const char *command, const struct mbus_json *payload, void (*callback) (struct mbus_client *client, void *context, struct mbus_client_message *message), void *context, int timeout)
{
	struct request *request;
	request = NULL;
	if (client == NULL) {
		mbus_errorf("client is invalid");
		goto bail;
	}
	if (destination == NULL) {
		mbus_errorf("destination is invalid");
		goto bail;
	}
	if (command == NULL) {
		mbus_errorf("command is invalid");
		goto bail;
	}
	request = request_create(MBUS_METHOD_TYPE_COMMAND, destination, command, client->sequence, payload, callback, context, timeout);
	if (request == NULL) {
		mbus_errorf("can not create request");
		goto bail;
	}
	client->sequence += 1;
	if (client->sequence > MBUS_METHOD_SEQUENCE_END) {
		client->sequence = MBUS_METHOD_SEQUENCE_START;
	}
	TAILQ_INSERT_TAIL(&client->requests, request, requests);
	return 0;
bail:	return -1;
}

int mbus_client_run (struct mbus_client *client, int timeout)
{
	int rc;
	unsigned long current;

	struct request *request;
	struct request *nrequest;

	int read_rc;
	int write_rc;
	struct pollfd pollfds[2];

	if (client == NULL) {
		mbus_errorf("client is invalid");
		goto bail;
	}
	current = mbus_clock_get();

	mbus_client_lock(client);

	if (client->state == mbus_client_state_connecting) {
		if (client->options->client.connect_interval <= 0 ||
		    mbus_clock_after(current, client->connect_tsms + client->options->client.connect_interval)) {
			mbus_debugf("connecting");
			client->connect_tsms = mbus_clock_get();
			rc = mbus_client_run_connect(client);
			if (rc != 0) {
				mbus_errorf("can not connect client");
				if (client->options->callbacks.connect != NULL) {
					mbus_client_unlock(client);
					client->options->callbacks.connect(client, client->options->callbacks.context, rc);
					mbus_client_lock(client);
				}
				if (client->options->client.connect_interval > 0) {

				} else {
					goto bail;
				}
			} else {
				client->state = mbus_client_state_connected;
			}
		}
	} else if (client->state == mbus_client_state_connected) {
		mbus_debugf("connected");
		if (client->options->callbacks.connect != NULL) {
			mbus_client_unlock(client);
			client->options->callbacks.connect(client, client->options->callbacks.context, mbus_client_connect_status_success);
			mbus_client_lock(client);
		}
		client->state = mbus_client_state_creating;
		rc = mbus_client_command_create_request(client);
		if (rc != 0) {
			mbus_errorf("can not create client");
			goto bail;
		}
		mbus_debugf("creating");
	} else if (client->state == mbus_client_state_creating) {
	} else if (client->state == mbus_client_state_created) {
	} else if (client->state == mbus_client_state_disconnected) {
		if (client->options->client.connect_interval > 0) {
			client->state = mbus_client_state_connecting;
		} else {
			goto bail;
		}
	} else {
		mbus_errorf("client state: %d is invalid", client->state);
		goto bail;
	}

	if (client->state == mbus_client_state_connected ||
	    client->state == mbus_client_state_creating ||
	    client->state == mbus_client_state_created) {
		if (client->ping_interval > 0) {
			if (mbus_clock_after(current, client->ping_send_tsms + client->ping_interval)) {
				mbus_debugf("send ping current: %ld, %ld, %d, %d", current, client->ping_send_tsms, client->ping_interval, client->ping_timeout);
				client->ping_send_tsms = current;
				client->pong_recv_tsms = 0;
				client->ping_wait_pong = 1;
				rc = mbus_client_publish_to_unlocked(client, MBUS_SERVER_NAME, MBUS_SERVER_EVENT_PING, NULL);
				if (rc != 0) {
					mbus_errorf("can not publish ping");
					goto bail;
				}
			}
			if (client->ping_wait_pong != 0 &&
			    client->ping_send_tsms != 0 &&
			    client->pong_recv_tsms == 0 &&
			    mbus_clock_after(current, client->ping_send_tsms + client->ping_timeout)) {
				mbus_infof("ping timeout: %ld, %ld, %d", current, client->ping_send_tsms, client->ping_timeout);
				client->ping_wait_pong = 0;
				client->pong_missed_count += 1;
			}
			if (client->pong_missed_count > client->ping_threshold) {
				mbus_errorf("missed too many pongs, %d > %d", client->pong_missed_count, client->ping_threshold);
				goto bail;
			}
		}

		TAILQ_FOREACH_SAFE(request, &client->requests, requests, nrequest) {
			mbus_debugf("request to server: %s, %s", mbus_compress_method_string(client->compression), request_get_string(request));
			rc = mbus_buffer_push_string(client->outgoing, client->compression, request_get_string(request));
			if (rc != 0) {
				mbus_errorf("can not push string to outgoing");
				goto bail;
			}
			TAILQ_REMOVE(&client->requests, request, requests);
			if (strcasecmp(request_get_type(request), MBUS_METHOD_TYPE_EVENT) == 0) {
				request_destroy(request);
			} else {
				TAILQ_INSERT_TAIL(&client->pendings, request, requests);
			}
		}

		pollfds[0].events = POLLIN;
		pollfds[0].revents = 0;
		pollfds[0].fd = mbus_socket_get_fd(client->socket);
		if (mbus_buffer_length(client->outgoing) > 0) {
			pollfds[0].events |= POLLOUT;
		}
		pollfds[1].events = POLLIN;
		pollfds[1].revents = 0;
		pollfds[1].fd = client->wakeup[0];
		mbus_client_unlock(client);
		rc = poll(pollfds, 2, timeout);
		mbus_client_lock(client);
		if (rc == 0) {
			goto out;
		}
		if (rc < 0) {
			mbus_errorf("poll error");
			goto bail;
		}

		if (pollfds[1].revents & POLLIN) {
			enum wakeup_reason reason;
			rc = read(pollfds[1].fd, &reason, sizeof(reason));
			if (rc != sizeof(reason)) {
				mbus_errorf("can not read wakeup reason");
				goto bail;
			}
		}
		if (pollfds[0].revents & POLLIN) {
			rc = mbus_buffer_reserve(client->incoming, mbus_buffer_length(client->incoming) + 1024);
			if (rc != 0) {
				mbus_errorf("can not reserve client buffer");
				goto bail;
			}
			read_rc = read(mbus_socket_get_fd(client->socket),
				       mbus_buffer_base(client->incoming) + mbus_buffer_length(client->incoming),
				       mbus_buffer_size(client->incoming) - mbus_buffer_length(client->incoming));
			if (read_rc <= 0) {
				if (errno == EINTR) {
				} else if (errno == EAGAIN) {
				} else if (errno == EWOULDBLOCK) {
				} else {
					mbus_errorf("connection reset by server");
					client->state = mbus_client_state_disconnected;
					if (client->options->callbacks.disconnect != NULL) {
						mbus_client_unlock(client);
						client->options->callbacks.disconnect(client, client->options->callbacks.context, mbus_client_disconnect_status_connection_closed);
						mbus_client_lock(client);
					}
					goto out;
				}
			} else {
				rc = mbus_buffer_set_length(client->incoming, mbus_buffer_length(client->incoming) + read_rc);
				if (rc != 0) {
					mbus_errorf("can not set buffer length: %d + %d / %d", mbus_buffer_length(client->incoming), read_rc, mbus_buffer_size(client->incoming));
					goto bail;
				}
			}
		}

		if (pollfds[0].revents & POLLOUT) {
			if (mbus_buffer_length(client->outgoing) <= 0) {
				mbus_errorf("logic error");
				goto bail;
			}
			write_rc = write(mbus_socket_get_fd(client->socket), mbus_buffer_base(client->outgoing), mbus_buffer_length(client->outgoing));
			if (write_rc <= 0) {
				if (errno == EINTR) {
				} else if (errno == EAGAIN) {
				} else if (errno == EWOULDBLOCK) {
				} else {
					mbus_errorf("can not write string to client");
					goto bail;
				}
			} else {
				rc = mbus_buffer_shift(client->outgoing, write_rc);
				if (rc != 0) {
					mbus_errorf("can not set buffer length");
					goto bail;
				}
			}
		}

		{
			uint8_t *ptr;
			uint8_t *end;
			uint8_t *data;
			uint32_t expected;
			uint32_t uncompressed;

			char *string;
			struct mbus_json *json;
			const char *type;

			while (mbus_buffer_length(client->incoming) >= 4) {
				json = NULL;
				string = NULL;
				mbus_debugf("incoming size: %d, length: %d", mbus_buffer_size(client->incoming), mbus_buffer_length(client->incoming));
				ptr = mbus_buffer_base(client->incoming);
				end = ptr + mbus_buffer_length(client->incoming);
				if (end - ptr < 4) {
					break;
				}
				memcpy(&expected, ptr, sizeof(expected));
				ptr += sizeof(expected);
				expected = ntohl(expected);
				mbus_debugf("expected: %d", expected);
				if (end - ptr < (int32_t) expected) {
					break;
				}
				data = ptr;
				if (client->compression != mbus_compress_method_none) {
					int uncompressedlen;
					memcpy(&uncompressed, ptr, sizeof(uncompressed));
					uncompressed = ntohl(uncompressed);
					mbus_debugf("uncompressed: %d", uncompressed);
					uncompressedlen = uncompressed;
					rc = mbus_uncompress_data(client->compression, (void **) &data, &uncompressedlen, ptr + sizeof(uncompressed), expected - sizeof(uncompressed));
					if (rc != 0) {
						mbus_errorf("can not uncompress data");
						goto incoming_bail;
					}
					if (uncompressedlen != (int) uncompressed) {
						mbus_errorf("can not uncompress data");
						goto incoming_bail;
					}
				} else {
					data = ptr;
					uncompressed = expected;
				}
				mbus_debugf("message: '%.*s'", uncompressed, data);
				string = _strndup((char *) data, uncompressed);
				if (string == NULL) {
					mbus_errorf("can not allocate memory");
					goto incoming_bail;
				}
				rc = mbus_buffer_shift(client->incoming, sizeof(uint32_t) + expected);
				if (rc != 0) {
					mbus_errorf("can not shift in");
					goto incoming_bail;
				}
				json = mbus_json_parse(string);
				if (json == NULL) {
					mbus_errorf("can not parse message: '%s'", string);
					goto incoming_bail;
				}
				type = mbus_json_get_string_value(json, "type", NULL);
				if (type == NULL) {
					mbus_errorf("message type is invalid");
					goto incoming_bail;
				}
				if (strcasecmp(type, MBUS_METHOD_TYPE_RESULT) == 0) {
					rc = mbus_client_message_handle_result(client, json);
					if (rc != 0) {
						mbus_errorf("can not handle message result");
						goto incoming_bail;
					}
				} else if (strcasecmp(type, MBUS_METHOD_TYPE_EVENT) == 0) {
					rc = mbus_client_message_handle_event(client, json);
					if (rc != 0) {
						mbus_errorf("can not handle message event");
						goto incoming_bail;
					}
				} else {
					mbus_errorf("message type: %s unknown", type);
					goto incoming_bail;
				}
				mbus_json_delete(json);
				free(string);
				if (data != ptr) {
					free(data);
				}
				continue;
incoming_bail:			if (json != NULL) {
					mbus_json_delete(json);
				}
				if (string != NULL) {
					free(string);
				}
				if (data != ptr) {
					free(data);
				}
				goto bail;
			}
		}
	} else if (client->state == mbus_client_state_connecting) {
		if (client->options->client.connect_interval > 0) {
			pollfds[0].events = POLLIN;
			pollfds[0].revents = 0;
			pollfds[0].fd = client->wakeup[0];
			if (timeout <= 0) {
				timeout = client->options->client.connect_interval;
			} else {
				timeout = (timeout < client->options->client.connect_interval) ? (timeout) : (client->options->client.connect_interval);
			}
			mbus_client_unlock(client);
			rc = poll(pollfds, 1, timeout);
			mbus_client_lock(client);
			if (rc == 0) {
				goto out;
			}
			if (rc < 0) {
				mbus_errorf("poll error");
				goto bail;
			}

			if (pollfds[1].revents & POLLIN) {
				enum wakeup_reason reason;
				rc = read(pollfds[1].fd, &reason, sizeof(reason));
				if (rc != sizeof(reason)) {
					mbus_errorf("can not read wakeup reason");
					goto bail;
				}
			}
		}
	}

out:	mbus_client_unlock(client);
	return 0;
bail:	if (client != NULL) {
		mbus_client_unlock(client);
	}
	return -1;
}

const char * mbus_client_message_event_source (struct mbus_client_message *message)
{
	if (message == NULL) {
		mbus_errorf("message is invalid");
		goto bail;
	}
	if (message->type != mbus_client_message_type_event) {
		mbus_errorf("message is invalid");
		goto bail;
	}
	return mbus_json_get_string_value(message->u.event.payload, "source", NULL);
bail:	return NULL;
}

const char * mbus_client_message_event_identifier (struct mbus_client_message *message)
{
	if (message == NULL) {
		mbus_errorf("message is invalid");
		goto bail;
	}
	if (message->type != mbus_client_message_type_event) {
		mbus_errorf("message is invalid");
		goto bail;
	}
	return mbus_json_get_string_value(message->u.event.payload, "identifier", NULL);
bail:	return NULL;
}

const struct mbus_json * mbus_client_message_event_payload (struct mbus_client_message *message)
{
	if (message == NULL) {
		mbus_errorf("message is invalid");
		goto bail;
	}
	if (message->type != mbus_client_message_type_event) {
		mbus_errorf("message is invalid");
		goto bail;
	}
	return mbus_json_get_object(message->u.event.payload, "payload");
bail:	return NULL;
}

const char * mbus_client_message_status_source (struct mbus_client_message *message)
{
	if (message == NULL) {
		mbus_errorf("message is invalid");
		goto bail;
	}
	if (message->type != mbus_client_message_type_status) {
		mbus_errorf("message is invalid");
		goto bail;
	}
	return mbus_json_get_string_value(message->u.status.payload, "source", NULL);
bail:	return NULL;
}

const char * mbus_client_message_status_identifier (struct mbus_client_message *message)
{
	if (message == NULL) {
		mbus_errorf("message is invalid");
		goto bail;
	}
	if (message->type != mbus_client_message_type_status) {
		mbus_errorf("message is invalid");
		goto bail;
	}
	return mbus_json_get_string_value(message->u.status.payload, "identifier", NULL);
bail:	return NULL;
}

const struct mbus_json * mbus_client_message_status_payload (struct mbus_client_message *message)
{
	if (message == NULL) {
		mbus_errorf("message is invalid");
		goto bail;
	}
	if (message->type != mbus_client_message_type_status) {
		mbus_errorf("message is invalid");
		goto bail;
	}
	return mbus_json_get_object(message->u.status.payload, "payload");
bail:	return NULL;
}

const struct mbus_json * mbus_client_message_command_request_payload (struct mbus_client_message *message)
{
	if (message == NULL) {
		mbus_errorf("message is invalid");
		goto bail;
	}
	if (message->type != mbus_client_message_type_command) {
		mbus_errorf("message is invalid");
		goto bail;
	}
	return mbus_json_get_object(message->u.command.request, "payload");
bail:	return NULL;
}

const struct mbus_json * mbus_client_message_command_response_payload (struct mbus_client_message *message)
{
	if (message == NULL) {
		mbus_errorf("message is invalid");
		goto bail;
	}
	if (message->type != mbus_client_message_type_command) {
		mbus_errorf("message is invalid");
		goto bail;
	}
	return mbus_json_get_object(message->u.command.response, "payload");
bail:	return NULL;
}

int mbus_client_message_command_response_result (struct mbus_client_message *message)
{
	if (message == NULL) {
		mbus_errorf("message is invalid");
		goto bail;
	}
	if (message->type != mbus_client_message_type_command) {
		mbus_errorf("message is invalid");
		goto bail;
	}
	return mbus_json_get_int_value(message->u.command.response, "result", -1);
bail:	return -1;
}
