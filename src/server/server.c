
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

#define MBUS_DEBUG_NAME	"mbus-server"

#include "mbus/cJSON.h"
#include "mbus/debug.h"
#include "mbus/tailq.h"
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
		cJSON *payload;
		cJSON *json;
		char *string;
	} request;
	struct {
		cJSON *payload;
		cJSON *json;
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

enum client_status {
	client_status_connected		= 0x00000001,
};

struct client {
	TAILQ_ENTRY(client) clients;
	char *name;
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
			struct mbus_socket *socket;
		} websocket;
	} socket;
	struct clients clients;
	struct methods methods;
	int running;
};

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

static cJSON * method_get_request_payload (struct method *method)
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
	cJSON_AddNumberToObjectCS(method->result.json, "result", code);
	return 0;
}

static int method_add_result_payload (struct method *method, const char *name, cJSON *payload)
{
	if (method == NULL) {
		return -1;
	}
	cJSON_AddItemToObjectCS(method->result.payload, name, payload);
	return 0;
}

static int method_set_result_payload (struct method *method, cJSON *payload)
{
	if (method == NULL) {
		return -1;
	}
	cJSON_DeleteItemFromObject(method->result.json, "payload");
	cJSON_AddItemToObjectCS(method->result.json, "payload", payload);
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
	method->result.string = cJSON_Print(method->result.json);
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
	method->request.string = cJSON_Print(method->request.json);
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
		cJSON_Delete(method->request.json);
	}
	if (method->request.string != NULL) {
		free(method->request.string);
	}
	if (method->result.json != NULL) {
		cJSON_Delete(method->result.json);
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
	if (source == NULL) {
		mbus_errorf("source is null");
		goto bail;
	}
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
	method->request.json = cJSON_Parse(string);
	if (method->request.json == NULL) {
		mbus_errorf("can not parse method");
		goto bail;
	}
	method->request.type = cJSON_GetStringValue(method->request.json, "type");
	method->request.source = cJSON_GetStringValue(method->request.json, "source");
	method->request.destination = cJSON_GetStringValue(method->request.json, "destination");
	method->request.identifier = cJSON_GetStringValue(method->request.json, "identifier");
	method->request.sequence = cJSON_GetIntValue(method->request.json, "sequence");
	method->request.payload = cJSON_GetObjectItem(method->request.json, "payload");
	if ((method->request.source == NULL) ||
	    (method->request.destination == NULL) ||
	    (method->request.type == NULL) ||
	    (method->request.identifier == NULL) ||
	    (method->request.sequence == -1) ||
	    (method->request.payload == NULL)) {
		mbus_errorf("invalid method");
		goto bail;
	}
	method->result.json = cJSON_CreateObject();
	if (method->result.json == NULL) {
		mbus_errorf("can not create result");
		goto bail;
	}
	method->result.payload = cJSON_CreateObject();
	if (method->result.payload == NULL) {
		mbus_errorf("can not create result payload");
		goto bail;
	}
	cJSON_AddStringToObjectCS(method->result.json, "type", MBUS_METHOD_TYPE_RESULT);
	cJSON_AddNumberToObjectCS(method->result.json, "sequence", method->request.sequence);
	cJSON_AddItemToObjectCS(method->result.json, "payload", method->result.payload);
	method->source = source;
	return method;
bail:	method_destroy(method);
	return NULL;
}

static struct method * method_create (const char *type, const char *source, const char *destination, const char *identifier, int sequence, const cJSON *payload)
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
	method->request.payload = cJSON_Duplicate((cJSON *) payload, 1);
	if (method->request.payload == NULL) {
		mbus_errorf("can not create method payload");
		goto bail;
	}
	method->request.json = cJSON_CreateObject();
	if (method->request.json == NULL) {
		mbus_errorf("can not create method object");
		cJSON_Delete(method->request.payload);
		method->request.payload = NULL;
		goto bail;
	}
	cJSON_AddStringToObjectCS(method->request.json, "type", type);
	cJSON_AddStringToObjectCS(method->request.json, "source", source);
	cJSON_AddStringToObjectCS(method->request.json, "destination", destination);
	cJSON_AddStringToObjectCS(method->request.json, "identifier", identifier);
	cJSON_AddNumberToObjectCS(method->request.json, "sequence", sequence);
	cJSON_AddItemToObjectCS(method->request.json, "payload", method->request.payload);
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
			mbus_socket_destroy(client->socket);
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
		mbus_socket_destroy(client->socket);
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

static struct client * client_create (const char *name)
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
	client->ssequence = MBUS_METHOD_SEQUENCE_START;
	client->esequence = MBUS_METHOD_SEQUENCE_START;
	return client;
bail:	client_destroy(client);
	return NULL;
}

static struct client * server_create_client_by_name (struct mbus_server *server, const char *name)
{
	struct client *client;
	if (name == NULL) {
		mbus_errorf("name is null");
		return NULL;
	}
	TAILQ_FOREACH(client, &server->clients, clients) {
		if (strcmp(client_get_name(client), name) == 0) {
			mbus_errorf("client with name: '%s' already exists", name);
			return NULL;
		}
	}
	client = client_create(name);
	if (client == NULL) {
		mbus_errorf("can not create client");
		return NULL;
	}
	TAILQ_INSERT_TAIL(&server->clients, client, clients);
	return client;
}

static struct client * server_find_client_by_socket (struct mbus_server *server, struct mbus_socket *socket)
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

static int server_send_event_to (struct mbus_server *server, const char *source, const char *destination, const char *identifier, cJSON *payload)
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

static int server_send_status_to (struct mbus_server *server, const char *destination, const char *identifier, cJSON *payload)
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
	cJSON *payload;
	payload = NULL;
	if (server == NULL) {
		mbus_errorf("server is null");
		goto bail;
	}
	if (source == NULL) {
		mbus_errorf("source is null");
		goto bail;
	}
	payload = cJSON_CreateObject();
	if (payload == NULL) {
		mbus_errorf("can not create payload");
		goto bail;
	}
	cJSON_AddStringToObjectCS(payload, "source", source);
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
	cJSON_Delete(payload);
	return 0;
bail:	if (payload != NULL) {
		cJSON_Delete(payload);
	}
	return -1;
}

static int server_send_event_disconnected (struct mbus_server *server, const char *source)
{
	int rc;
	cJSON *payload;
	payload = NULL;
	if (server == NULL) {
		mbus_errorf("server is null");
		goto bail;
	}
	if (source == NULL) {
		mbus_errorf("source is null");
		goto bail;
	}
	payload = cJSON_CreateObject();
	if (payload == NULL) {
		mbus_errorf("can not create payload");
		goto bail;
	}
	cJSON_AddStringToObjectCS(payload, "source", source);
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
	cJSON_Delete(payload);
	return 0;
bail:	if (payload != NULL) {
		cJSON_Delete(payload);
	}
	return -1;
}

static int server_send_event_subscribed (struct mbus_server *server, const char *source, const char *destination, const char *identifier)
{
	int rc;
	cJSON *payload;
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
	payload = cJSON_CreateObject();
	if (payload == NULL) {
		mbus_errorf("can not create payload");
		goto bail;
	}
	cJSON_AddStringToObjectCS(payload, "destination", destination);
	cJSON_AddStringToObjectCS(payload, "identifier", identifier);
	rc = server_send_status_to(server, source, MBUS_SERVER_STATUS_SUBSCRIBED, payload);
	if (rc != 0) {
		mbus_errorf("can not send event");
		goto bail;
	}
	cJSON_AddStringToObjectCS(payload, "source", source);
	rc = server_send_event_to(server, MBUS_SERVER_NAME, MBUS_METHOD_EVENT_DESTINATION_SUBSCRIBERS, MBUS_SERVER_EVENT_SUBSCRIBED, payload);
	if (rc != 0) {
		mbus_errorf("can not send event");
		goto bail;
	}
	cJSON_Delete(payload);
	return 0;
bail:	if (payload != NULL) {
		cJSON_Delete(payload);
	}
	return -1;
}

static int server_send_event_subscriber (struct mbus_server *server, const char *source, const char *destination, const char *identifier)
{
	int rc;
	cJSON *payload;
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
	payload = cJSON_CreateObject();
	if (payload == NULL) {
		mbus_errorf("can not create payload");
		goto bail;
	}
	cJSON_AddStringToObjectCS(payload, "source", source);
	cJSON_AddStringToObjectCS(payload, "identifier", identifier);
	rc = server_send_status_to(server, destination, MBUS_SERVER_STATUS_SUBSCRIBER, payload);
	if (rc != 0) {
		mbus_errorf("can not send event");
		goto bail;
	}
	cJSON_Delete(payload);
	return 0;
bail:	if (payload != NULL) {
		cJSON_Delete(payload);
	}
	return -1;
}

static int server_accept_client (struct mbus_server *server, struct mbus_socket *from)
{
	int rc;
	char *name;
	struct client *client;
	struct mbus_socket *socket;
	name = NULL;
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
	name = mbus_socket_read_string(socket);
	if (name == NULL) {
		mbus_errorf("can not read name");
		goto bail;
	}
	TAILQ_FOREACH(client, &server->clients, clients) {
		if (strcmp(client_get_name(client), name) == 0) {
			break;
		}
	}
	if (client != NULL) {
		mbus_errorf("client with name: '%s' already exists", name);
		client = NULL;
		goto exists;
	}
	client = server_create_client_by_name(server, name);
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
	free(name);
	return 0;
bail:	if (socket != NULL) {
		mbus_socket_destroy(socket);
	}
	if (client != NULL) {
		client_destroy(client);
	}
	if (name != NULL) {
		free(name);
	}
	return -1;
exists:	if (socket != NULL) {
		mbus_socket_destroy(socket);
	}
	if (client != NULL) {
		client_destroy(client);
	}
	if (name != NULL) {
		free(name);
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
	source = cJSON_GetStringValue(method_get_request_payload(method), "source");
	event = cJSON_GetStringValue(method_get_request_payload(method), "event");
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
	command = cJSON_GetStringValue(method_get_request_payload(method), "command");
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
	cJSON *event;
	if (server == NULL) {
		mbus_errorf("server is null");
		goto bail;
	}
	if (method == NULL) {
		mbus_errorf("method is null");
		goto bail;
	}
	destination = cJSON_GetStringValue(method_get_request_payload(method), "destination");
	identifier = cJSON_GetStringValue(method_get_request_payload(method), "identifier");
	event = cJSON_GetObjectItem(method_get_request_payload(method), "event");
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
	cJSON *call;
	response = 1;
	if (server == NULL) {
		mbus_errorf("server is null");
		goto bail;
	}
	if (method == NULL) {
		mbus_errorf("method is null");
		goto bail;
	}
	destination = cJSON_GetStringValue(method_get_request_payload(method), "destination");
	identifier = cJSON_GetStringValue(method_get_request_payload(method), "identifier");
	call = cJSON_GetObjectItem(method_get_request_payload(method), "call");
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
			cJSON *object;
			cJSON *source;
			cJSON *clients;
			cJSON *commands;
			cJSON *subscribes;
			struct command *command;
			struct subscription *subscription;
			clients = NULL;
			clients = cJSON_CreateArray();
			if (clients == NULL) {
				goto command_status_bail;
			}
			TAILQ_FOREACH(client, &server->clients, clients) {
				source = cJSON_CreateObject();
				if (source == NULL) {
					goto command_status_bail;
				}
				cJSON_AddItemToArray(clients, source);
				cJSON_AddStringToObjectCS(source, "source", client_get_name(client));
				subscribes = cJSON_CreateArray();
				if (subscribes == NULL) {
					goto command_status_bail;
				}
				cJSON_AddItemToObjectCS(source, "subscriptions", subscribes);
				TAILQ_FOREACH(subscription, &client->subscriptions, subscriptions) {
					object = cJSON_CreateObject();
					if (object == NULL) {
						goto command_status_bail;
					}
					cJSON_AddItemToArray(subscribes, object);
					cJSON_AddStringToObjectCS(object, "source", subscription_get_source(subscription));
					cJSON_AddStringToObjectCS(object, "identifier", subscription_get_event(subscription));
				}
				commands = cJSON_CreateArray();
				if (commands == NULL) {
					goto command_status_bail;
				}
				cJSON_AddItemToObjectCS(source, "commands", commands);
				TAILQ_FOREACH(command, &client->commands, commands) {
					object = cJSON_CreateObject();
					if (object == NULL) {
						goto command_status_bail;
					}
					cJSON_AddItemToArray(commands, object);
					cJSON_AddStringToObjectCS(object, "identifier", command_get_identifier(command));
				}
			}
			method_add_result_payload(method, "clients", clients);
			goto out;
command_status_bail:
			if (clients != NULL) {
				cJSON_Delete(clients);
			}
		} else if (strcmp(identifier, MBUS_SERVER_COMMAND_CLOSE) == 0) {
			const char *source;
			source = cJSON_GetStringValue(call, "source");
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
	destination = cJSON_GetStringValue(method_get_request_payload(method), "destination");
	identifier = cJSON_GetStringValue(method_get_request_payload(method), "identifier");
	sequence = cJSON_GetIntValue(method_get_request_payload(method), "sequence");
	rc = cJSON_GetIntValue(method_get_request_payload(method), "return");
	TAILQ_FOREACH(client, &server->clients, clients) {
		if (strcmp(client_get_name(client), destination) != 0) {
			continue;
		}
		TAILQ_FOREACH_SAFE(wait, &client->waits, methods, nwait) {
			if (sequence != method_get_request_sequence(wait)) {
				continue;
			}
			if (strcmp(cJSON_GetStringValue(method_get_request_payload(wait), "destination"), source) != 0) {
				continue;
			}
			if (strcmp(cJSON_GetStringValue(method_get_request_payload(wait), "identifier"), identifier) != 0) {
				continue;
			}
			TAILQ_REMOVE(&client->waits, wait, methods);
			method_set_result_code(wait, rc);
			method_set_result_payload(wait, cJSON_Duplicate(cJSON_GetObjectItem(method_get_request_payload(method), "result"), 1));
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
	struct mbus_poll *polls;
	if (server == NULL) {
		mbus_errorf("server is null");
		return -1;
	}
	mbus_debugf("running server");
	polls = NULL;
	if (server->running == 0) {
		goto out;
	}
	rc = server_handle_methods(server);
	if (rc != 0) {
		mbus_errorf("can not handle methods");
		goto bail;
	}
	if (polls != NULL) {
		free(polls);
		polls = NULL;
	}
	n = 3 + server->clients.count;
	polls = malloc(sizeof(struct mbus_poll) * n);
	if (polls == NULL) {
		mbus_errorf("can not allocate memory");
		goto bail;
	}
	n = 0;
	if (server->socket.uds.socket != NULL) {
		polls[n].events = mbus_poll_event_in;
		polls[n].revents = 0;
		polls[n].socket = server->socket.uds.socket;
		n += 1;
	}
	if (server->socket.tcp.socket != NULL) {
		polls[n].events = mbus_poll_event_in;
		polls[n].revents = 0;
		polls[n].socket = server->socket.tcp.socket;
		n += 1;
	}
	if (server->socket.websocket.socket != NULL) {
		polls[n].events = mbus_poll_event_in;
		polls[n].revents = 0;
		polls[n].socket = server->socket.websocket.socket;
		n += 1;
	}
	TAILQ_FOREACH(client, &server->clients, clients) {
		if (client_get_socket(client) != NULL) {
			polls[n].events = mbus_poll_event_in;
			polls[n].revents = 0;
			polls[n].socket = client_get_socket(client);
			if (client_get_requests_count(client) != 0) {
				polls[n].events |= mbus_poll_event_out;
			}
			if (client_get_results_count(client) != 0) {
				polls[n].events |= mbus_poll_event_out;
			}
			if (client_get_events_count(client) != 0) {
				polls[n].events |= mbus_poll_event_out;
			}
			n += 1;
		}
	}
	rc = mbus_socket_poll(polls, n, milliseconds);
	if (rc == 0) {
		goto out;
	}
	if (rc < 0) {
		mbus_errorf("poll error");
		goto bail;
	}
	for (c = 0; c < n; c++) {
		if (polls[c].revents == 0) {
			continue;
		}
		if (polls[c].socket == server->socket.uds.socket ||
		    polls[c].socket == server->socket.tcp.socket ||
		    polls[c].socket == server->socket.websocket.socket) {
			if (polls[c].socket == server->socket.uds.socket) {
				if (polls[c].revents & mbus_poll_event_in) {
					rc = server_accept_client(server, server->socket.uds.socket);
					if (rc == -1) {
						mbus_errorf("can not accept new connection");
						goto bail;
					} else if (rc == -2) {
						mbus_errorf("rejected new connection");
					}
				}
			}
			if (polls[c].socket == server->socket.tcp.socket) {
				if (polls[c].revents & mbus_poll_event_in) {
					rc = server_accept_client(server, server->socket.tcp.socket);
					if (rc == -1) {
						mbus_errorf("can not accept new connection");
						goto bail;
					} else if (rc == -2) {
						mbus_errorf("rejected new connection");
					}
				}
			}
			if (polls[c].socket == server->socket.websocket.socket) {
				if (polls[c].revents & mbus_poll_event_in) {
					rc = server_accept_client(server, server->socket.websocket.socket);
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
		client = server_find_client_by_socket(server, polls[c].socket);
		if (client == NULL) {
			mbus_errorf("can not find client by socket");
			goto bail;
		}
		if (polls[c].revents & mbus_poll_event_in) {
			string = mbus_socket_read_string(polls[c].socket);
			if (string == NULL) {
				mbus_debugf("can not read string from client");
				client_set_socket(client, NULL);
				mbus_infof("client: '%s' connection reset by peer", client_get_name(client));
				break;
			} else {
				mbus_debugf("new request from client: '%s', '%s'", client_get_name(client), string);
				if (client_get_socket(client) == polls[c].socket) {
					rc = server_handle_method(server, client, string);
				} else {
					mbus_errorf("invalid socket, client mismatch");
					free(string);
					goto bail;
				}
				free(string);
				if (rc != 0) {
					mbus_errorf("can not handle request, closing client: '%s' connection", client_get_name(client));
					client_set_socket(client, NULL);
				}
			}
		}
		if (polls[c].revents & mbus_poll_event_out) {
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
		if (polls[c].revents & mbus_poll_event_err) {
			client_set_socket(client, NULL);
			mbus_infof("client: '%s' connection reset by server", client_get_name(client));
			break;
		}
	}
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
				if (strcmp(cJSON_GetStringValue(method_get_request_payload(method), "destination"), client_get_name(client)) == 0) {
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
out:	if (polls != NULL) {
		free(polls);
		polls = NULL;
	}
	return (server->running == 0) ? 1 : 0;
bail:	if (polls != NULL) {
		free(polls);
		polls = NULL;
	}
	return -1;
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
	if (server->socket.websocket.socket != NULL) {
		mbus_socket_destroy(server->socket.websocket.socket);
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
