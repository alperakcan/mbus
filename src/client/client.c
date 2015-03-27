
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
#include <getopt.h>
#include <errno.h>
#include <pthread.h>
#include <sys/time.h>

#include "cJSON.h"

#define MBUS_DEBUG_NAME	"mbus-client"

#include "debug.h"
#include "tailq.h"
#include "method.h"
#include "socket.h"
#include "server.h"
#include "client.h"

enum method_type {
	method_type_unknown,
	method_type_status,
	method_type_event,
	method_type_command,
	method_type_result,
};

struct method {
	TAILQ_ENTRY(method) methods;
	const char *type;
	const char *source;
	const char *destination;
	const char *identifier;
	int sequence;
	int result;
	cJSON *payload;
	cJSON *json;
};
TAILQ_HEAD(methods, method);

enum request_state {
	request_state_detached,
	request_state_request,
	request_state_wait,
};

struct request {
	TAILQ_ENTRY(request) requests;
	int sequence;
	cJSON *payload;
	cJSON *json;
	char *string;
	struct method *result;
	enum request_state state;
};
TAILQ_HEAD(requests, request);

struct callback {
	TAILQ_ENTRY(callback) callbacks;
	char *source;
	char *identifier;
	void (*function) (struct mbus_client *client, const char *source, const char *event, cJSON *payload, void *data);
	void *data;
};
TAILQ_HEAD(callbacks, callback);

struct command {
	TAILQ_ENTRY(command) commands;
	char *identifier;
	int (*function) (struct mbus_client *client, const char *source, const char *command, cJSON *payload, cJSON *result, void *data);
	void *data;
};
TAILQ_HEAD(commands, command);

struct mbus_client {
	char *name;
	int sequence;
	struct mbus_socket *socket;
	struct methods methods;
	struct requests requests;
	struct requests waitings;
	struct callbacks callbacks;
	struct commands commands;
	int incommand;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	pthread_t thread;
	int started;
	int running;
	int stopped;
	int error;
};

#define OPTION_HELP		0x100
#define OPTION_DEBUG_LEVEL	0x101
#define OPTION_SERVER_PROTOCOL	0x102
#define OPTION_SERVER_ADDRESS	0x103
#define OPTION_SERVER_PORT	0x104
#define OPTION_CLIENT_NAME	0x105
static struct option longopts[] = {
	{ "mbus-help",			no_argument,		NULL,	OPTION_HELP },
	{ "mbus-debug-level",		required_argument,	NULL,	OPTION_DEBUG_LEVEL },
	{ "mbus-server-protocol",	required_argument,	NULL,	OPTION_SERVER_PROTOCOL },
	{ "mbus-server-address",	required_argument,	NULL,	OPTION_SERVER_ADDRESS },
	{ "mbus-server-port",		required_argument,	NULL,	OPTION_SERVER_PORT },
	{ "mbus-client-name",		required_argument,	NULL,	OPTION_CLIENT_NAME },
	{ NULL,				0,			NULL,	0 },
};

static void usage (void)
{
	fprintf(stdout, "mbus client arguments:\n");
	fprintf(stdout, "  --mbus-debug-level     : debug level (default: %s)\n", mbus_debug_level_to_string(mbus_debug_level));
	fprintf(stdout, "  --mbus-server-protocol : server protocol (default: %s)\n", MBUS_SERVER_PROTOCOL);
	fprintf(stdout, "  --mbus-server-address  : server address (default: %s)\n", MBUS_SERVER_ADDRESS);
	fprintf(stdout, "  --mbus-server-port     : server port (default: %d)\n", MBUS_SERVER_PORT);
	fprintf(stdout, "  --mbus-client-name     : client name (overrides api parameter)\n");
}

static const char * callback_get_identifier (struct callback *callback)
{
	if (callback == NULL) {
		return NULL;
	}
	return callback->identifier;
}

static const char * callback_get_source (struct callback *callback)
{
	if (callback == NULL) {
		return NULL;
	}
	return callback->source;
}

static void callback_destroy (struct callback *callback)
{
	if (callback == NULL) {
		return;
	}
	if (callback->source != NULL) {
		free(callback->source);
	}
	if (callback->identifier != NULL) {
		free(callback->identifier);
	}
	free(callback);
}

static struct callback * callback_create (const char *source, const char *identifier, void (*function) (struct mbus_client *client, const char *source, const char *event, cJSON *payload, void *data), void *data)
{
	struct callback *callback;
	callback = NULL;
	if (identifier == NULL) {
		mbus_errorf("identifier is null");
		goto bail;
	}
	if (function == NULL) {
		mbus_errorf("function is null");
		goto bail;
	}
	callback = malloc(sizeof(struct callback));
	if (callback == NULL) {
		mbus_errorf("can not allocate memory");
		goto bail;
	}
	if (source != NULL) {
		callback->source = strdup(source);
		if (callback->source == NULL) {
			mbus_errorf("can not allocate memory");
			goto bail;
		}
	}
	callback->identifier = strdup(identifier);
	if (callback->identifier == NULL) {
		mbus_errorf("can not allocate memory");
		goto bail;
	}
	callback->function = function;
	callback->data = data;
	return callback;
bail:	if (callback != NULL) {
		callback_destroy(callback);
	}
	return NULL;
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

static struct command * command_create (const char *identifier, int (*function) (struct mbus_client *client, const char *source, const char *event, cJSON *payload, cJSON *result, void *data), void *data)
{
	struct command *command;
	command = NULL;
	if (identifier == NULL) {
		mbus_errorf("identifier is null");
		goto bail;
	}
	if (function == NULL) {
		mbus_errorf("function is null");
		goto bail;
	}
	command = malloc(sizeof(struct command));
	if (command == NULL) {
		mbus_errorf("can not allocate memory");
		goto bail;
	}
	command->identifier = strdup(identifier);
	if (command->identifier == NULL) {
		mbus_errorf("can not allocate memory");
		goto bail;
	}
	command->function = function;
	command->data = data;
	return command;
bail:	if (command != NULL) {
		command_destroy(command);
	}
	return NULL;
}

static cJSON * method_get_payload (struct method *method)
{
	if (method == NULL) {
		return NULL;
	}
	return method->payload;
}

static const char * method_get_identifier (struct method *method)
{
	if (method == NULL) {
		return NULL;
	}
	return method->identifier;
}

static const char * method_get_source (struct method *method)
{
	if (method == NULL) {
		return NULL;
	}
	return method->source;
}

static enum method_type method_get_type (struct method *method)
{
	if (method == NULL) {
		return method_type_unknown;
	}
	if (strcmp(method->type, MBUS_METHOD_TYPE_RESULT) == 0) {
		return method_type_result;
	} else if (strcmp(method->type, MBUS_METHOD_TYPE_EVENT) == 0) {
		return method_type_event;
	} else if (strcmp(method->type, MBUS_METHOD_TYPE_STATUS) == 0) {
		return method_type_status;
	} else if (strcmp(method->type, MBUS_METHOD_TYPE_COMMAND) == 0) {
		return method_type_command;
	} else {
		return method_type_unknown;
	}
}

static int method_get_sequence (struct method *method)
{
	if (method == NULL) {
		return -1;
	}
	return method->sequence;
}

static int method_get_result (struct method *method)
{
	if (method == NULL) {
		return -1;
	}
	return method->result;
}

static void method_destroy (struct method *method)
{
	if (method == NULL) {
		return;
	}
	if (method->json != NULL) {
		cJSON_Delete(method->json);
	}
	free(method);
}

static struct method * method_create_from_string (const char *string)
{
	struct method *method;
	method = NULL;
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
	method->json = cJSON_Parse(string);
	if (method->json == NULL) {
		mbus_errorf("can not parse method");
		goto bail;
	}
	method->type = cJSON_GetStringValue(method->json, "type");
	method->source = cJSON_GetStringValue(method->json, "source");
	method->destination = cJSON_GetStringValue(method->json, "destination");
	method->identifier = cJSON_GetStringValue(method->json, "identifier");
	method->sequence = cJSON_GetIntValue(method->json, "sequence");
	method->result = cJSON_GetIntValue(method->json, "result");
	method->payload = cJSON_GetObjectItem(method->json, "payload");
	if (method->type == NULL) {
		mbus_errorf("invalid method");
		goto bail;
	}
	if (strcmp(method->type, MBUS_METHOD_TYPE_RESULT) == 0) {
		if ((method->sequence == -1) ||
		    (method->payload == NULL)) {
			mbus_errorf("invalid method");
			goto bail;
		}
	} else if (strcmp(method->type, MBUS_METHOD_TYPE_EVENT) == 0) {
		if ((method->source == NULL) ||
		    (method->destination == NULL) ||
		    (method->type == NULL) ||
		    (method->identifier == NULL) ||
		    (method->sequence == -1) ||
		    (method->payload == NULL)) {
			mbus_errorf("invalid method");
			goto bail;
		}
	} else if (strcmp(method->type, MBUS_METHOD_TYPE_STATUS) == 0) {
		if ((method->source == NULL) ||
		    (method->destination == NULL) ||
		    (method->type == NULL) ||
		    (method->identifier == NULL) ||
		    (method->sequence == -1) ||
		    (method->payload == NULL)) {
			mbus_errorf("invalid method");
			goto bail;
		}
	} else if (strcmp(method->type, MBUS_METHOD_TYPE_COMMAND) == 0) {
		if ((method->source == NULL) ||
		    (method->destination == NULL) ||
		    (method->type == NULL) ||
		    (method->identifier == NULL) ||
		    (method->sequence == -1) ||
		    (method->payload == NULL)) {
			mbus_errorf("invalid method");
			goto bail;
		}
	} else {
		mbus_errorf("invalid method type: %s", method->type);
		goto bail;
	}
	return method;
bail:	method_destroy(method);
	return NULL;
}

static char * request_get_string (struct request *request)
{
	if (request == NULL) {
		return NULL;
	}
	if (request->string != NULL) {
		free(request->string);
	}
	request->string = cJSON_Print(request->json);
	return request->string;
}

static int request_get_sequence (struct request *request)
{
	if (request == NULL) {
		return -1;
	}
	return request->sequence;
}

static int request_set_result (struct request *request, struct method *result)
{
	if (request == NULL) {
		return -1;
	}
	request->result = result;
	return 0;
}

static struct method * request_get_result (struct request *request)
{
	if (request == NULL) {
		return NULL;
	}
	return request->result;
}

static void request_destroy (struct request *request)
{
	if (request == NULL) {
		return;
	}
	if (request->json != NULL) {
		cJSON_Delete(request->json);
	}
	if (request->string != NULL) {
		free(request->string);
	}
	if (request->result != NULL) {
		method_destroy(request->result);
	}
	free(request);
}

static struct request * request_create (const char *type, const char *source, const char *destination, const char *identifier, int sequence, const cJSON *payload)
{
	struct request *request;
	request = NULL;
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
	request = malloc(sizeof(struct request));
	if (request == NULL) {
		mbus_errorf("can not allocate memory");
		goto bail;
	}
	memset(request, 0, sizeof(struct request));
	request->state = request_state_detached;
	request->sequence = sequence;
	request->payload = cJSON_Duplicate((cJSON *) payload, 1);
	if (request->payload == NULL) {
		mbus_errorf("can not create request payload");
		goto bail;
	}
	request->json = cJSON_CreateObject();
	if (request->json == NULL) {
		mbus_errorf("can not create request object");
		cJSON_Delete(request->payload);
		request->payload = NULL;
		goto bail;
	}
	cJSON_AddStringToObjectCS(request->json, "type", type);
	cJSON_AddStringToObjectCS(request->json, "source", source);
	cJSON_AddStringToObjectCS(request->json, "destination", destination);
	cJSON_AddStringToObjectCS(request->json, "identifier", identifier);
	cJSON_AddNumberToObjectCS(request->json, "sequence", sequence);
	cJSON_AddItemToObjectCS(request->json, "payload", request->payload);
	return request;
bail:	if (request != NULL) {
		request_destroy(request);
	}
	return NULL;
}

static void * client_worker (void *arg)
{
	int rc;
	char *string;
	struct method *method;
	struct request *request;
	struct request *waiting;
	struct mbus_poll polls[1];
	struct mbus_client *client = arg;

	pthread_mutex_lock(&client->mutex);
	client->started = 1;
	client->running = 1;
	client->stopped = 0;
	pthread_cond_broadcast(&client->cond);
	pthread_mutex_unlock(&client->mutex);

	while (1) {
		sched_yield();
		pthread_mutex_lock(&client->mutex);
		if (client->running == 0) {
			pthread_mutex_unlock(&client->mutex);
			break;
		}
		polls[0].events = mbus_poll_event_in;
		polls[0].revents = 0;
		polls[0].socket = client->socket;
		if (client->requests.count > 0) {
			polls[0].events |= mbus_poll_event_out;
		}
		pthread_mutex_unlock(&client->mutex);
		rc = mbus_socket_poll(polls, 1, 20);
		if (rc == 0) {
			continue;
		}
		if (rc < 0) {
			mbus_errorf("poll error");
			goto bail;
		}
		if (polls[0].revents & mbus_poll_event_in) {
			string = mbus_socket_read_string(polls[0].socket);
			if (string == NULL) {
				mbus_infof("client: '%s' connection reset by peer", client->name);
				goto bail;
			}
			mbus_debugf("request from server: %s", string);
			method = method_create_from_string(string);
			if (method == NULL) {
				mbus_errorf("method create failed");
				free(string);
				goto bail;
			}
			pthread_mutex_lock(&client->mutex);
			switch (method_get_type(method)) {
				case method_type_result:
					TAILQ_FOREACH(waiting, &client->waitings, requests) {
						if (request_get_sequence(waiting) != method_get_sequence(method)) {
							continue;
						}
						request_set_result(waiting, method);
						TAILQ_REMOVE(&client->waitings, waiting, requests);
						waiting->state = request_state_detached;
						pthread_cond_broadcast(&client->cond);
						break;
					}
					if (waiting == NULL) {
						mbus_errorf("unknown result sequence");
						pthread_mutex_unlock(&client->mutex);
						free(string);
						method_destroy(method);
						goto bail;
					}
					break;
				case method_type_status:
				case method_type_event:
				case method_type_command:
					TAILQ_INSERT_TAIL(&client->methods, method, methods);
					pthread_cond_broadcast(&client->cond);
					break;
				default:
					mbus_errorf("unknown method type");
					pthread_mutex_unlock(&client->mutex);
					free(string);
					method_destroy(method);
					goto bail;
			}
			pthread_mutex_unlock(&client->mutex);
			free(string);
		}
		if (polls[0].revents & mbus_poll_event_out) {
			pthread_mutex_lock(&client->mutex);
			if (client->requests.count > 0) {
				request = client->requests.tqh_first;
				TAILQ_REMOVE(&client->requests, request, requests);
				request->state = request_state_detached;
				pthread_cond_broadcast(&client->cond);
			} else {
				request = NULL;
			}
			pthread_mutex_unlock(&client->mutex);
			if (request != NULL) {
				string = request_get_string(request);
				if (string == NULL) {
					mbus_errorf("could not get request string");
					goto bail;
				}
				mbus_debugf("request to server: %s", string);
				rc = mbus_socket_write_string(client->socket, string);
				if (rc != 0) {
					mbus_errorf("could not send request string");
					goto bail;
				}
				pthread_mutex_lock(&client->mutex);
				TAILQ_INSERT_TAIL(&client->waitings, request, requests);
				request->state = request_state_wait;
				pthread_cond_broadcast(&client->cond);
				pthread_mutex_unlock(&client->mutex);
			}
		}
	}
	pthread_mutex_lock(&client->mutex);
	client->started = 1;
	client->running = 0;
	client->stopped = 1;
	pthread_cond_broadcast(&client->cond);
	pthread_mutex_unlock(&client->mutex);
	return NULL;

bail:
	pthread_mutex_lock(&client->mutex);
	client->started = 1;
	client->running = 0;
	client->stopped = 1;
	client->error = 1;
	pthread_cond_broadcast(&client->cond);
	pthread_mutex_unlock(&client->mutex);
	return NULL;
}

struct mbus_client * mbus_client_create (const char *name, int argc, char *_argv[])
{
	int a;
	int ch;
	int rc;
	char **argv;
	struct mbus_client *client;

	int server_port;
	const char *server_address;
	const char *server_protocol;
	const char *client_name;

	enum mbus_socket_type socket_type;
	enum mbus_socket_domain socket_domain;

	argv = NULL;
	client = NULL;
	server_port = -1;
	server_address = NULL;
	server_protocol = NULL;

	if (name == NULL) {
		mbus_errorf("name is null");
		goto bail;
	}

	client_name = name;

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
			case OPTION_SERVER_PROTOCOL:
				server_protocol = optarg;
				break;
			case OPTION_SERVER_ADDRESS:
				server_address = optarg;
				break;
			case OPTION_SERVER_PORT:
				server_port = atoi(optarg);
				break;
			case OPTION_CLIENT_NAME:
				client_name = optarg;
				break;
			case OPTION_HELP:
				usage();
				goto bail;
		}
	}

	if (server_protocol == NULL) {
		server_protocol = MBUS_SERVER_PROTOCOL;
	}

	if (strcmp(server_protocol, MBUS_SERVER_TCP_PROTOCOL) == 0) {
		if (server_port == -1) {
			server_port = MBUS_SERVER_TCP_PORT;
		}
		if (server_address == NULL) {
			server_address = MBUS_SERVER_TCP_ADDRESS;
		}
		socket_domain = mbus_socket_domain_af_inet;
		socket_type = mbus_socket_type_sock_stream;
	} else if (strcmp(server_protocol, MBUS_SERVER_UDS_PROTOCOL) == 0) {
		if (server_port == -1) {
			server_port = MBUS_SERVER_UDS_PORT;
		}
		if (server_address == NULL) {
			server_address = MBUS_SERVER_UDS_ADDRESS;
		}
		socket_domain = mbus_socket_domain_af_unix;
		socket_type = mbus_socket_type_sock_stream;
	} else {
		mbus_errorf("invalid server protocol");
		goto bail;
	}

	mbus_infof("creating client: '%s'", client_name);

	client = malloc(sizeof(struct mbus_client));
	if (client == NULL) {
		mbus_errorf("can not allocate memory");
		goto bail;
	}
	memset(client, 0, sizeof(struct mbus_client));
	TAILQ_INIT(&client->methods);
	TAILQ_INIT(&client->requests);
	TAILQ_INIT(&client->waitings);
	TAILQ_INIT(&client->callbacks);
	TAILQ_INIT(&client->commands);
	pthread_mutex_init(&client->mutex, NULL);
	pthread_cond_init(&client->cond, NULL);
	client->name = strdup(client_name);
	if (client->name == NULL) {
		mbus_errorf("can not allocate memory");
		goto bail;
	}
	client->socket = mbus_socket_create(socket_domain, socket_type, mbus_socket_protocol_any);
	if (client->socket == NULL) {
		mbus_errorf("can not create event socket");
		goto bail;
	}
	rc = mbus_socket_set_reuseaddr(client->socket, 1);
	if (rc != 0) {
		mbus_errorf("can not reuse event");
		goto bail;
	}
	mbus_infof("connecting to server: '%s:%s:%d'", server_protocol, server_address, server_port);
	rc = mbus_socket_connect(client->socket, server_address, server_port);
	if (rc != 0) {
		mbus_errorf("can not connect to server: '%s:%s:%d'", server_protocol, server_address, server_port);
		goto bail;
	}
	rc = mbus_socket_write_string(client->socket, client->name);
	if (rc != 0) {
		mbus_errorf("can not write to socket");
		goto bail;
	}
	client->incommand = 0;
	client->sequence = MBUS_METHOD_SEQUENCE_START;
	pthread_mutex_lock(&client->mutex);
	pthread_create(&client->thread, NULL, client_worker, client);
	while (client->started == 0) {
		pthread_cond_wait(&client->cond, &client->mutex);
	}
	pthread_mutex_unlock(&client->mutex);
	free(argv);
	return client;
bail:	if (client != NULL) {
		mbus_client_destroy(client);
	}
	if (argv != NULL) {
		free(argv);
	}
	return NULL;
}

void mbus_client_destroy (struct mbus_client *client)
{
	struct method *method;
	struct request *request;
	struct request *waiting;
	struct callback *callback;
	struct command *command;
	if (client == NULL) {
		return;
	}
	mbus_infof("destroying client: '%s'", client->name);
	pthread_mutex_lock(&client->mutex);
	client->running = 0;
	pthread_cond_broadcast(&client->cond);
	while (client->started == 1 && client->stopped == 0) {
		pthread_cond_wait(&client->cond, &client->mutex);
	}
	pthread_mutex_unlock(&client->mutex);
	if (client->started == 1) {
		pthread_join(client->thread, NULL);
	}
	pthread_cond_destroy(&client->cond);
	pthread_mutex_destroy(&client->mutex);
	if (client->socket != NULL) {
		mbus_socket_destroy(client->socket);
	}
	if (client->name != NULL) {
		free(client->name);
	}
	while (client->methods.tqh_first != NULL) {
		method = client->methods.tqh_first;
		TAILQ_REMOVE(&client->methods, client->methods.tqh_first, methods);
		method_destroy(method);
	}
	while (client->requests.tqh_first != NULL) {
		request = client->requests.tqh_first;
		TAILQ_REMOVE(&client->requests, client->requests.tqh_first, requests);
		request_destroy(request);
	}
	while (client->waitings.tqh_first != NULL) {
		waiting = client->waitings.tqh_first;
		TAILQ_REMOVE(&client->waitings, client->waitings.tqh_first, requests);
		request_destroy(waiting);
	}
	while (client->callbacks.tqh_first != NULL) {
		callback = client->callbacks.tqh_first;
		TAILQ_REMOVE(&client->callbacks, client->callbacks.tqh_first, callbacks);
		callback_destroy(callback);
	}
	while (client->commands.tqh_first != NULL) {
		command = client->commands.tqh_first;
		TAILQ_REMOVE(&client->commands, client->commands.tqh_first, commands);
		command_destroy(command);
	}
	free(client);
}

static int __pthread_cond_timedwait (pthread_cond_t *cond, pthread_mutex_t *mutex, int msec)
{
        int ret;
        struct timeval tval;
        struct timespec tspec;

        if (msec < 0) {
                return pthread_cond_wait(cond, mutex);
        }

        gettimeofday(&tval, NULL);
        tspec.tv_sec = tval.tv_sec + (msec / 1000);
        tspec.tv_nsec = (tval.tv_usec + ((msec % 1000) * 1000)) * 1000;

        if (tspec.tv_nsec >= 1000000000) {
                tspec.tv_sec += 1;
                tspec.tv_nsec -= 1000000000;
        }

again:  ret = pthread_cond_timedwait(cond, mutex, &tspec);
        switch (ret) {
                case EINTR:
                        goto again;
                        break;
                case ETIMEDOUT:
                        ret = 1;
                        break;
                case 0:
                        break;
                default:
                        ret = -1;
                        break;
        }
        return ret;
}

int mbus_client_run (struct mbus_client *client)
{
	int rc;
	if (client == NULL) {
		mbus_errorf("client is null");
		return -1;
	}
	mbus_infof("running client: '%s'", client->name);
	rc = 0;
	while (1) {
		rc = mbus_client_run_timeout(client, -1);
		if (rc != 0) {
			break;
		}
	}
	mbus_infof("finished client: '%s'", client->name);
	return rc;
}

static int mbus_client_result (struct mbus_client *client, cJSON *payload)
{
	int rc;
	struct method *result;
	struct request *request;
	struct callback *callback;
	request = NULL;
	callback = NULL;
	if (client == NULL) {
		mbus_errorf("client is null");
		goto bail;
	}
	if (payload == NULL) {
		mbus_errorf("payload is null");
		goto bail;
	}
	pthread_mutex_lock(&client->mutex);
	request = request_create(MBUS_METHOD_TYPE_COMMAND, client->name, MBUS_SERVER_NAME, MBUS_SERVER_COMMAND_RESULT, client->sequence, payload);
	if (request == NULL) {
		mbus_errorf("can not create request");
		pthread_mutex_unlock(&client->mutex);
		goto bail;
	}
	client->sequence += 1;
	if (client->sequence >= MBUS_METHOD_SEQUENCE_END) {
		client->sequence = MBUS_METHOD_SEQUENCE_START;
	}
	TAILQ_INSERT_TAIL(&client->requests, request, requests);
	request->state = request_state_request;
	client->incommand = 1;
	pthread_cond_broadcast(&client->cond);
	while (request->result == NULL) {
		pthread_cond_wait(&client->cond, &client->mutex);
	}
	if (request->state == request_state_request) {
		TAILQ_REMOVE(&client->requests, request, requests);
		request->state = request_state_detached;
	}
	if (request->state == request_state_wait) {
		TAILQ_REMOVE(&client->waitings, request, requests);
		request->state = request_state_detached;
	}
	result = request_get_result(request);
	rc = method_get_result(result);
	client->incommand = 0;
	pthread_cond_broadcast(&client->cond);
	pthread_mutex_unlock(&client->mutex);
	request_destroy(request);
	return rc;
bail:	if (request != NULL) {
		request_destroy(request);
	}
	if (callback != NULL) {
		callback_destroy(callback);
	}
	return -1;
}

int mbus_client_run_timeout (struct mbus_client *client, int msec)
{
	int rc;
	struct method *method;
	struct command *command;
	struct callback *callback;
	if (client == NULL) {
		mbus_errorf("client is null");
		return -1;
	}
	pthread_mutex_lock(&client->mutex);
	if ((client->running == 1) &&
	    ((client->methods.count == 0) ||
	     (client->incommand == 1))) {
		__pthread_cond_timedwait(&client->cond, &client->mutex, msec);
	}
	if (client->running == 0) {
		pthread_mutex_unlock(&client->mutex);
		return 1;
	}
	if (client->methods.count == 0) {
		pthread_mutex_unlock(&client->mutex);
		return 0;
	}
	if (client->incommand != 0) {
		pthread_mutex_unlock(&client->mutex);
		return 0;
	}
	command = NULL;
	callback = NULL;
	method = client->methods.tqh_first;
	TAILQ_REMOVE(&client->methods, client->methods.tqh_first, methods);
	switch (method_get_type(method)) {
		case method_type_status:
			TAILQ_FOREACH(callback, &client->callbacks, callbacks) {
				if (strcmp(method_get_source(method), callback_get_source(callback)) != 0) {
					continue;
				}
				if (strcmp(callback_get_identifier(callback), MBUS_METHOD_STATUS_IDENTIFIER_ALL) != 0) {
					if (strcmp(method_get_identifier(method), callback_get_identifier(callback)) != 0) {
						continue;
					}
				}
				break;
			}
			break;
		case method_type_event:
			TAILQ_FOREACH(callback, &client->callbacks, callbacks) {
				if (strcmp(callback_get_source(callback), MBUS_METHOD_EVENT_SOURCE_ALL) != 0) {
					if (strcmp(method_get_source(method), callback_get_source(callback)) != 0) {
						continue;
					}
				}
				if (strcmp(callback_get_identifier(callback), MBUS_METHOD_EVENT_IDENTIFIER_ALL) != 0) {
					if (strcmp(method_get_identifier(method), callback_get_identifier(callback)) != 0) {
						continue;
					}
				}
				break;
			}
			break;
		case method_type_command:
			TAILQ_FOREACH(command, &client->commands, commands) {
				if (strcmp(method_get_identifier(method), command_get_identifier(command)) != 0) {
					continue;
				}
				break;
			}
			break;
		default:
			break;
	}
	pthread_mutex_unlock(&client->mutex);
	{
		char *string;
		string = cJSON_Print(method_get_payload(method));
		if (string == NULL) {
			mbus_errorf("can not allocate memory");
			method_destroy(method);
			return -1;
		}
		mbus_infof("%s.%s: %s", method_get_source(method), method_get_identifier(method), string);
		free(string);
	}
	if (callback != NULL) {
		if (method_get_type(method) == method_type_status) {
			callback->function(client, method_get_source(method), method_get_identifier(method), method_get_payload(method), callback->data);
		} else if (method_get_type(method) == method_type_event) {
			callback->function(client, method_get_source(method), method_get_identifier(method), method_get_payload(method), callback->data);
		}
	}
	if (command != NULL) {
		cJSON *payload;
		cJSON *result;
		payload = cJSON_CreateObject();
		if (payload == NULL) {
			mbus_errorf("can not create result object");
			method_destroy(method);
			return -1;
		}
		result = cJSON_CreateObject();
		if (result == NULL) {
			mbus_errorf("can not create result object");
			cJSON_Delete(payload);
			method_destroy(method);
			return -1;
		}
		rc = command->function(client, method_get_source(method), method_get_identifier(method), method_get_payload(method), result, command->data);
		cJSON_AddStringToObjectCS(payload, "destination", method_get_source(method));
		cJSON_AddStringToObjectCS(payload, "identifier", method_get_identifier(method));
		cJSON_AddNumberToObjectCS(payload, "sequence", method_get_sequence(method));
		cJSON_AddNumberToObjectCS(payload, "return", rc);
		cJSON_AddItemToObjectCS(payload, "result", result);
		rc = mbus_client_result(client, payload);
		if (rc != 0) {
			mbus_errorf("can not send result");
			cJSON_Delete(payload);
			method_destroy(method);
			return -1;
		}
		cJSON_Delete(payload);
	}
	method_destroy(method);
	return 0;
}

int mbus_client_subscribe (struct mbus_client *client, const char *source, const char *event, void (*function) (struct mbus_client *client, const char *source, const char *event, cJSON *payload, void *data), void *data)
{
	int rc;
	cJSON *payload;
	struct method *result;
	struct request *request;
	struct callback *callback;
	payload = NULL;
	request = NULL;
	callback = NULL;
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
	callback = callback_create(source, event, function, data);
	if (callback == NULL) {
		mbus_errorf("can not create callback");
		goto bail;
	}
	mbus_infof("subscribe client: '%s', source: '%s', event: '%s'", client->name, source, event);
	payload = cJSON_CreateObject();
	if (payload == NULL) {
		mbus_errorf("can not create command payload");
		goto bail;
	}
	cJSON_AddItemToObjectCS(payload, "source", cJSON_CreateString(source));
	cJSON_AddItemToObjectCS(payload, "event", cJSON_CreateString(event));
	pthread_mutex_lock(&client->mutex);
	if (client->error != 0) {
		mbus_errorf("client is in error state");
		pthread_mutex_unlock(&client->mutex);
		goto bail;
	}
	request = request_create(MBUS_METHOD_TYPE_COMMAND, client->name, MBUS_SERVER_NAME, MBUS_SERVER_COMMAND_SUBSCRIBE, client->sequence, payload);
	if (request == NULL) {
		mbus_errorf("can not create request");
		pthread_mutex_unlock(&client->mutex);
		goto bail;
	}
	client->sequence += 1;
	if (client->sequence >= MBUS_METHOD_SEQUENCE_END) {
		client->sequence = MBUS_METHOD_SEQUENCE_START;
	}
	TAILQ_INSERT_TAIL(&client->requests, request, requests);
	request->state = request_state_request;
	client->incommand = 1;
	pthread_cond_broadcast(&client->cond);
	while (client->running == 1 && request->result == NULL) {
		pthread_cond_wait(&client->cond, &client->mutex);
	}
	if (request->state == request_state_request) {
		TAILQ_REMOVE(&client->requests, request, requests);
		request->state = request_state_detached;
	}
	if (request->state == request_state_wait) {
		TAILQ_REMOVE(&client->waitings, request, requests);
		request->state = request_state_detached;
	}
	result = request_get_result(request);
	rc = method_get_result(result);
	if (rc == 0) {
		TAILQ_INSERT_TAIL(&client->callbacks, callback, callbacks);
	}
	client->incommand = 0;
	pthread_cond_broadcast(&client->cond);
	pthread_mutex_unlock(&client->mutex);
	cJSON_Delete(payload);
	request_destroy(request);
	return rc;
bail:	if (payload != NULL) {
		cJSON_Delete(payload);
	}
	if (request != NULL) {
		request_destroy(request);
	}
	if (callback != NULL) {
		callback_destroy(callback);
	}
	return -1;
}

int mbus_client_register (struct mbus_client *client, const char *command, int (*function) (struct mbus_client *client, const char *source, const char *command, cJSON *payload, cJSON *result, void *data), void *data)
{
	int rc;
	cJSON *payload;
	struct method *result;
	struct request *request;
	struct command *callback;
	payload = NULL;
	request = NULL;
	callback = NULL;
	if (client == NULL) {
		mbus_errorf("client is null");
		goto bail;
	}
	if (command == NULL) {
		mbus_errorf("event is null");
		goto bail;
	}
	callback = command_create(command, function, data);
	if (callback == NULL) {
		mbus_errorf("can not create callback");
		goto bail;
	}
	mbus_infof("register client: '%s', command: '%s'", client->name, command);
	payload = cJSON_CreateObject();
	if (payload == NULL) {
		mbus_errorf("can not create command payload");
		goto bail;
	}
	cJSON_AddItemToObjectCS(payload, "command", cJSON_CreateString(command));
	pthread_mutex_lock(&client->mutex);
	if (client->error != 0) {
		mbus_errorf("client is in error state");
		pthread_mutex_unlock(&client->mutex);
		goto bail;
	}
	request = request_create(MBUS_METHOD_TYPE_COMMAND, client->name, MBUS_SERVER_NAME, MBUS_SERVER_COMMAND_REGISTER, client->sequence, payload);
	if (request == NULL) {
		mbus_errorf("can not create request");
		pthread_mutex_unlock(&client->mutex);
		goto bail;
	}
	client->sequence += 1;
	if (client->sequence >= MBUS_METHOD_SEQUENCE_END) {
		client->sequence = MBUS_METHOD_SEQUENCE_START;
	}
	TAILQ_INSERT_TAIL(&client->requests, request, requests);
	request->state = request_state_request;
	client->incommand = 1;
	pthread_cond_broadcast(&client->cond);
	while (request->result == NULL) {
		pthread_cond_wait(&client->cond, &client->mutex);
	}
	if (request->state == request_state_request) {
		TAILQ_REMOVE(&client->requests, request, requests);
		request->state = request_state_detached;
	}
	if (request->state == request_state_wait) {
		TAILQ_REMOVE(&client->waitings, request, requests);
		request->state = request_state_detached;
	}
	result = request_get_result(request);
	rc = method_get_result(result);
	if (rc == 0) {
		TAILQ_INSERT_TAIL(&client->commands, callback, commands);
	}
	client->incommand = 0;
	pthread_cond_broadcast(&client->cond);
	pthread_mutex_unlock(&client->mutex);
	cJSON_Delete(payload);
	request_destroy(request);
	return rc;
bail:	if (payload != NULL) {
		cJSON_Delete(payload);
	}
	if (request != NULL) {
		request_destroy(request);
	}
	if (callback != NULL) {
		command_destroy(callback);
	}
	return -1;
}

int mbus_client_event_to (struct mbus_client *client, const char *to, const char *identifier, cJSON *event)
{
	int rc;
	cJSON *data;
	cJSON *payload;
	struct method *result;
	struct request *request;
	data = NULL;
	payload = NULL;
	request = NULL;
	if (client == NULL) {
		mbus_errorf("client is null");
		goto bail;
	}
	if (identifier == NULL) {
		mbus_errorf("identifier is null");
		goto bail;
	}
	if (event == NULL) {
		data = cJSON_CreateObject();
	} else {
		data = cJSON_Duplicate(event, 1);
	}
	if (data == NULL) {
		mbus_errorf("can not create data");
		goto bail;
	}
	pthread_mutex_lock(&client->mutex);
	if (client->error != 0) {
		mbus_errorf("client is in error state");
		pthread_mutex_unlock(&client->mutex);
		goto bail;
	}
	payload = cJSON_CreateObject();
	if (payload == NULL) {
		mbus_errorf("can not create command payload");
		goto bail;
	}
	cJSON_AddStringToObjectCS(payload, "destination", to);
	cJSON_AddStringToObjectCS(payload, "identifier", identifier);
	cJSON_AddItemToObjectCS(payload, "event", data);
	data = NULL;
	request = request_create(MBUS_METHOD_TYPE_COMMAND, client->name, MBUS_SERVER_NAME, MBUS_SERVER_COMMAND_EVENT, client->sequence, payload);
	if (request == NULL) {
		mbus_errorf("can not create request");
		pthread_mutex_unlock(&client->mutex);
		goto bail;
	}
	client->sequence += 1;
	if (client->sequence >= MBUS_METHOD_SEQUENCE_END) {
		client->sequence = MBUS_METHOD_SEQUENCE_START;
	}
	TAILQ_INSERT_TAIL(&client->requests, request, requests);
	request->state = request_state_request;
	client->incommand = 1;
	pthread_cond_broadcast(&client->cond);
	while (client->running == 1 && request->result == NULL) {
		pthread_cond_wait(&client->cond, &client->mutex);
	}
	if (request->state == request_state_request) {
		TAILQ_REMOVE(&client->requests, request, requests);
		request->state = request_state_detached;
	}
	if (request->state == request_state_wait) {
		TAILQ_REMOVE(&client->waitings, request, requests);
		request->state = request_state_detached;
	}
	result = request_get_result(request);
	rc = method_get_result(result);
	if (rc != 0) {
		mbus_errorf("could not send event: %d, %d, %p", client->running, client->error, request->result);
	}
	client->incommand = 0;
	pthread_cond_broadcast(&client->cond);
	pthread_mutex_unlock(&client->mutex);
	cJSON_Delete(payload);
	request_destroy(request);
	return rc;
bail:	if (payload != NULL) {
		cJSON_Delete(payload);
	}
	if (data != NULL) {
		cJSON_Delete(data);
	}
	if (request != NULL) {
		request_destroy(request);
	}
	return -1;
}

int mbus_client_event (struct mbus_client *client, const char *identifier, cJSON *event)
{
	return mbus_client_event_to(client, MBUS_METHOD_EVENT_DESTINATION_SUBSCRIBERS, identifier, event);
}

int mbus_client_command (struct mbus_client *client, const char *destination, const char *command, cJSON *call, cJSON **rslt)
{
	int rc;
	cJSON *data;
	cJSON *answer;
	cJSON *payload;
	struct method *result;
	struct request *request;
	data = NULL;
	payload = NULL;
	request = NULL;
	if (rslt != NULL) {
		*rslt = NULL;
	}
	if (client == NULL) {
		mbus_errorf("client is null");
		goto bail;
	}
	if (destination == NULL) {
		mbus_errorf("destination is null");
		goto bail;
	}
	if (command == NULL) {
		mbus_errorf("command is null");
		goto bail;
	}
	if (call == NULL) {
		data = cJSON_CreateObject();
	} else {
		data = cJSON_Duplicate(call, 1);
	}
	if (data == NULL) {
		mbus_errorf("can not create data");
		goto bail;
	}
	pthread_mutex_lock(&client->mutex);
	if (client->error != 0) {
		mbus_errorf("client is in error state");
		pthread_mutex_unlock(&client->mutex);
		goto bail;
	}
	payload = cJSON_CreateObject();
	if (payload == NULL) {
		mbus_errorf("can not create command payload");
		goto bail;
	}
	cJSON_AddStringToObjectCS(payload, "destination", destination);
	cJSON_AddStringToObjectCS(payload, "identifier", command);
	cJSON_AddItemToObjectCS(payload, "call", data);
	request = request_create(MBUS_METHOD_TYPE_COMMAND, client->name, MBUS_SERVER_NAME, MBUS_SERVER_COMMAND_CALL, client->sequence, payload);
	if (request == NULL) {
		mbus_errorf("can not create request");
		pthread_mutex_unlock(&client->mutex);
		goto bail;
	}
	client->sequence += 1;
	if (client->sequence >= MBUS_METHOD_SEQUENCE_END) {
		client->sequence = MBUS_METHOD_SEQUENCE_START;
	}
	TAILQ_INSERT_TAIL(&client->requests, request, requests);
	request->state = request_state_request;
	client->incommand = 1;
	pthread_cond_broadcast(&client->cond);
	while (client->running == 1 && request->result == NULL) {
		pthread_cond_wait(&client->cond, &client->mutex);
	}
	if (request->state == request_state_request) {
		TAILQ_REMOVE(&client->requests, request, requests);
		request->state = request_state_detached;
	}
	if (request->state == request_state_wait) {
		TAILQ_REMOVE(&client->waitings, request, requests);
		request->state = request_state_detached;
	}
	result = request_get_result(request);
	rc = method_get_result(result);
	if (rc != 0) {
		mbus_errorf("could not call command");
	} else {
		if (rslt != NULL) {
			answer = method_get_payload(result);
			if (answer != NULL) {
				*rslt = cJSON_Duplicate(answer, 1);
			}
		}
	}
	client->incommand = 0;
	pthread_cond_broadcast(&client->cond);
	pthread_mutex_unlock(&client->mutex);
	cJSON_Delete(payload);
	request_destroy(request);
	return rc;
bail:	if (payload != NULL) {
		cJSON_Delete(payload);
	}
	if (request != NULL) {
		request_destroy(request);
	}
	return -1;
}
