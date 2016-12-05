
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
#include <libwebsockets.h>

#define MBUS_DEBUG_NAME	"mbus-server"

#include "mbus/debug.h"
#include "mbus/tailq.h"
#include "mbus/json.h"
#include "mbus/method.h"
#include "mbus/socket.h"
#include "server.h"

struct method {
	TAILQ_ENTRY(method) methods;
	struct {
		const char *type;
		const char *source;
		const char *destination;
		const char *identifier;
		int sequence;
		struct mbus_json *payload;
		struct mbus_json *json;
		char *string;
	} request;
	struct {
		struct mbus_json *payload;
		struct mbus_json *json;
		char *string;
	} result;
	struct client *source;
};
TAILQ_HEAD(methods, method);

struct subscription {
	TAILQ_ENTRY(subscription) subscriptions;
	char *source;
	char *event;
};
TAILQ_HEAD(subscriptions, subscription);

struct command {
	TAILQ_ENTRY(command) commands;
	char *identifier;
};
TAILQ_HEAD(commands, command);

struct websocket_client_data_buffer {
	unsigned int length;
	unsigned int size;
	uint8_t *buffer;
};

struct websocket_client_data {
	struct lws *wsi;
	struct client *client;
	struct {
		struct websocket_client_data_buffer in;
		struct websocket_client_data_buffer out;
	} buffer;
};

enum client_status {
	client_status_connected		= 0x00000001,
};

enum client_link {
	client_link_unknown,
	client_link_tcp,
	client_link_uds,
	client_link_websocket,
};

struct client {
	TAILQ_ENTRY(client) clients;
	char *name;
	enum client_link link;
	enum client_status status;
	struct mbus_socket *socket;
	struct subscriptions subscriptions;
	struct commands commands;
	struct methods requests;
	struct methods results;
	struct methods events;
	struct methods waits;
	int ssequence;
	int esequence;
};
TAILQ_HEAD(clients, client);

struct mbus_server {
	struct {
		struct {
			int enabled;
			const char *address;
			unsigned int port;
			struct mbus_socket *socket;
		} uds;
		struct {
			int enabled;
			const char *address;
			unsigned int port;
			struct mbus_socket *socket;
		} tcp;
		struct {
			int enabled;
			const char *address;
			unsigned int port;
			struct lws_context *context;
			struct {
				unsigned int length;
				unsigned int size;
				struct pollfd *pollfds;
			} pollfds;
		} websocket;
		struct {
			unsigned int length;
			unsigned int size;
			struct pollfd *pollfds;
		} pollfds;
	} socket;
	struct clients clients;
	struct methods methods;
	int running;
};

static struct mbus_server *g_server;

#define OPTION_HELP			0x100
#define OPTION_DEBUG_LEVEL		0x101
#define OPTION_SERVER_TCP_ENABLE	0x102
#define OPTION_SERVER_TCP_ADDRESS	0x103
#define OPTION_SERVER_TCP_PORT		0x104
#define OPTION_SERVER_UDS_ENABLE	0x105
#define OPTION_SERVER_UDS_ADDRESS	0x106
#define OPTION_SERVER_UDS_PORT		0x107
#define OPTION_SERVER_WEBSOCKET_ENABLE	0x108
#define OPTION_SERVER_WEBSOCKET_ADDRESS	0x109
#define OPTION_SERVER_WEBSOCKET_PORT	0x110
static struct option longopts[] = {
	{ "help",				no_argument,		NULL,	OPTION_HELP },
	{ "mbus-help",				no_argument,		NULL,	OPTION_HELP },
	{ "mbus-debug-level",			required_argument,	NULL,	OPTION_DEBUG_LEVEL },
	{ "mbus-server-tcp-enable",		required_argument,	NULL,	OPTION_SERVER_TCP_ENABLE },
	{ "mbus-server-tcp-address",		required_argument,	NULL,	OPTION_SERVER_TCP_ADDRESS },
	{ "mbus-server-tcp-port",		required_argument,	NULL,	OPTION_SERVER_TCP_PORT },
	{ "mbus-server-uds-enable",		required_argument,	NULL,	OPTION_SERVER_UDS_ENABLE },
	{ "mbus-server-uds-address",		required_argument,	NULL,	OPTION_SERVER_UDS_ADDRESS },
	{ "mbus-server-uds-port",		required_argument,	NULL,	OPTION_SERVER_UDS_PORT },
	{ "mbus-server-websocket-enable",	required_argument,	NULL,	OPTION_SERVER_WEBSOCKET_ENABLE },
	{ "mbus-server-websocket-address",	required_argument,	NULL,	OPTION_SERVER_WEBSOCKET_ADDRESS },
	{ "mbus-server-websocket-port",		required_argument,	NULL,	OPTION_SERVER_WEBSOCKET_PORT },
	{ NULL,					0,			NULL,	0 },
};

static void usage (void)
{
	fprintf(stdout, "mbus server arguments:\n");
	fprintf(stdout, "  --mbus-debug-level             : debug level (default: %s)\n", mbus_debug_level_to_string(mbus_debug_level));
	fprintf(stdout, "  --mbus-server-tcp-enable       : server tcp enable (default: %d)\n", MBUS_SERVER_TCP_ENABLE);
	fprintf(stdout, "  --mbus-server-tcp-address      : server tcp address (default: %s)\n", MBUS_SERVER_TCP_ADDRESS);
	fprintf(stdout, "  --mbus-server-tcp-port         : server tcp port (default: %d)\n", MBUS_SERVER_TCP_PORT);
	fprintf(stdout, "  --mbus-server-uds-enable       : server uds enable (default: %d)\n", MBUS_SERVER_UDS_ENABLE);
	fprintf(stdout, "  --mbus-server-uds-address      : server uds address (default: %s)\n", MBUS_SERVER_UDS_ADDRESS);
	fprintf(stdout, "  --mbus-server-uds-port         : server uds port (default: %d)\n", MBUS_SERVER_UDS_PORT);
	fprintf(stdout, "  --mbus-server-websocket-enable : server websocket enable (default: %d)\n", MBUS_SERVER_WEBSOCKET_ENABLE);
	fprintf(stdout, "  --mbus-server-websocket-address: server websocket address (default: %s)\n", MBUS_SERVER_WEBSOCKET_ADDRESS);
	fprintf(stdout, "  --mbus-server-websocket-port   : server websocket port (default: %d)\n", MBUS_SERVER_WEBSOCKET_PORT);
	fprintf(stdout, "  --mbus-help                    : this text\n");
}


static const char * command_get_identifier (struct command *command)
{
	if (command == NULL) {
		return NULL;
	}
	return command->identifier;
}

static void command_destroy (struct command *command)
{
	if (command == NULL) {
		return;
	}
	if (command->identifier != NULL) {
		free(command->identifier);
	}
	free(command);
}

static struct command * command_create (const char *identifier)
{
	struct command *command;
	command = NULL;
	if (identifier == NULL) {
		mbus_errorf("identifier is null");
		goto bail;
	}
	command = malloc(sizeof(struct command));
	if (command == NULL) {
		mbus_errorf("can not allocate memory");
		goto bail;
	}
	memset(command, 0, sizeof(struct command));
	command->identifier = strdup(identifier);
	if (command->identifier == NULL) {
		mbus_errorf("can not allocate memory");
		goto bail;
	}
	return command;
bail:	command_destroy(command);
	return NULL;
}

static const char * subscription_get_source (struct subscription *subscription)
{
	if (subscription == NULL) {
		return NULL;
	}
	return subscription->source;
}

static const char * subscription_get_event (struct subscription *subscription)
{
	if (subscription == NULL) {
		return NULL;
	}
	return subscription->event;
}

static void subscription_destroy (struct subscription *subscription)
{
	if (subscription == NULL) {
		return;
	}
	if (subscription->source != NULL) {
		free(subscription->source);
	}
	if (subscription->event != NULL) {
		free(subscription->event);
	}
	free(subscription);
}

static struct subscription * subscription_create (const char *source, const char *event)
{
	struct subscription *subscription;
	subscription = NULL;
	if (source == NULL) {
		mbus_errorf("source is null");
		goto bail;
	}
	if (event == NULL) {
		mbus_errorf("event is null");
		goto bail;
	}
	subscription = malloc(sizeof(struct subscription));
	if (subscription == NULL) {
		mbus_errorf("can not allocate memory");
		goto bail;
	}
	memset(subscription, 0, sizeof(struct subscription));
	subscription->source = strdup(source);
	subscription->event = strdup(event);
	if ((subscription->source == NULL) ||
	    (subscription->event == NULL)) {
		mbus_errorf("can not allocate memory");
		goto bail;
	}
	return subscription;
bail:	subscription_destroy(subscription);
	return NULL;
}

static const char * method_get_request_source (struct method *method)
{
	if (method == NULL) {
		return NULL;
	}
	return method->request.source;
}

static const char * method_get_request_destination (struct method *method)
{
	if (method == NULL) {
		return NULL;
	}
	return method->request.destination;
}

static const char * method_get_request_type (struct method *method)
{
	if (method == NULL) {
		return NULL;
	}
	return method->request.type;
}

static const char * method_get_request_identifier (struct method *method)
{
	if (method == NULL) {
		return NULL;
	}
	return method->request.identifier;
}

static struct mbus_json * method_get_request_payload (struct method *method)
{
	if (method == NULL) {
		return NULL;
	}
	return method->request.payload;
}

static int method_set_result_code (struct method *method, int code)
{
	if (method == NULL) {
		return -1;
	}
	mbus_json_add_number_to_object_cs(method->result.json, "result", code);
	return 0;
}

static int method_add_result_payload (struct method *method, const char *name, struct mbus_json *payload)
{
	if (method == NULL) {
		return -1;
	}
	mbus_json_add_item_to_object_cs(method->result.payload, name, payload);
	return 0;
}

static int method_set_result_payload (struct method *method, struct mbus_json *payload)
{
	if (method == NULL) {
		return -1;
	}
	mbus_json_delete_item_from_object(method->result.json, "payload");
	mbus_json_add_item_to_object_cs(method->result.json, "payload", payload);
	return 0;
}

static char * method_get_result_string (struct method *method)
{
	if (method == NULL) {
		return NULL;
	}
	if (method->result.string != NULL) {
		free(method->result.string);
	}
	method->result.string = mbus_json_print_unformatted(method->result.json);
	return method->result.string;
}

static char * method_get_request_string (struct method *method)
{
	if (method == NULL) {
		return NULL;
	}
	if (method->request.string != NULL) {
		free(method->request.string);
	}
	method->request.string = mbus_json_print_unformatted(method->request.json);
	return method->request.string;
}

static int method_get_request_sequence (struct method *method)
{
	if (method == NULL) {
		return -1;
	}
	return method->request.sequence;
}

static struct client * method_get_source (struct method *method)
{
	if (method == NULL) {
		return NULL;
	}
	return method->source;
}

static void method_destroy (struct method *method)
{
	if (method == NULL) {
		return;
	}
	if (method->request.json != NULL) {
		mbus_json_delete(method->request.json);
	}
	if (method->request.string != NULL) {
		free(method->request.string);
	}
	if (method->result.json != NULL) {
		mbus_json_delete(method->result.json);
	}
	if (method->result.string != NULL) {
		free(method->result.string);
	}
	free(method);
}

static struct method * method_create_from_string (struct client *source, const char *string)
{
	struct method *method;
	method = NULL;
#if 0
	if (source == NULL) {
		mbus_errorf("source is null");
		goto bail;
	}
#endif
	if (string == NULL) {
		mbus_errorf("string is null");
		goto bail;
	}
	method = malloc(sizeof(struct method));
	if (method == NULL) {
		mbus_errorf("can not allocate memory");
		goto bail;
	}
	memset(method, 0, sizeof(struct method));
	method->request.json = mbus_json_parse(string);
	if (method->request.json == NULL) {
		mbus_errorf("can not parse method");
		goto bail;
	}
	method->request.type = mbus_json_get_string_value(method->request.json, "type");
	method->request.source = mbus_json_get_string_value(method->request.json, "source");
	method->request.destination = mbus_json_get_string_value(method->request.json, "destination");
	method->request.identifier = mbus_json_get_string_value(method->request.json, "identifier");
	method->request.sequence = mbus_json_get_int_value(method->request.json, "sequence");
	method->request.payload = mbus_json_get_object_item(method->request.json, "payload");
	if ((method->request.source == NULL) ||
	    (method->request.destination == NULL) ||
	    (method->request.type == NULL) ||
	    (method->request.identifier == NULL) ||
	    (method->request.sequence == -1) ||
	    (method->request.payload == NULL)) {
		mbus_errorf("invalid method");
		goto bail;
	}
	method->result.json = mbus_json_create_object();
	if (method->result.json == NULL) {
		mbus_errorf("can not create result");
		goto bail;
	}
	method->result.payload = mbus_json_create_object();
	if (method->result.payload == NULL) {
		mbus_errorf("can not create result payload");
		goto bail;
	}
	mbus_json_add_string_to_object_cs(method->result.json, "type", MBUS_METHOD_TYPE_RESULT);
	mbus_json_add_number_to_object_cs(method->result.json, "sequence", method->request.sequence);
	mbus_json_add_item_to_object_cs(method->result.json, "payload", method->result.payload);
	method->source = source;
	return method;
bail:	method_destroy(method);
	return NULL;
}

static struct method * method_create (const char *type, const char *source, const char *destination, const char *identifier, int sequence, const struct mbus_json *payload)
{
	struct method *method;
	method = NULL;
	if (type == NULL) {
		mbus_errorf("type is null");
		goto bail;
	}
	if (source == NULL) {
		mbus_errorf("source is null");
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
	if (sequence < 0) {
		mbus_errorf("sequence is invalid");
		goto bail;
	}
	if (payload == NULL) {
		mbus_errorf("payload is null");
		goto bail;
	}
	method = malloc(sizeof(struct method));
	if (method == NULL) {
		mbus_errorf("can not allocate memory");
		goto bail;
	}
	memset(method, 0, sizeof(struct method));
	method->request.sequence = sequence;
	method->request.payload = mbus_json_duplicate((struct mbus_json *) payload, 1);
	if (method->request.payload == NULL) {
		mbus_errorf("can not create method payload");
		goto bail;
	}
	method->request.json = mbus_json_create_object();
	if (method->request.json == NULL) {
		mbus_errorf("can not create method object");
		mbus_json_delete(method->request.payload);
		method->request.payload = NULL;
		goto bail;
	}
	mbus_json_add_string_to_object_cs(method->request.json, "type", type);
	mbus_json_add_string_to_object_cs(method->request.json, "source", source);
	mbus_json_add_string_to_object_cs(method->request.json, "destination", destination);
	mbus_json_add_string_to_object_cs(method->request.json, "identifier", identifier);
	mbus_json_add_number_to_object_cs(method->request.json, "sequence", sequence);
	mbus_json_add_item_to_object_cs(method->request.json, "payload", method->request.payload);
	return method;
bail:	if (method != NULL) {
		method_destroy(method);
	}
	return NULL;
}

static int client_set_socket (struct client *client, struct mbus_socket *socket)
{
	if (client == NULL) {
		mbus_errorf("client is null");
		return -1;
	}
	if (socket == NULL) {
		if (client->socket != NULL) {
			if (client->link == client_link_tcp ||
			    client->link == client_link_uds) {
				mbus_socket_destroy(client->socket);
			}
			client->socket = NULL;
		}
	} else {
		if (client->socket != NULL) {
			mbus_errorf("client event is not null");
			return -1;
		}
		client->socket = socket;
	}
	return 0;
}

static struct mbus_socket * client_get_socket (struct client *client)
{
	if (client == NULL) {
		mbus_errorf("client is null");
		return NULL;
	}
	return client->socket;
}

enum client_link client_get_link (struct client *client)
{
	if (client == NULL) {
		mbus_errorf("client is null");
		return client_link_unknown;
	}
	return client->link;
}

static int client_set_status (struct client *client, enum client_status status)
{
	if (client == NULL) {
		mbus_errorf("client is null");
		return -1;
	}
	client->status = status;
	return 0;
}

static enum client_status client_get_status (struct client *client)
{
	if (client == NULL) {
		mbus_errorf("client is null");
		return 0;
	}
	return client->status;
}

static const char * client_get_name (struct client *client)
{
	if (client == NULL) {
		mbus_errorf("client is null");
		return NULL;
	}
	return client->name;
}

static int client_add_subscription (struct client *client, const char *source, const char *event)
{
	struct subscription *subscription;
	subscription = NULL;
	if (client == NULL) {
		mbus_errorf("client is null");
		goto bail;
	}
	if (source == NULL) {
		mbus_errorf("source is null");
		goto bail;
	}
	if (event == NULL) {
		mbus_errorf("event is null");
		goto bail;
	}
	TAILQ_FOREACH(subscription, &client->subscriptions, subscriptions) {
		if ((strcmp(subscription_get_source(subscription), source) == 0) &&
		    (strcmp(subscription_get_event(subscription), event) == 0)) {
			goto out;
		}
	}
	subscription = subscription_create(source, event);
	if (subscription == NULL) {
		mbus_errorf("can not create subscription");
		goto bail;
	}
	TAILQ_INSERT_TAIL(&client->subscriptions, subscription, subscriptions);
	mbus_infof("subscribed '%s' to '%s', '%s'", client_get_name(client), source, event);
out:	return 0;
bail:	subscription_destroy(subscription);
	return -1;
}

static int client_add_command (struct client *client, const char *identifier)
{
	struct command *command;
	command = NULL;
	if (client == NULL) {
		mbus_errorf("client is null");
		goto bail;
	}
	if (identifier == NULL) {
		mbus_errorf("identifier is null");
		goto bail;
	}
	TAILQ_FOREACH(command, &client->commands, commands) {
		if (strcmp(command_get_identifier(command), identifier) == 0) {
			goto out;
		}
	}
	command = command_create(identifier);
	if (command == NULL) {
		mbus_errorf("can not create command");
		goto bail;
	}
	TAILQ_INSERT_TAIL(&client->commands, command, commands);
	mbus_infof("registered '%s' '%s'", client_get_name(client), identifier);
out:	return 0;
bail:	command_destroy(command);
	return -1;
}

static int client_get_requests_count (struct client *client)
{
	if (client == NULL) {
		mbus_errorf("client is null");
		return 0;
	}
	return client->requests.count;
}

static int client_push_request (struct client *client, struct method *request)
{
	if (client == NULL) {
		mbus_errorf("client is null");
		goto bail;
	}
	if (request == NULL) {
		mbus_errorf("request is null");
		goto bail;
	}
	TAILQ_INSERT_TAIL(&client->requests, request, methods);
	return 0;
bail:	return -1;
}

static struct method * client_pop_request (struct client *client)
{
	struct method *request;
	if (client == NULL) {
		mbus_errorf("client is null");
		goto bail;
	}
	if (client->requests.count <= 0) {
		goto bail;
	}
	request = client->requests.tqh_first;
	TAILQ_REMOVE(&client->requests, client->requests.tqh_first, methods);
	return request;
bail:	return NULL;
}

static int client_get_results_count (struct client *client)
{
	if (client == NULL) {
		mbus_errorf("client is null");
		return 0;
	}
	return client->results.count;
}

static int client_push_result (struct client *client, struct method *result)
{
	if (client == NULL) {
		mbus_errorf("client is null");
		goto bail;
	}
	if (result == NULL) {
		mbus_errorf("result is null");
		goto bail;
	}
	TAILQ_INSERT_TAIL(&client->results, result, methods);
	return 0;
bail:	return -1;
}

static struct method * client_pop_result (struct client *client)
{
	struct method *result;
	if (client == NULL) {
		mbus_errorf("client is null");
		goto bail;
	}
	if (client->results.count <= 0) {
		goto bail;
	}
	result = client->results.tqh_first;
	TAILQ_REMOVE(&client->results, client->results.tqh_first, methods);
	return result;
bail:	return NULL;
}

static int client_get_events_count (struct client *client)
{
	if (client == NULL) {
		mbus_errorf("client is null");
		return 0;
	}
	return client->events.count;
}

static int client_push_event (struct client *client, struct method *event)
{
	if (client == NULL) {
		mbus_errorf("client is null");
		goto bail;
	}
	if (event == NULL) {
		mbus_errorf("event is null");
		goto bail;
	}
	TAILQ_INSERT_TAIL(&client->events, event, methods);
	return 0;
bail:	return -1;
}

static struct method * client_pop_event (struct client *client)
{
	struct method *event;
	if (client == NULL) {
		mbus_errorf("client is null");
		goto bail;
	}
	if (client->events.count <= 0) {
		goto bail;
	}
	event = client->events.tqh_first;
	TAILQ_REMOVE(&client->events, client->events.tqh_first, methods);
	return event;
bail:	return NULL;
}

static void client_destroy (struct client *client)
{
	struct method *result;
	struct method *request;
	struct method *event;
	struct method *wait;
	struct command *command;
	struct subscription *subscription;
	if (client == NULL) {
		return;
	}
	if (client->socket != NULL) {
		if (client->link == client_link_tcp ||
		    client->link == client_link_uds) {
			mbus_socket_destroy(client->socket);
		}
	}
	if (client->name != NULL) {
		free(client->name);
	}
	while (client->commands.tqh_first != NULL) {
		command = client->commands.tqh_first;
		TAILQ_REMOVE(&client->commands, client->commands.tqh_first, commands);
		command_destroy(command);
	}
	while (client->subscriptions.tqh_first != NULL) {
		subscription = client->subscriptions.tqh_first;
		TAILQ_REMOVE(&client->subscriptions, client->subscriptions.tqh_first, subscriptions);
		subscription_destroy(subscription);
	}
	while (client->requests.tqh_first != NULL) {
		request = client->requests.tqh_first;
		TAILQ_REMOVE(&client->requests, client->requests.tqh_first, methods);
		method_destroy(request);
	}
	while (client->results.tqh_first != NULL) {
		result = client->results.tqh_first;
		TAILQ_REMOVE(&client->results, client->results.tqh_first, methods);
		method_destroy(result);
	}
	while (client->events.tqh_first != NULL) {
		event = client->events.tqh_first;
		TAILQ_REMOVE(&client->events, client->events.tqh_first, methods);
		method_destroy(event);
	}
	while (client->waits.tqh_first != NULL) {
		wait = client->waits.tqh_first;
		TAILQ_REMOVE(&client->waits, client->waits.tqh_first, methods);
		method_destroy(wait);
	}
	free(client);
}

static struct client * client_create (const char *name, enum client_link link)
{
	struct client *client;
	client = NULL;
	if (name == NULL) {
		mbus_errorf("name is null");
		goto bail;
	}
	client = malloc(sizeof(struct client));
	if (client == NULL) {
		mbus_errorf("can not allocate memory");
		goto bail;
	}
	memset(client, 0, sizeof(struct client));
	TAILQ_INIT(&client->subscriptions);
	TAILQ_INIT(&client->commands);
	TAILQ_INIT(&client->requests);
	TAILQ_INIT(&client->results);
	TAILQ_INIT(&client->events);
	TAILQ_INIT(&client->waits);
	client->name = strdup(name);
	if (client->name == NULL) {
		mbus_errorf("can not allocate memory");
		goto bail;
	}
	client->status = 0;
	client->link = link;
	client->ssequence = MBUS_METHOD_SEQUENCE_START;
	client->esequence = MBUS_METHOD_SEQUENCE_START;
	return client;
bail:	client_destroy(client);
	return NULL;
}

static struct client * server_find_client_by_name (struct mbus_server *server, const char *name)
{
	struct client *client;
	if (name == NULL) {
		mbus_errorf("name is null");
		return NULL;
	}
	TAILQ_FOREACH(client, &server->clients, clients) {
		if (strcmp(client_get_name(client), name) == 0) {
			return client;
		}
	}
	return NULL;
}

static struct client * server_find_client_by_fd (struct mbus_server *server, int fd)
{
	struct client *client;
	if (fd < 0) {
		mbus_errorf("fd is invalid");
		return NULL;
	}
	TAILQ_FOREACH(client, &server->clients, clients) {
		if (client_get_link(client) == client_link_websocket) {
			struct websocket_client_data *data;
			data = (struct websocket_client_data *) client_get_socket(client);
			if (lws_get_socket_fd(data->wsi) == fd) {
				return client;
			}
		} else if (client_get_link(client) == client_link_uds) {
			if (mbus_socket_get_fd(client_get_socket(client)) == fd) {
				return client;
			}
		} else if (client_get_link(client) == client_link_tcp) {
			if (mbus_socket_get_fd(client_get_socket(client)) == fd) {
				return client;
			}
		}
	}
	return NULL;
}

static __attribute__ ((__unused__)) struct client * server_find_client_by_socket (struct mbus_server *server, struct mbus_socket *socket)
{
	struct client *client;
	if (socket == NULL) {
		mbus_errorf("socket is null");
		return NULL;
	}
	TAILQ_FOREACH(client, &server->clients, clients) {
		if (client_get_socket(client) == socket) {
			return client;
		}
	}
	return NULL;
}

static int server_send_event_to (struct mbus_server *server, const char *source, const char *destination, const char *identifier, struct mbus_json *payload)
{
	int rc;
	struct client *client;
	struct method *method;
	struct subscription *subscription;
	if (server == NULL) {
		mbus_errorf("server is null");
		goto bail;
	}
	if (source == NULL) {
		mbus_errorf("source is null");
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
	if (payload == NULL) {
		mbus_errorf("payload is null");
		goto bail;
	}
	if (strcmp(destination, MBUS_METHOD_EVENT_DESTINATION_ALL) == 0) {
		TAILQ_FOREACH(client, &server->clients, clients) {
			method = method_create(MBUS_METHOD_TYPE_EVENT, source, client_get_name(client), identifier, client->esequence, payload);
			if (method == NULL) {
				mbus_errorf("can not create method");
				goto bail;
			}
			client->esequence += 1;
			if (client->esequence >= MBUS_METHOD_SEQUENCE_END) {
				client->esequence = MBUS_METHOD_SEQUENCE_START;
			}
			rc = client_push_event(client, method);
			if (rc != 0) {
				mbus_errorf("can not push method");
				method_destroy(method);
				goto bail;
			}
		}
	} else if (strcmp(destination, MBUS_METHOD_EVENT_DESTINATION_SUBSCRIBERS) == 0) {
		TAILQ_FOREACH(client, &server->clients, clients) {
			TAILQ_FOREACH(subscription, &client->subscriptions, subscriptions) {
				if (strcmp(subscription_get_source(subscription), MBUS_METHOD_EVENT_SOURCE_ALL) != 0) {
					if (strcmp(subscription_get_source(subscription), source) != 0) {
						continue;
					}
				}
				if (strcmp(subscription_get_event(subscription), MBUS_METHOD_EVENT_IDENTIFIER_ALL) != 0) {
					if (strcmp(subscription_get_event(subscription), identifier) != 0) {
						continue;
					}
				}
				method = method_create(MBUS_METHOD_TYPE_EVENT, source, client_get_name(client), identifier, client->esequence, payload);
				if (method == NULL) {
					mbus_errorf("can not create method");
					goto bail;
				}
				client->esequence += 1;
				if (client->esequence >= MBUS_METHOD_SEQUENCE_END) {
					client->esequence = MBUS_METHOD_SEQUENCE_START;
				}
				rc = client_push_event(client, method);
				if (rc != 0) {
					mbus_errorf("can not push method");
					method_destroy(method);
					goto bail;
				}
				break;
			}
		}
	} else {
		TAILQ_FOREACH(client, &server->clients, clients) {
			if (strcmp(client_get_name(client), destination) != 0) {
				continue;
			}
			method = method_create(MBUS_METHOD_TYPE_EVENT, source, client_get_name(client), identifier, client->esequence, payload);
			if (method == NULL) {
				mbus_errorf("can not create method");
				goto bail;
			}
			client->esequence += 1;
			if (client->esequence >= MBUS_METHOD_SEQUENCE_END) {
				client->esequence = MBUS_METHOD_SEQUENCE_START;
			}
			rc = client_push_event(client, method);
			if (rc != 0) {
				mbus_errorf("can not push method");
				method_destroy(method);
				goto bail;
			}
			break;
		}
	}
	return 0;
bail:	return -1;
}

static int server_send_status_to (struct mbus_server *server, const char *destination, const char *identifier, struct mbus_json *payload)
{
	int rc;
	struct client *client;
	struct method *method;
	if (server == NULL) {
		mbus_errorf("server is null");
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
	if (payload == NULL) {
		mbus_errorf("payload is null");
		goto bail;
	}
	TAILQ_FOREACH(client, &server->clients, clients) {
		if (strcmp(client_get_name(client), destination) != 0) {
			continue;
		}
		method = method_create(MBUS_METHOD_TYPE_STATUS, MBUS_SERVER_NAME, client_get_name(client), identifier, client->ssequence, payload);
		if (method == NULL) {
			mbus_errorf("can not create method");
			goto bail;
		}
		client->ssequence += 1;
		if (client->ssequence >= MBUS_METHOD_SEQUENCE_END) {
			client->ssequence = MBUS_METHOD_SEQUENCE_START;
		}
		rc = client_push_event(client, method);
		if (rc != 0) {
			mbus_errorf("can not push method");
			method_destroy(method);
			goto bail;
		}
		break;
	}
	return 0;
bail:	return -1;
}

static int server_send_event_connected (struct mbus_server *server, const char *source)
{
	int rc;
	struct mbus_json *payload;
	payload = NULL;
	if (server == NULL) {
		mbus_errorf("server is null");
		goto bail;
	}
	if (source == NULL) {
		mbus_errorf("source is null");
		goto bail;
	}
	payload = mbus_json_create_object();
	if (payload == NULL) {
		mbus_errorf("can not create payload");
		goto bail;
	}
	mbus_json_add_string_to_object_cs(payload, "source", source);
	rc = server_send_status_to(server, source, MBUS_SERVER_STATUS_CONNECTED, payload);
	if (rc != 0) {
		mbus_errorf("can not send event");
		goto bail;
	}
	rc = server_send_event_to(server, MBUS_SERVER_NAME, MBUS_METHOD_EVENT_DESTINATION_SUBSCRIBERS, MBUS_SERVER_EVENT_CONNECTED, payload);
	if (rc != 0) {
		mbus_errorf("can not send event");
		goto bail;
	}
	mbus_json_delete(payload);
	return 0;
bail:	if (payload != NULL) {
		mbus_json_delete(payload);
	}
	return -1;
}

static int server_send_event_disconnected (struct mbus_server *server, const char *source)
{
	int rc;
	struct mbus_json *payload;
	payload = NULL;
	if (server == NULL) {
		mbus_errorf("server is null");
		goto bail;
	}
	if (source == NULL) {
		mbus_errorf("source is null");
		goto bail;
	}
	payload = mbus_json_create_object();
	if (payload == NULL) {
		mbus_errorf("can not create payload");
		goto bail;
	}
	mbus_json_add_string_to_object_cs(payload, "source", source);
	rc = server_send_status_to(server, source, MBUS_SERVER_STATUS_DISCONNECTED, payload);
	if (rc != 0) {
		mbus_errorf("can not send event");
		goto bail;
	}
	rc = server_send_event_to(server, MBUS_SERVER_NAME, MBUS_METHOD_EVENT_DESTINATION_SUBSCRIBERS, MBUS_SERVER_EVENT_DISCONNECTED, payload);
	if (rc != 0) {
		mbus_errorf("can not send event");
		goto bail;
	}
	mbus_json_delete(payload);
	return 0;
bail:	if (payload != NULL) {
		mbus_json_delete(payload);
	}
	return -1;
}

static int server_send_event_subscribed (struct mbus_server *server, const char *source, const char *destination, const char *identifier)
{
	int rc;
	struct mbus_json *payload;
	payload = NULL;
	if (server == NULL) {
		mbus_errorf("server is null");
		goto bail;
	}
	if (source == NULL) {
		mbus_errorf("source is null");
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
	payload = mbus_json_create_object();
	if (payload == NULL) {
		mbus_errorf("can not create payload");
		goto bail;
	}
	mbus_json_add_string_to_object_cs(payload, "destination", destination);
	mbus_json_add_string_to_object_cs(payload, "identifier", identifier);
	rc = server_send_status_to(server, source, MBUS_SERVER_STATUS_SUBSCRIBED, payload);
	if (rc != 0) {
		mbus_errorf("can not send event");
		goto bail;
	}
	mbus_json_add_string_to_object_cs(payload, "source", source);
	rc = server_send_event_to(server, MBUS_SERVER_NAME, MBUS_METHOD_EVENT_DESTINATION_SUBSCRIBERS, MBUS_SERVER_EVENT_SUBSCRIBED, payload);
	if (rc != 0) {
		mbus_errorf("can not send event");
		goto bail;
	}
	mbus_json_delete(payload);
	return 0;
bail:	if (payload != NULL) {
		mbus_json_delete(payload);
	}
	return -1;
}

static int server_send_event_subscriber (struct mbus_server *server, const char *source, const char *destination, const char *identifier)
{
	int rc;
	struct mbus_json *payload;
	payload = NULL;
	if (server == NULL) {
		mbus_errorf("server is null");
		goto bail;
	}
	if (source == NULL) {
		mbus_errorf("source is null");
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
	payload = mbus_json_create_object();
	if (payload == NULL) {
		mbus_errorf("can not create payload");
		goto bail;
	}
	mbus_json_add_string_to_object_cs(payload, "source", source);
	mbus_json_add_string_to_object_cs(payload, "identifier", identifier);
	rc = server_send_status_to(server, destination, MBUS_SERVER_STATUS_SUBSCRIBER, payload);
	if (rc != 0) {
		mbus_errorf("can not send event");
		goto bail;
	}
	mbus_json_delete(payload);
	return 0;
bail:	if (payload != NULL) {
		mbus_json_delete(payload);
	}
	return -1;
}

static int server_accept_client (struct mbus_server *server, struct mbus_socket *from)
{
	int rc;
	char *string;
	struct method *method;
	struct client *client;
	struct mbus_socket *socket;
	string = NULL;
	method = NULL;
	client = NULL;
	socket = NULL;
	if (server == NULL) {
		mbus_errorf("server is null");
		goto bail;
	}
	socket = mbus_socket_accept(from);
	if (socket == NULL) {
		mbus_errorf("can not accept new socket connection");
		goto bail;
	}
	string = mbus_socket_read_string(socket);
	if (string == NULL) {
		mbus_errorf("can not read name");
		goto bail;
	}
	method = method_create_from_string(NULL, string);
	if (method == NULL) {
		mbus_errorf("can not create method");
		goto bail;
	}
	if (strcmp(method_get_request_type(method), MBUS_METHOD_TYPE_COMMAND) != 0) {
		mbus_errorf("invalid type: %s", method_get_request_type(method));
		goto bail;
	}
	if (strcmp(method_get_request_destination(method), MBUS_SERVER_NAME) != 0) {
		mbus_errorf("invalid destination: %s", method_get_request_destination(method));
		goto bail;
	}
	if (strcmp(method_get_request_identifier(method), MBUS_SERVER_COMMAND_CREATE) != 0) {
		mbus_errorf("invalid identifier: %s", method_get_request_identifier(method));
		goto bail;
	}
	TAILQ_FOREACH(client, &server->clients, clients) {
		if (strcmp(client_get_name(client), method_get_request_source(method)) == 0) {
			break;
		}
	}
	if (client != NULL) {
		mbus_errorf("client with name: '%s' already exists", method_get_request_source(method));
		client = NULL;
		goto exists;
	}
	client = server_find_client_by_name(server, method_get_request_source(method));
	if (client != NULL) {
		mbus_errorf("client with name: %s already exists", method_get_request_source(method));
		goto bail;
	}
	client = client_create(method_get_request_source(method), client_link_tcp);
	if (client == NULL) {
		mbus_errorf("can not create client");
		goto bail;
	}
	rc = client_set_socket(client, socket);
	if (rc != 0) {
		mbus_errorf("can not set client socket");
		goto bail;
	}
	mbus_infof("client: '%s' accepted", client_get_name(client));
	rc = method_set_result_code(method, 0);
	if (rc != 0) {
		mbus_errorf("can not set method result code");
		goto bail;
	}
	rc = mbus_socket_write_string(socket, method_get_result_string(method));
	if (rc != 0) {
		mbus_errorf("can not write string");
		goto bail;
	}
	method_destroy(method);
	free(string);
	TAILQ_INSERT_TAIL(&server->clients, client, clients);
	return 0;
bail:	if (socket != NULL) {
		mbus_socket_destroy(socket);
	}
	if (client != NULL) {
		client_destroy(client);
	}
	if (method != NULL) {
		method_destroy(method);
	}
	if (string != NULL) {
		free(string);
	}
	return -1;
exists:	if (socket != NULL) {
		mbus_socket_destroy(socket);
	}
	if (client != NULL) {
		client_destroy(client);
	}
	if (method != NULL) {
		method_destroy(method);
	}
	if (string != NULL) {
		free(string);
	}
	return -2;
}

static int server_handle_command_subscribe (struct mbus_server *server, struct method *method)
{
	int rc;
	const char *source;
	const char *event;
	if (server == NULL) {
		mbus_errorf("server is null");
		goto bail;
	}
	if (method == NULL) {
		mbus_errorf("method is null");
		goto bail;
	}
	source = mbus_json_get_string_value(method_get_request_payload(method), "source");
	event = mbus_json_get_string_value(method_get_request_payload(method), "event");
	if ((source == NULL) ||
	    (event == NULL)) {
		mbus_errorf("invalid request");
		goto bail;
	}
	rc = client_add_subscription(method_get_source(method), source, event);
	if (rc != 0) {
		mbus_errorf("can not add subscription");
		goto bail;
	}
	rc = server_send_event_subscribed(server, client_get_name(method_get_source(method)), source, event);
	if (rc != 0) {
		mbus_errorf("can not send connected event");
		goto bail;
	}
	rc = server_send_event_subscriber(server, client_get_name(method_get_source(method)), source, event);
	if (rc != 0) {
		mbus_errorf("can not send connected event");
		goto bail;
	}
	return 0;
bail:	return -1;
}

static int server_handle_command_register (struct mbus_server *server, struct method *method)
{
	int rc;
	const char *command;
	if (server == NULL) {
		mbus_errorf("server is null");
		goto bail;
	}
	if (method == NULL) {
		mbus_errorf("method is null");
		goto bail;
	}
	command = mbus_json_get_string_value(method_get_request_payload(method), "command");
	if (command == NULL) {
		mbus_errorf("invalid request");
		goto bail;
	}
	rc = client_add_command(method_get_source(method), command);
	if (rc != 0) {
		mbus_errorf("can not add subscription");
		goto bail;
	}
	return 0;
bail:	return -1;
}

static int server_handle_command_event (struct mbus_server *server, struct method *method)
{
	int rc;
	const char *destination;
	const char *identifier;
	struct mbus_json *event;
	if (server == NULL) {
		mbus_errorf("server is null");
		goto bail;
	}
	if (method == NULL) {
		mbus_errorf("method is null");
		goto bail;
	}
	destination = mbus_json_get_string_value(method_get_request_payload(method), "destination");
	identifier = mbus_json_get_string_value(method_get_request_payload(method), "identifier");
	event = mbus_json_get_object_item(method_get_request_payload(method), "event");
	if ((destination == NULL) ||
	    (identifier == NULL) ||
	    (event == NULL)) {
		mbus_errorf("invalid request");
		goto bail;
	}
	rc = server_send_event_to(server, client_get_name(method_get_source(method)), destination, identifier, event);
	if (rc != 0) {
		mbus_errorf("can not send event");
	}
	return 0;
bail:	return -1;
}

static int server_handle_command_call (struct mbus_server *server, struct method *method)
{
	int response;
	const char *destination;
	const char *identifier;
	struct method *request;
	struct client *client;
	struct mbus_json *call;
	response = 1;
	if (server == NULL) {
		mbus_errorf("server is null");
		goto bail;
	}
	if (method == NULL) {
		mbus_errorf("method is null");
		goto bail;
	}
	destination = mbus_json_get_string_value(method_get_request_payload(method), "destination");
	identifier = mbus_json_get_string_value(method_get_request_payload(method), "identifier");
	call = mbus_json_get_object_item(method_get_request_payload(method), "call");
	if ((destination == NULL) ||
	    (identifier == NULL) ||
	    (call == NULL)) {
		mbus_errorf("invalid request");
		goto bail;
	}
	if (strcmp(destination, MBUS_SERVER_NAME) == 0) {
		mbus_debugf("call from server");
		if (strcmp(identifier, MBUS_SERVER_COMMAND_STATUS) == 0) {
			mbus_debugf("command status");
			struct mbus_json *object;
			struct mbus_json *source;
			struct mbus_json *clients;
			struct mbus_json *commands;
			struct mbus_json *subscribes;
			struct command *command;
			struct subscription *subscription;
			clients = NULL;
			clients = mbus_json_create_array();
			if (clients == NULL) {
				goto command_status_bail;
			}
			TAILQ_FOREACH(client, &server->clients, clients) {
				source = mbus_json_create_object();
				if (source == NULL) {
					goto command_status_bail;
				}
				mbus_json_add_item_to_array(clients, source);
				mbus_json_add_string_to_object_cs(source, "source", client_get_name(client));
				subscribes = mbus_json_create_array();
				if (subscribes == NULL) {
					goto command_status_bail;
				}
				mbus_json_add_item_to_object_cs(source, "subscriptions", subscribes);
				TAILQ_FOREACH(subscription, &client->subscriptions, subscriptions) {
					object = mbus_json_create_object();
					if (object == NULL) {
						goto command_status_bail;
					}
					mbus_json_add_item_to_array(subscribes, object);
					mbus_json_add_string_to_object_cs(object, "source", subscription_get_source(subscription));
					mbus_json_add_string_to_object_cs(object, "identifier", subscription_get_event(subscription));
				}
				commands = mbus_json_create_array();
				if (commands == NULL) {
					goto command_status_bail;
				}
				mbus_json_add_item_to_object_cs(source, "commands", commands);
				TAILQ_FOREACH(command, &client->commands, commands) {
					object = mbus_json_create_object();
					if (object == NULL) {
						goto command_status_bail;
					}
					mbus_json_add_item_to_array(commands, object);
					mbus_json_add_string_to_object_cs(object, "identifier", command_get_identifier(command));
				}
			}
			method_add_result_payload(method, "clients", clients);
			goto out;
command_status_bail:
			if (clients != NULL) {
				mbus_json_delete(clients);
			}
		} else if (strcmp(identifier, MBUS_SERVER_COMMAND_CLOSE) == 0) {
			const char *source;
			source = mbus_json_get_string_value(call, "source");
			if (source == NULL) {
				mbus_errorf("method request source is null");
				goto bail;
			}
			if (strcmp(source, MBUS_SERVER_NAME) == 0) {
				server->running = 0;
			} else {
				TAILQ_FOREACH(client, &server->clients, clients) {
					if (strcmp(client_get_name(client), source) != 0) {
						continue;
					}
					client_set_socket(client, NULL);
					break;
				}
				if (client == NULL) {
					mbus_errorf("could not find requested source: %s", source);
					goto bail;
				}
			}
		} else {
			mbus_errorf("unknown command request: %s", identifier);
			goto bail;
		}
	} else {
		TAILQ_FOREACH(client, &server->clients, clients) {
			if (strcmp(destination, client_get_name(client)) != 0) {
				continue;
			}
			break;
		}
		if (client == NULL) {
			mbus_errorf("client %s does not exists", destination);
			goto bail;
		}
		request = method_create(MBUS_METHOD_TYPE_COMMAND, client_get_name(method_get_source(method)), destination, identifier, method_get_request_sequence(method), call);
		if (request == NULL) {
			mbus_errorf("can not create call method");
			goto bail;
		}
		client_push_request(client, request);
		response = 0;
	}
out:	return response;
bail:	return -1;
}

static int server_handle_command_result (struct mbus_server *server, struct method *method)
{
	struct client *client;
	struct method *wait;
	struct method *nwait;
	const char *source;
	const char *destination;
	const char *identifier;
	int sequence;
	int rc;
	source = client_get_name(method_get_source(method));
	destination = mbus_json_get_string_value(method_get_request_payload(method), "destination");
	identifier = mbus_json_get_string_value(method_get_request_payload(method), "identifier");
	sequence = mbus_json_get_int_value(method_get_request_payload(method), "sequence");
	rc = mbus_json_get_int_value(method_get_request_payload(method), "return");
	TAILQ_FOREACH(client, &server->clients, clients) {
		if (strcmp(client_get_name(client), destination) != 0) {
			continue;
		}
		TAILQ_FOREACH_SAFE(wait, &client->waits, methods, nwait) {
			if (sequence != method_get_request_sequence(wait)) {
				continue;
			}
			if (strcmp(mbus_json_get_string_value(method_get_request_payload(wait), "destination"), source) != 0) {
				continue;
			}
			if (strcmp(mbus_json_get_string_value(method_get_request_payload(wait), "identifier"), identifier) != 0) {
				continue;
			}
			TAILQ_REMOVE(&client->waits, wait, methods);
			method_set_result_code(wait, rc);
			method_set_result_payload(wait, mbus_json_duplicate(mbus_json_get_object_item(method_get_request_payload(method), "result"), 1));
			client_push_result(client, wait);
			break;
		}
		break;
	}
	return 0;
}

static int server_handle_methods (struct mbus_server *server)
{
	int rc;
	int response;
	struct method *method;
	struct method *nmethod;
	if (server == NULL) {
		mbus_errorf("server is null");
		goto bail;
	}
	TAILQ_FOREACH_SAFE(method, &server->methods, methods, nmethod) {
		rc = -1;
		response = 1;
		if (strcmp(method_get_request_destination(method), MBUS_SERVER_NAME) == 0) {
			if (strcmp(method_get_request_identifier(method), MBUS_SERVER_COMMAND_SUBSCRIBE) == 0) {
				rc = server_handle_command_subscribe(server, method);
			} else if (strcmp(method_get_request_identifier(method), MBUS_SERVER_COMMAND_REGISTER) == 0) {
				rc = server_handle_command_register(server, method);
			} else if (strcmp(method_get_request_identifier(method), MBUS_SERVER_COMMAND_RESULT) == 0) {
				rc = server_handle_command_result(server, method);
			} else if (strcmp(method_get_request_identifier(method), MBUS_SERVER_COMMAND_EVENT) == 0) {
				rc = server_handle_command_event(server, method);
			} else if (strcmp(method_get_request_identifier(method), MBUS_SERVER_COMMAND_CALL) == 0) {
				response = server_handle_command_call(server, method);
				if (response < 0) {
					rc = response;
				} else {
					rc = 0;
				}
			}
		}
		if (rc != 0) {
			mbus_errorf("can not execute method type: '%s', identifier: '%s'", method_get_request_type(method), method_get_request_identifier(method));
		}
		if (strcmp(method_get_request_destination(method), MBUS_SERVER_NAME) == 0) {
			TAILQ_REMOVE(&server->methods, method, methods);
		}
		if (response == 1 || rc != 0) {
			method_set_result_code(method, rc);
			client_push_result(method_get_source(method), method);
		} else {
			TAILQ_INSERT_TAIL(&method_get_source(method)->waits, method, methods);
		}
	}
	return 0;
bail:	return -1;
}

static int server_handle_method_command (struct mbus_server *server, struct method *method)
{
	TAILQ_INSERT_TAIL(&server->methods, method, methods);
	return 0;
}

static int server_handle_method (struct mbus_server *server, struct client *client, const char *string)
{
	int rc;
	struct method *method;
	method = method_create_from_string(client, string);
	if (method == NULL) {
		mbus_errorf("invalid method");
		goto bail;
	}
	if (strcmp(method_get_request_type(method), MBUS_METHOD_TYPE_COMMAND) == 0) {
		rc = server_handle_method_command(server, method);
	} else {
		mbus_errorf("invalid method");
		goto bail;
	}
	if (rc != 0) {
		mbus_errorf("could not handle method");
		goto bail;
	}
	return 0;
bail:	if (method != NULL) {
		method_destroy(method);
	}
	return -1;
}

static void websocket_client_data_buffer_uninit (struct websocket_client_data_buffer *buffer)
{
	if (buffer->buffer != NULL) {
		free(buffer->buffer);
	}
	memset(buffer, 0, sizeof(struct websocket_client_data_buffer));
}

static void websocket_client_data_buffer_init (struct websocket_client_data_buffer *buffer)
{
	websocket_client_data_buffer_uninit(buffer);
	memset(buffer, 0, sizeof(struct websocket_client_data_buffer));
}

static unsigned int websocket_client_data_buffer_length (struct websocket_client_data_buffer *buffer)
{
	return buffer->length;
}

static int websocket_client_data_buffer_push (struct websocket_client_data_buffer *buffer, const void *data, unsigned int length)
{
	if (buffer->size < buffer->length + length) {
		uint8_t *tmp;
		while (buffer->size < buffer->length + length) {
			buffer->size += 1024;
		}
		tmp = realloc(buffer->buffer, buffer->size);
		if (tmp == NULL) {
			tmp = malloc(buffer->size);
			if (tmp == NULL) {
				mbus_errorf("can not allocate memory");
				return -1;
			}
			memcpy(tmp, buffer->buffer, buffer->length);
			free(buffer->buffer);
		}
		buffer->buffer = tmp;
	}
	memcpy(buffer->buffer + buffer->length, data, length);
	buffer->length += length;
	return 0;
}

static int websocket_client_data_buffer_push_string (struct websocket_client_data_buffer *buffer, const char *string)
{
	int rc;
	uint32_t length;
	if (string == NULL) {
		mbus_errorf("string is invalid");
		return -1;
	}
	length = strlen(string);
	length = htonl(length);
	rc = websocket_client_data_buffer_push(buffer, &length, sizeof(length));
	if (rc != 0) {
		mbus_errorf("can not push length");
		return -1;
	}
	length = ntohl(length);
	rc = websocket_client_data_buffer_push(buffer, string, length);
	if (rc != 0) {
		mbus_errorf("can not push string");
		return -1;
	}
	return 0;
}

static int websocket_client_data_buffer_shift (struct websocket_client_data_buffer *buffer, unsigned int length)
{
	if (length == 0) {
		return 0;
	}
	if (length > buffer->length) {
		mbus_errorf("invalid length");
		return -1;
	}
	memmove(buffer->buffer, buffer->buffer + length, buffer->length - length);
	buffer->length -= length;
	return 0;
}

static int websocket_accept_client (struct mbus_server *server, struct websocket_client_data *data, const char *string)
{
	int rc;
	struct client *client;
	struct method *method;
	method = NULL;
	method = method_create_from_string(NULL, string);
	if (method == NULL) {
		mbus_errorf("can not create method");
		goto bail;
	}
	if (strcmp(method_get_request_type(method), MBUS_METHOD_TYPE_COMMAND) != 0) {
		mbus_errorf("invalid type: %s", method_get_request_type(method));
		goto bail;
	}
	if (strcmp(method_get_request_destination(method), MBUS_SERVER_NAME) != 0) {
		mbus_errorf("invalid destination: %s", method_get_request_destination(method));
		goto bail;
	}
	if (strcmp(method_get_request_identifier(method), MBUS_SERVER_COMMAND_CREATE) != 0) {
		mbus_errorf("invalid identifier: %s", method_get_request_identifier(method));
		goto bail;
	}
	client = server_find_client_by_name(server, method_get_request_source(method));
	if (client != NULL) {
		mbus_errorf("client with name: %s already exists", method_get_request_source(method));
		goto bail;
	}
	data->client = client_create(method_get_request_source(method), client_link_websocket);
	if (data->client == NULL) {
		mbus_errorf("can not create client");
		goto bail;
	}
	rc = client_set_socket(data->client, (struct mbus_socket *) data);
	if (rc != 0) {
		mbus_errorf("can not set client socket");
		goto bail;
	}
	mbus_debugf("client: '%s' accepted", client_get_name(data->client));
	rc = method_set_result_code(method, 0);
	if (rc != 0) {
		mbus_errorf("can not set method result code");
		goto bail;
	}
	rc = websocket_client_data_buffer_push_string(&data->buffer.out, method_get_result_string(method));
	if (rc != 0) {
		mbus_errorf("can not write string");
		goto bail;
	}
	lws_callback_on_writable(data->wsi);
	method_destroy(method);
	TAILQ_INSERT_TAIL(&server->clients, data->client, clients);
	return 0;
bail:	if (method != NULL) {
		method_destroy(method);
	}
	if (data->client != NULL) {
		client_destroy(data->client);
		data->client = NULL;
	}
	return -1;
}

static int websocket_protocol_mbus_callback (struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
	int rc;
	struct mbus_server *server;
	struct websocket_client_data *data;
	(void) wsi;
	(void) reason;
	(void) user;
	(void) in;
	(void) len;
	mbus_debugf("websocket callback");
	server = g_server;
	data = (struct websocket_client_data *) user;
	switch (reason) {
		case LWS_CALLBACK_LOCK_POLL:
			mbus_debugf("  lock poll");
			break;
		case LWS_CALLBACK_ADD_POLL_FD:
			mbus_debugf("  add poll fd");
			{
				struct pollfd *tmp;
				struct lws_pollargs *pa = (struct lws_pollargs *) in;
				{
					unsigned int i;
					struct lws_pollargs *pa = (struct lws_pollargs *) in;
					for (i = 0; i < server->socket.websocket.pollfds.length; i++) {
						if (server->socket.websocket.pollfds.pollfds[i].fd == pa->fd) {
							server->socket.websocket.pollfds.pollfds[i].events = pa->events;
							break;
						}
					}
					if (i < server->socket.websocket.pollfds.length) {
						break;
					}
				}
				if (server->socket.websocket.pollfds.length + 1 > server->socket.websocket.pollfds.size) {
					while (server->socket.websocket.pollfds.length + 1 > server->socket.websocket.pollfds.size) {
						server->socket.websocket.pollfds.size += 1024;
					}
					tmp = realloc(server->socket.websocket.pollfds.pollfds, sizeof(struct pollfd) * server->socket.websocket.pollfds.size);
					if (tmp == NULL) {
						tmp = malloc(sizeof(int) * server->socket.websocket.pollfds.size);
						if (tmp == NULL) {
							mbus_errorf("can not allocate memory");
							return -1;
						}
						memcpy(tmp, server->socket.websocket.pollfds.pollfds, sizeof(struct pollfd) * server->socket.websocket.pollfds.length);
						free(server->socket.websocket.pollfds.pollfds);
					}
					server->socket.websocket.pollfds.pollfds = tmp;
				}
				server->socket.websocket.pollfds.pollfds[server->socket.websocket.pollfds.length].fd = pa->fd;
				server->socket.websocket.pollfds.pollfds[server->socket.websocket.pollfds.length].events = pa->events;
				server->socket.websocket.pollfds.pollfds[server->socket.websocket.pollfds.length].revents = 0;
				server->socket.websocket.pollfds.length += 1;
			}
			break;
		case LWS_CALLBACK_CHANGE_MODE_POLL_FD:
			mbus_debugf("  change mode poll fd");
			{
				unsigned int i;
				struct lws_pollargs *pa = (struct lws_pollargs *) in;
				for (i = 0; i < server->socket.websocket.pollfds.length; i++) {
					if (server->socket.websocket.pollfds.pollfds[i].fd == pa->fd) {
						server->socket.websocket.pollfds.pollfds[i].events = pa->events;
						break;
					}
				}
			}
			break;
		case LWS_CALLBACK_DEL_POLL_FD:
			mbus_debugf("  del poll fd");
			{
				unsigned int i;
				struct lws_pollargs *pa = (struct lws_pollargs *) in;
				for (i = 0; i < server->socket.websocket.pollfds.length; i++) {
					if (server->socket.websocket.pollfds.pollfds[i].fd == pa->fd) {
						memmove(&server->socket.websocket.pollfds.pollfds[i], &server->socket.websocket.pollfds.pollfds[i + 1], server->socket.websocket.pollfds.length - i);
						server->socket.websocket.pollfds.length -= 1;
						break;
					}
				}
			}
			break;
		case LWS_CALLBACK_UNLOCK_POLL:
			mbus_debugf("  unlock poll");
			break;
		case LWS_CALLBACK_GET_THREAD_ID:
			mbus_debugf("  get thread id");
			break;
		case LWS_CALLBACK_PROTOCOL_INIT:
			mbus_debugf("  protocol init");
			break;
		case LWS_CALLBACK_FILTER_NETWORK_CONNECTION:
			mbus_debugf("  filter network connection");
			break;
		case LWS_CALLBACK_SERVER_NEW_CLIENT_INSTANTIATED:
			mbus_debugf("  new client instantiated");
			break;
		case LWS_CALLBACK_FILTER_PROTOCOL_CONNECTION:
			mbus_debugf("  filter protocol connection");
			break;
		case LWS_CALLBACK_CONFIRM_EXTENSION_OKAY:
			mbus_debugf("  confirm extensions okay");
			break;
		case LWS_CALLBACK_ESTABLISHED:
			mbus_debugf("  established");
			data->wsi = wsi;
			websocket_client_data_buffer_init(&data->buffer.in);
			websocket_client_data_buffer_init(&data->buffer.out);
			break;
		case LWS_CALLBACK_HTTP_DROP_PROTOCOL:
			mbus_debugf("  http drop protocol");
			break;
		case LWS_CALLBACK_PROTOCOL_DESTROY:
			mbus_debugf("  protocol destroy");
			break;
		case LWS_CALLBACK_WS_PEER_INITIATED_CLOSE:
			mbus_debugf("  ws peer initiated close");
			break;
		case LWS_CALLBACK_WSI_CREATE:
			mbus_debugf("  wsi create");
			break;
		case LWS_CALLBACK_WSI_DESTROY:
			mbus_debugf("  wsi destroy");
			break;
		case LWS_CALLBACK_CLOSED:
			mbus_debugf("  closed");
			websocket_client_data_buffer_uninit(&data->buffer.in);
			websocket_client_data_buffer_uninit(&data->buffer.out);
			if (data->client != NULL) {
				client_set_socket(data->client, NULL);
				data->client = NULL;
			}
			data->wsi = NULL;
			break;
		case LWS_CALLBACK_RECEIVE:
			mbus_debugf("  receive");
			mbus_debugf("    data: %p", data);
			mbus_debugf("      wsi   : %p", data->wsi);
			mbus_debugf("      client: %p", data->client);
			mbus_debugf("      buffer:");
			mbus_debugf("        in:");
			mbus_debugf("          length  : %d", data->buffer.in.length);
			mbus_debugf("          size    : %d", data->buffer.in.size);
			mbus_debugf("        out:");
			mbus_debugf("          length  : %d", data->buffer.out.length);
			mbus_debugf("          size    : %d", data->buffer.out.size);
			mbus_debugf("    in: %p", in);
			mbus_debugf("    len: %zd", len);
			if (data->wsi == NULL &&
			    data->client == NULL) {
			}
			rc = websocket_client_data_buffer_push(&data->buffer.in, in, len);
			if (rc != 0) {
				mbus_errorf("can not push in");
				return -1;
			}
			{
				uint8_t *ptr;
				uint8_t *end;
				uint32_t expected;
				mbus_debugf("      buffer.in:");
				mbus_debugf("        length  : %d", data->buffer.in.length);
				mbus_debugf("        size    : %d", data->buffer.in.size);
				while (1) {
					char *string;
					ptr = data->buffer.in.buffer;
					end = ptr + data->buffer.in.length;
					if (end - ptr < 4) {
						break;
					}
					expected  = *ptr++ << 0x00;
					expected |= *ptr++ << 0x08;
					expected |= *ptr++ << 0x10;
					expected |= *ptr++ << 0x18;
					expected = ntohl(expected);
					mbus_debugf("%d", expected);
					if (end - ptr < expected) {
						break;
					}
					mbus_debugf("message: '%.*s'", expected, ptr);
					string = strndup((char *) ptr, expected);
					if (string == NULL) {
						mbus_errorf("can not allocate memory");
						return -1;
					}
					if (data->client == NULL) {
						rc = websocket_accept_client(server, data, string);
						if (rc != 0) {
							mbus_errorf("can not accept client");
							lws_callback_on_writable(data->wsi);
						}
					} else {
						mbus_debugf("new request from client: '%s', '%s'", client_get_name(data->client), string);
						rc = server_handle_method(server, data->client, string);
						if (rc != 0) {
							mbus_errorf("can not handle request, closing client: '%s' connection", client_get_name(data->client));
							free(string);
							return -1;
						}
					}
					free(string);
					rc = websocket_client_data_buffer_shift(&data->buffer.in, sizeof(uint32_t) + expected);
					if (rc != 0) {
						mbus_errorf("can not shift in");
						return -1;
					}
				}
			}
			break;
		case LWS_CALLBACK_SERVER_WRITEABLE:
			mbus_debugf("  server writable");
			if (data->client == NULL ||
			    client_get_socket(data->client) == NULL) {
				mbus_debugf("client is closed");
				return -1;
			}
			while (websocket_client_data_buffer_length(&data->buffer.out) > 0 &&
			       lws_send_pipe_choked(data->wsi) == 0) {
				uint8_t *ptr;
				uint8_t *end;
				uint32_t expected;
				uint8_t *payload;
				ptr = data->buffer.out.buffer;
				end = ptr + data->buffer.out.length;
				expected = end - ptr;
				if (end - ptr < expected) {
					break;
				}
				mbus_debugf("write");
				payload = malloc(LWS_PRE + expected);
				if (payload == NULL) {
					mbus_errorf("can not allocate memory");
					return -1;
				}
				memset(payload, 0, LWS_PRE + expected);
				memcpy(payload + LWS_PRE, ptr, expected);
				mbus_debugf("payload: %d, %.*s", expected - 4, expected - 4, ptr + 4);
				rc = lws_write(data->wsi, payload + LWS_PRE, expected, LWS_WRITE_BINARY);
				mbus_debugf("expected: %d, rc: %d", expected, rc);
				rc = websocket_client_data_buffer_shift(&data->buffer.out, rc);
				if (rc != 0) {
					mbus_errorf("can not shift in");
					free(payload);
					return -1;
				}
				free(payload);
				break;
			}
			if (websocket_client_data_buffer_length(&data->buffer.out) > 0) {
				lws_callback_on_writable(data->wsi);
			}
			break;
		default:
			mbus_errorf("unknown reason: %d", reason);
			return -1;
			break;
	}
	return 0;
}

static struct lws_protocols websocket_protocols[] = {
	{
		"mbus",
		websocket_protocol_mbus_callback,
		sizeof(struct websocket_client_data),
		0,
		0,
		NULL,
	},
	{
		NULL,
		NULL,
		0,
		0,
		0,
		NULL
	}
};

static const struct lws_extension websocket_extensions[] = {
	{
		"permessage-deflate",
		lws_extension_callback_pm_deflate,
		"permessage-deflate"
	},
	{
		"deflate-frame",
		lws_extension_callback_pm_deflate,
		"deflate_frame"
	},
	{
		NULL,
		NULL,
		NULL
	}
};

int mbus_server_run_timeout (struct mbus_server *server, int milliseconds)
{
	int rc;
	char *string;
	unsigned int c;
	unsigned int n;
	struct client *client;
	struct client *nclient;
	struct client *wclient;
	struct client *nwclient;
	struct method *method;
	struct method *nmethod;
	if (server == NULL) {
		mbus_errorf("server is null");
		return -1;
	}
	mbus_debugf("running server");
	if (server->running == 0) {
		goto out;
	}
	rc = server_handle_methods(server);
	if (rc != 0) {
		mbus_errorf("can not handle methods");
		goto bail;
	}
	n  = 3;
	n += server->clients.count;
	n += server->socket.websocket.pollfds.length;
	if (n > server->socket.pollfds.size) {
		struct pollfd *tmp;
		while (n > server->socket.pollfds.size) {
			server->socket.pollfds.size += 1024;
		}
		tmp = realloc(server->socket.pollfds.pollfds, sizeof(struct pollfd) * server->socket.pollfds.size);
		if (tmp == NULL) {
			tmp = malloc(sizeof(int) * server->socket.pollfds.size);
			if (tmp == NULL) {
				mbus_errorf("can not allocate memory");
				goto bail;
			}
			memcpy(tmp, server->socket.pollfds.pollfds, sizeof(struct pollfd) * server->socket.pollfds.length);
			free(server->socket.pollfds.pollfds);
		}
		server->socket.pollfds.pollfds = tmp;
	}
	n = 0;
	if (server->socket.uds.socket != NULL) {
		server->socket.pollfds.pollfds[n].events = mbus_poll_event_in;
		server->socket.pollfds.pollfds[n].revents = 0;
		server->socket.pollfds.pollfds[n].fd = mbus_socket_get_fd(server->socket.uds.socket);
		n += 1;
	}
	if (server->socket.tcp.socket != NULL) {
		server->socket.pollfds.pollfds[n].events = mbus_poll_event_in;
		server->socket.pollfds.pollfds[n].revents = 0;
		server->socket.pollfds.pollfds[n].fd = mbus_socket_get_fd(server->socket.tcp.socket);
		n += 1;
	}
	TAILQ_FOREACH(client, &server->clients, clients) {
		if (client_get_socket(client) == NULL) {
			continue;
		}
		if (client_get_link(client) == client_link_tcp ||
		    client_get_link(client) == client_link_uds) {
			server->socket.pollfds.pollfds[n].events = mbus_poll_event_in;
			server->socket.pollfds.pollfds[n].revents = 0;
			server->socket.pollfds.pollfds[n].fd = mbus_socket_get_fd(client_get_socket(client));
			if (client_get_requests_count(client) != 0) {
				server->socket.pollfds.pollfds[n].events |= mbus_poll_event_out;
			}
			if (client_get_results_count(client) != 0) {
				server->socket.pollfds.pollfds[n].events |= mbus_poll_event_out;
			}
			if (client_get_events_count(client) != 0) {
				server->socket.pollfds.pollfds[n].events |= mbus_poll_event_out;
			}
			n += 1;
		} else if (client_get_link(client) == client_link_websocket) {
			struct websocket_client_data *data;
			data = (struct websocket_client_data *) client_get_socket(client);
			if (client_get_requests_count(client) != 0) {
				lws_callback_on_writable(data->wsi);
			}
			if (client_get_results_count(client) != 0) {
				lws_callback_on_writable(data->wsi);
			}
			if (client_get_events_count(client) != 0) {
				lws_callback_on_writable(data->wsi);
			}
		}
	}
	if (server->socket.websocket.pollfds.length > 0) {
		memcpy(&server->socket.pollfds.pollfds[n], server->socket.websocket.pollfds.pollfds, sizeof(struct pollfd) * server->socket.websocket.pollfds.length);
		n += server->socket.websocket.pollfds.length;
	}
	rc = poll(server->socket.pollfds.pollfds, n, milliseconds);
	if (rc == 0) {
		goto out;
	}
	if (rc < 0) {
		mbus_errorf("poll error");
		goto bail;
	}
	for (c = 0; c < n; c++) {
		if (server->socket.pollfds.pollfds[c].revents == 0) {
			continue;
		}
		if (server->socket.pollfds.pollfds[c].fd == mbus_socket_get_fd(server->socket.uds.socket) ||
		    server->socket.pollfds.pollfds[c].fd == mbus_socket_get_fd(server->socket.tcp.socket)) {
			if (server->socket.pollfds.pollfds[c].fd == mbus_socket_get_fd(server->socket.uds.socket)) {
				if (server->socket.pollfds.pollfds[c].revents & mbus_poll_event_in) {
					rc = server_accept_client(server, server->socket.uds.socket);
					if (rc == -1) {
						mbus_errorf("can not accept new connection");
						goto bail;
					} else if (rc == -2) {
						mbus_errorf("rejected new connection");
					}
				}
			}
			if (server->socket.pollfds.pollfds[c].fd == mbus_socket_get_fd(server->socket.tcp.socket)) {
				if (server->socket.pollfds.pollfds[c].revents & mbus_poll_event_in) {
					rc = server_accept_client(server, server->socket.tcp.socket);
					if (rc == -1) {
						mbus_errorf("can not accept new connection");
						goto bail;
					} else if (rc == -2) {
						mbus_errorf("rejected new connection");
					}
				}
			}
			continue;
		}
		client = server_find_client_by_fd(server, server->socket.pollfds.pollfds[c].fd);
		if (client == NULL) {
			continue;
			mbus_errorf("can not find client by socket");
			goto bail;
		}
		if (client_get_link(client) != client_link_tcp &&
		    client_get_link(client) != client_link_uds) {
			continue;
		}
		if (server->socket.pollfds.pollfds[c].revents & mbus_poll_event_in) {
			string = mbus_socket_read_string(client->socket);
			if (string == NULL) {
				mbus_debugf("can not read string from client");
				client_set_socket(client, NULL);
				mbus_infof("client: '%s' connection reset by peer", client_get_name(client));
				break;
			} else {
				mbus_debugf("new request from client: '%s', '%s'", client_get_name(client), string);
				rc = server_handle_method(server, client, string);
				if (rc != 0) {
					mbus_errorf("can not handle request, closing client: '%s' connection", client_get_name(client));
					client_set_socket(client, NULL);
				}
			}
			if (string != NULL) {
				free(string);
			}
		}
		if (server->socket.pollfds.pollfds[c].revents & mbus_poll_event_out) {
			if (client_get_results_count(client) > 0) {
				method = client_pop_result(client);
				if (method == NULL) {
					mbus_errorf("could not pop result from client");
					break;
				}
				string = method_get_result_string(method);
			} else if (client_get_requests_count(client) > 0) {
				method = client_pop_request(client);
				if (method == NULL) {
					mbus_errorf("could not pop request from client");
					break;
				}
				string = method_get_request_string(method);
			} else if (client_get_events_count(client) > 0) {
				method = client_pop_event(client);
				if (method == NULL) {
					mbus_errorf("could not pop event from client");
					break;
				}
				string = method_get_request_string(method);
			} else {
				mbus_errorf("logic error");
				goto bail;
			}
			if (string == NULL) {
				mbus_errorf("can not build string from method event");
				method_destroy(method);
				goto bail;
			}
			mbus_debugf("send method to client: %s", string);
			rc = mbus_socket_write_string(client_get_socket(client), string);
			if (rc != 0) {
				method_destroy(method);
				mbus_debugf("can not write string to client");
				client_set_socket(client, NULL);
				mbus_infof("client: '%s' connection reset by server", client_get_name(client));
				break;
			}
			method_destroy(method);
		}
		if (server->socket.pollfds.pollfds[c].revents & mbus_poll_event_err) {
			client_set_socket(client, NULL);
			mbus_infof("client: '%s' connection reset by server", client_get_name(client));
			break;
		}
	}
out:	lws_service(server->socket.websocket.context, 0);
	TAILQ_FOREACH(client, &server->clients, clients) {
		if (client_get_socket(client) == NULL) {
			continue;
		}
		if ((client_get_status(client) & client_status_connected) != 0) {
			continue;
		}
		client_set_status(client, client_get_status(client) | client_status_connected);
		mbus_infof("client: '%s' connected to server", client_get_name(client));
		rc = server_send_event_connected(server, client_get_name(client));
		if (rc != 0) {
			mbus_errorf("can not send connected event");
			goto bail;
		}
	}
	TAILQ_FOREACH_SAFE(client, &server->clients, clients, nclient) {
		if (client_get_socket(client) != NULL) {
			continue;
		}
		mbus_infof("client: '%s' disconnected from server", client_get_name(client));
		TAILQ_REMOVE(&server->clients, client, clients);
		TAILQ_FOREACH_SAFE(method, &server->methods, methods, nmethod) {
			if (method_get_source(method) != client) {
				continue;
			}
			TAILQ_REMOVE(&server->methods, method, methods);
			method_destroy(method);
		}
		TAILQ_FOREACH_SAFE(wclient, &server->clients, clients, nwclient) {
			TAILQ_FOREACH_SAFE(method, &wclient->waits, methods, nmethod) {
				if (strcmp(mbus_json_get_string_value(method_get_request_payload(method), "destination"), client_get_name(client)) == 0) {
					TAILQ_REMOVE(&wclient->waits, method, methods);
					method_set_result_code(method, -1);
					client_push_result(wclient, method);
				}
			}
		}
		if ((client_get_status(client) & client_status_connected) != 0) {
			rc = server_send_event_disconnected(server, client_get_name(client));
			if (rc != 0) {
				mbus_errorf("can not send disconnected event");
				goto bail;
			}
		}
		client_destroy(client);
	}
	{
		TAILQ_FOREACH_SAFE(client, &server->clients, clients, nclient) {
			struct websocket_client_data *data;
			if (client_get_socket(client) == NULL) {
				continue;
			}
			if (client_get_link(client) != client_link_websocket) {
				continue;
			}
			if (client_get_results_count(client) > 0) {
				method = client_pop_result(client);
				if (method == NULL) {
					mbus_errorf("could not pop result from client");
					continue;
				}
				string = method_get_result_string(method);
			} else if (client_get_requests_count(client) > 0) {
				method = client_pop_request(client);
				if (method == NULL) {
					mbus_errorf("could not pop request from client");
					continue;
				}
				string = method_get_request_string(method);
			} else if (client_get_events_count(client) > 0) {
				method = client_pop_event(client);
				if (method == NULL) {
					mbus_errorf("could not pop event from client");
					continue;
				}
				string = method_get_request_string(method);
			} else {
				continue;
			}
			if (string == NULL) {
				mbus_errorf("can not build string from method event");
				method_destroy(method);
				goto bail;
			}
			data = (struct websocket_client_data *) client_get_socket(client);
			mbus_debugf("push: %s", string);
			rc = websocket_client_data_buffer_push_string(&data->buffer.out, string);
			if (rc != 0) {
				mbus_errorf("can not push string");
				method_destroy(method);
				return -1;
			}
			lws_callback_on_writable(data->wsi);
			method_destroy(method);
		}
	}
	return (server->running == 0) ? 1 : 0;
bail:	return -1;
}

int mbus_server_run (struct mbus_server *server)
{
	int rc;
	if (server == NULL) {
		mbus_errorf("server is null");
		return -1;
	}
	mbus_infof("running server");
	while (server->running == 1) {
		rc = mbus_server_run_timeout(server, -1);
		if (rc < 0) {
			mbus_errorf("can not run server");
			goto bail;
		}
		if (rc == 1) {
			break;
		}
	}
	return 0;
bail:	return -1;
}

void mbus_server_destroy (struct mbus_server *server)
{
	struct client *client;
	struct method *method;
	if (server == NULL) {
		return;
	}
	if (server->socket.uds.socket != NULL) {
		mbus_socket_destroy(server->socket.uds.socket);
	}
	if (server->socket.tcp.socket != NULL) {
		mbus_socket_destroy(server->socket.tcp.socket);
	}
	if (server->socket.websocket.context != NULL) {
		lws_context_destroy(server->socket.websocket.context);
	}
	if (server->socket.websocket.pollfds.pollfds != NULL) {
		free(server->socket.websocket.pollfds.pollfds);
	}
	if (server->socket.pollfds.pollfds != NULL) {
		free(server->socket.pollfds.pollfds);
	}
	while (server->clients.tqh_first != NULL) {
		client = server->clients.tqh_first;
		TAILQ_REMOVE(&server->clients, server->clients.tqh_first, clients);
		client_destroy(client);
	}
	while (server->methods.tqh_first != NULL) {
		method = server->methods.tqh_first;
		TAILQ_REMOVE(&server->methods, server->methods.tqh_first, methods);
		method_destroy(method);
	}
	free(server);
}

struct mbus_server * mbus_server_create (int argc, char *_argv[])
{
	int a;
	int ch;
	int rc;
	char **argv;
	struct mbus_server *server;

	argv= NULL;
	server = NULL;

	mbus_infof("creating server");

	server = malloc(sizeof(struct mbus_server));
	if (server == NULL) {
		mbus_errorf("can not allocate memory");
		goto bail;
	}
	g_server = server;
	memset(server, 0, sizeof(struct mbus_server));
	TAILQ_INIT(&server->clients);
	TAILQ_INIT(&server->methods);

	server->socket.tcp.enabled = MBUS_SERVER_TCP_ENABLE;
	server->socket.tcp.address = MBUS_SERVER_TCP_ADDRESS;
	server->socket.tcp.port = MBUS_SERVER_TCP_PORT;

	server->socket.uds.enabled = MBUS_SERVER_UDS_ENABLE;
	server->socket.uds.address = MBUS_SERVER_UDS_ADDRESS;
	server->socket.uds.port = MBUS_SERVER_UDS_PORT;

	server->socket.websocket.enabled = MBUS_SERVER_WEBSOCKET_ENABLE;
	server->socket.websocket.address = MBUS_SERVER_WEBSOCKET_ADDRESS;
	server->socket.websocket.port = MBUS_SERVER_WEBSOCKET_PORT;

	argv = malloc(sizeof(char *) * (argc + 1));
	if (argv == NULL) {
		mbus_errorf("can not allocate memory");
		goto bail;
	}
	for (a = 0; a < argc; a++) {
		argv[a] = _argv[a];
	}
	argv[a] = NULL;

	optind = 1;
	while ((ch = getopt_long(argc, argv, ":", longopts, NULL)) != -1) {
		switch (ch) {
			case OPTION_DEBUG_LEVEL:
				mbus_debug_level = mbus_debug_level_from_string(optarg);
				break;
			case OPTION_SERVER_TCP_ENABLE:
				server->socket.tcp.enabled = !!atoi(optarg);
				break;
			case OPTION_SERVER_TCP_ADDRESS:
				server->socket.tcp.address = optarg;
				break;
			case OPTION_SERVER_TCP_PORT:
				server->socket.tcp.port = atoi(optarg);
				break;
			case OPTION_SERVER_UDS_ENABLE:
				server->socket.uds.enabled = !!atoi(optarg);
				break;
			case OPTION_SERVER_UDS_ADDRESS:
				server->socket.uds.address = optarg;
				break;
			case OPTION_SERVER_UDS_PORT:
				server->socket.uds.port = atoi(optarg);
				break;
			case OPTION_SERVER_WEBSOCKET_ENABLE:
				server->socket.websocket.enabled = !!atoi(optarg);
				break;
			case OPTION_SERVER_WEBSOCKET_ADDRESS:
				server->socket.websocket.address = optarg;
				break;
			case OPTION_SERVER_WEBSOCKET_PORT:
				server->socket.websocket.port = atoi(optarg);
				break;
			case OPTION_HELP:
				usage();
				goto bail;
		}
	}

	if (server->socket.tcp.enabled == 0 &&
	    server->socket.uds.enabled == 0 &&
	    server->socket.websocket.enabled == 0) {
		mbus_errorf("at leat one protocol must be enabled");
		goto bail;
	}

	if (server->socket.tcp.enabled == 1) {
		server->socket.tcp.socket = mbus_socket_create(mbus_socket_domain_af_inet, mbus_socket_type_sock_stream, mbus_socket_protocol_any);
		if (server->socket.tcp.socket == NULL) {
			mbus_errorf("can not create socket");
			goto bail;
		}
		rc = mbus_socket_set_reuseaddr(server->socket.tcp.socket, 1);
		if (rc != 0) {
			mbus_errorf("can not reuse socket");
			goto bail;
		}
		rc = mbus_socket_bind(server->socket.tcp.socket, server->socket.tcp.address, server->socket.tcp.port);
		if (rc != 0) {
			mbus_errorf("can not bind socket: '%s:%s:%d'", "tcp", server->socket.tcp.address, server->socket.tcp.port);
			goto bail;
		}
		rc = mbus_socket_listen(server->socket.tcp.socket, 1024);
		if (rc != 0) {
			mbus_errorf("can not listen socket");
			goto bail;
		}
		mbus_infof("listening from: '%s:%s:%d'", "tcp", server->socket.tcp.address, server->socket.tcp.port);
	}
	if (server->socket.uds.enabled == 1) {
		server->socket.uds.socket = mbus_socket_create(mbus_socket_domain_af_unix, mbus_socket_type_sock_stream, mbus_socket_protocol_any);
		if (server->socket.uds.socket == NULL) {
			mbus_errorf("can not create socket");
			goto bail;
		}
		rc = mbus_socket_set_reuseaddr(server->socket.uds.socket, 1);
		if (rc != 0) {
			mbus_errorf("can not reuse socket");
			goto bail;
		}
		rc = mbus_socket_bind(server->socket.uds.socket, server->socket.uds.address, server->socket.uds.port);
		if (rc != 0) {
			mbus_errorf("can not bind socket: '%s:%s:%d'", "uds", server->socket.uds.address, server->socket.uds.port);
			goto bail;
		}
		rc = mbus_socket_listen(server->socket.uds.socket, 1024);
		if (rc != 0) {
			mbus_errorf("can not listen socket");
			goto bail;
		}
		mbus_infof("listening from: '%s:%s:%d'", "uds", server->socket.uds.address, server->socket.uds.port);
	}
	if (server->socket.websocket.enabled == 1) {
		struct lws_context_creation_info info;
		memset(&info, 0, sizeof(info));
		info.iface = NULL;
		info.port = server->socket.websocket.port;
		info.protocols = websocket_protocols;
		info.extensions = websocket_extensions;
		info.gid = -1;
		info.uid = -1;
		server->socket.websocket.context = lws_create_context(&info);
		if (server->socket.websocket.context == NULL) {
			mbus_errorf("can not create websocket context for: '%s:%s:%d'", "websocket", server->socket.websocket.address, server->socket.websocket.port);
			goto bail;
		}
		mbus_infof("listening from: '%s:%s:%d'", "websocket", server->socket.websocket.address, server->socket.websocket.port);
	}

	free(argv);
	server->running = 1;
	return server;
bail:	mbus_server_destroy(server);
	if (argv != NULL) {
		free(argv);
	}
	return NULL;
}
