
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

#if !defined(MIN)
#define MIN(a, b)	(((a) < (b)) ? (a) : (b))
#endif
#if !defined(MAX)
#define MAX(a, b)	(((a) > (b)) ? (a) : (b))
#endif

#define OPTION_HELP			0x100
#define OPTION_DEBUG_LEVEL		0x101
#define OPTION_IDENTIFIER		0x201
#define OPTION_SERVER_PROTOCOL		0x202
#define OPTION_SERVER_ADDRESS		0x203
#define OPTION_SERVER_PORT		0x204
#define OPTION_CONNECT_TIMEOUT		0x205
#define OPTION_CONNECT_INTERVAL		0x206
#define OPTION_SUBSCRIBE_TIMEOUT	0x207
#define OPTION_REGISTER_TIMEOUT		0x208
#define OPTION_COMMAND_TIMEOUT		0x210
#define OPTION_PUBLISH_TIMEOUT		0x211
#define OPTION_PING_INTERVAL		0x212
#define OPTION_PING_TIMEOUT		0x213
#define OPTION_PING_THRESHOLD		0x214
static struct option longopts[] = {
	{ "mbus-help",				no_argument,		NULL,	OPTION_HELP },
	{ "mbus-debug-level",			required_argument,	NULL,	OPTION_DEBUG_LEVEL },
	{ "mbus-client-identifier",		required_argument,	NULL,	OPTION_IDENTIFIER },
	{ "mbus-client-server-protocol",	required_argument,	NULL,	OPTION_SERVER_PROTOCOL },
	{ "mbus-client-server-address"	,	required_argument,	NULL,	OPTION_SERVER_ADDRESS },
	{ "mbus-client-server-port",		required_argument,	NULL,	OPTION_SERVER_PORT },
	{ "mbus-client-connect-timeout",	required_argument,	NULL,	OPTION_CONNECT_TIMEOUT },
	{ "mbus-client-connect-interval",	required_argument,	NULL,	OPTION_CONNECT_INTERVAL },
	{ "mbus-client-subscribe-timeout",	required_argument,	NULL,	OPTION_SUBSCRIBE_TIMEOUT },
	{ "mbus-client-register-timeout",	required_argument,	NULL,	OPTION_REGISTER_TIMEOUT },
	{ "mbus-client-command-timeout",	required_argument,	NULL,	OPTION_COMMAND_TIMEOUT },
	{ "mbus-client-publish-timeout",	required_argument,	NULL,	OPTION_PUBLISH_TIMEOUT },
	{ "mbus-client-ping-interval",		required_argument,	NULL,	OPTION_PING_INTERVAL },
	{ "mbus-client-ping-timeout",		required_argument,	NULL,	OPTION_PING_TIMEOUT },
	{ "mbus-client-ping-threshold",		required_argument,	NULL,	OPTION_PING_THRESHOLD },
	{ NULL,					0,			NULL,	0 },
};

enum wakeup_reason {
	wakeup_reason_break,
	wakeup_reason_connect,
	wakeup_reason_disconnect
};

TAILQ_HEAD(requests, request);
struct request {
	TAILQ_ENTRY(request) requests;
	char *string;
	struct mbus_json *json;
	void (*callback) (struct mbus_client *client, void *context, struct mbus_client_message *message, enum mbus_client_command_status status);
	void *context;
	unsigned long created_at;
	int timeout;
};

TAILQ_HEAD(routines, routine);
struct routine {
	TAILQ_ENTRY(routine) routines;
	char *identifier;
	int (*callback) (struct mbus_client *client, void *context, struct mbus_client_message *message);
	void *context;
};

TAILQ_HEAD(subscriptions, subscription);
struct subscription {
	TAILQ_ENTRY(subscription) subscriptions;
	char *source;
	char *identifier;
	void (*callback) (struct mbus_client *client, void *context, struct mbus_client_message *message);
	void *context;
};

struct mbus_client {
	struct mbus_client_options *options;
	enum mbus_client_state state;
	struct mbus_socket *socket;
	struct requests requests;
	struct requests pendings;
	struct routines routines;
	struct subscriptions subscriptions;
	struct mbus_buffer *incoming;
	struct mbus_buffer *outgoing;
	char *identifier;
	int connect_tsms;
	int ping_interval;
	int ping_timeout;
	int ping_threshold;
	unsigned long ping_send_tsms;
	unsigned long pong_recv_tsms;
	int ping_wait_pong;
	int pong_missed_count;
	enum mbus_compress_method compression;
	int socket_connected;
	int sequence;
	int wakeup[2];
	pthread_mutex_t mutex;
#if defined(SSL_ENABLE) && (SSL_ENABLE == 1)
	struct {
		const SSL_METHOD *method;
		SSL_CTX *context;
		SSL *ssl;
		int want_read;
		int want_write;
	} ssl;
#endif
};

enum mbus_client_message_type {
	mbus_client_message_type_event,
	mbus_client_message_type_command,
	mbus_client_message_type_routine
};

struct mbus_client_message {
	enum mbus_client_message_type type;
	union  {
		struct {
			const struct mbus_json *payload;
		} event;
		struct {
			const struct mbus_json *request;
			const struct mbus_json *response;
		} command;
		struct {
			const struct mbus_json *request;
			struct mbus_json *response;
		} routine;
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

static void * routine_get_context (const struct routine *routine)
{
	if (routine == NULL) {
		return NULL;
	}
	return routine->context;
}

static int (* routine_get_callback (const struct routine *routine)) (struct mbus_client *client, void *context, struct mbus_client_message *message)
{
	if (routine == NULL) {
		return NULL;
	}
	return routine->callback;
}

static const char * routine_get_identifier (const struct routine *routine)
{
	if (routine == NULL) {
		return NULL;
	}
	return routine->identifier;
}

static void routine_destroy (struct routine *routine)
{
	if (routine == NULL) {
		return;
	}
	if (routine->identifier != NULL) {
		free(routine->identifier);
	}
	free(routine);
}

static struct routine * routine_create (const char *identifier, int (*callback) (struct mbus_client *client, void *context, struct mbus_client_message *message), void *context)
{
	struct routine *routine;
	routine = malloc(sizeof(struct routine));
	if (routine == NULL) {
		mbus_errorf("can not allocate memory");
		goto bail;
	}
	memset(routine, 0, sizeof(struct routine));
	if (identifier == NULL) {
		mbus_errorf("identifier is invalid");
		goto bail;
	}
	if (callback == NULL) {
		mbus_errorf("callback is invalid");
		goto bail;
	}
	routine->identifier = strdup(identifier);
	if (routine->identifier == NULL) {
		mbus_errorf("can not allocate memory");
		goto bail;
	}
	routine->callback = callback;
	routine->context = context;
	return routine;
bail:	if (routine != NULL) {
		routine_destroy(routine);
	}
	return NULL;
}

static void * subscription_get_context (const struct subscription *subscription)
{
	if (subscription == NULL) {
		return NULL;
	}
	return subscription->context;
}

static void (* subscription_get_callback (const struct subscription *subscription)) (struct mbus_client *client, void *context, struct mbus_client_message *message)
{
	if (subscription == NULL) {
		return NULL;
	}
	return subscription->callback;
}

static const char * subscription_get_source (const struct subscription *subscription)
{
	if (subscription == NULL) {
		return NULL;
	}
	return subscription->source;
}

static const char * subscription_get_identifier (const struct subscription *subscription)
{
	if (subscription == NULL) {
		return NULL;
	}
	return subscription->identifier;
}

static void subscription_destroy (struct subscription *subscription)
{
	if (subscription == NULL) {
		return;
	}
	if (subscription->source != NULL) {
		free(subscription->source);
	}
	if (subscription->identifier != NULL) {
		free(subscription->identifier);
	}
	free(subscription);
}

static struct subscription * subscription_create (const char *source, const char *identifier, void (*callback) (struct mbus_client *client, void *context, struct mbus_client_message *message), void *context)
{
	struct subscription *subscription;
	subscription = malloc(sizeof(struct subscription));
	if (subscription == NULL) {
		mbus_errorf("can not allocate memory");
		goto bail;
	}
	memset(subscription, 0, sizeof(struct subscription));
	if (source == NULL) {
		mbus_errorf("source is invalid");
		goto bail;
	}
	if (identifier == NULL) {
		mbus_errorf("identifier is invalid");
		goto bail;
	}
	if (callback == NULL) {
		mbus_errorf("callback is invalid");
		goto bail;
	}
	subscription->source = strdup(source);
	if (subscription->source == NULL) {
		mbus_errorf("can not allocate memory");
		goto bail;
	}
	subscription->identifier = strdup(identifier);
	if (subscription->identifier == NULL) {
		mbus_errorf("can not allocate memory");
		goto bail;
	}
	subscription->callback = callback;
	subscription->context = context;
	return subscription;
bail:	if (subscription != NULL) {
		subscription_destroy(subscription);
	}
	return NULL;
}

static int request_get_sequence (const struct request *request)
{
	if (request == NULL) {
		return -1;
	}
	return mbus_json_get_int_value(request->json, "sequence", -1);
}

static const char * request_get_type (const struct request *request)
{
	if (request == NULL) {
		return NULL;
	}
	return mbus_json_get_string_value(request->json, "type", NULL);
}

static const char * request_get_destination (const struct request *request)
{
	if (request == NULL) {
		return NULL;
	}
	return mbus_json_get_string_value(request->json, "destination", NULL);
}

static const char * request_get_identifier (const struct request *request)
{
	if (request == NULL) {
		return NULL;
	}
	return mbus_json_get_string_value(request->json, "identifier", NULL);
}

static const struct mbus_json * request_get_payload (const struct request *request)
{
	if (request == NULL) {
		return NULL;
	}
	return mbus_json_get_object(request->json, "payload");
}

static const struct mbus_json * request_get_json (const struct request *request)
{
	if (request == NULL) {
		return NULL;
	}
	return request->json;
}

static unsigned long request_get_created_at (const struct request *request)
{
	if (request == NULL) {
		return 0;
	}
	return request->created_at;
}

static int request_get_timeout (const struct request *request)
{
	if (request == NULL) {
		return -1;
	}
	return request->timeout;
}

static const char * request_get_string (const struct request *request)
{
	if (request == NULL) {
		return NULL;
	}
	return request->string;
}

static void * request_get_context (const struct request *request)
{
	if (request == NULL) {
		return NULL;
	}
	return request->context;
}

static void (* request_get_callback (const struct request *request)) (struct mbus_client *client, void *context, struct mbus_client_message *message, enum mbus_client_command_status status)
{
	if (request == NULL) {
		return NULL;
	}
	return request->callback;
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

static struct request * request_create (const char *type, const char *destination, const char *identifier, int sequence, const struct mbus_json *payload, void (*callback) (struct mbus_client *client, void *context, struct mbus_client_message *message, enum mbus_client_command_status status), void *context, int timeout)
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
	    sequence >= MBUS_METHOD_SEQUENCE_END) {
		mbus_errorf("sequence is invalid");
		goto bail;
	}
	if (timeout < 0) {
		mbus_errorf("timeout is invalid");
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
	rc = mbus_json_add_number_to_object_cs(request->json, "timeout", timeout);
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
	request->created_at = mbus_clock_get();
	request->timeout = timeout;
	return request;
bail:	if (request != NULL) {
		request_destroy(request);
	}
	return NULL;
}

static void mbus_client_reset (struct mbus_client *client)
{
	int i;
	struct request *request;
	struct request *nrequest;
	struct requests *requests[2];
	struct routine *routine;
	struct routine *nroutine;
	struct subscription *subscription;
	struct subscription *nsubscription;
	if (client->socket != NULL) {
		mbus_socket_shutdown(client->socket, mbus_socket_shutdown_rdwr);
		mbus_socket_destroy(client->socket);
		client->socket = NULL;
	}
	if (client->incoming != NULL) {
		mbus_buffer_reset(client->incoming);
	}
	if (client->outgoing != NULL) {
		mbus_buffer_reset(client->outgoing);
	}
	requests[0] = &client->requests;
	requests[1] = &client->pendings;
	for (i = 0; i < (int) (sizeof(requests) / sizeof(requests[0])); i++) {
		TAILQ_FOREACH_SAFE(request, requests[i], requests, nrequest) {
			TAILQ_REMOVE(requests[i], request, requests);
			if (strcasecmp(request_get_type(request), MBUS_METHOD_TYPE_EVENT) == 0) {
				if (strcasecmp(MBUS_SERVER_IDENTIFIER, request_get_destination(request)) != 0 &&
				    strcasecmp(MBUS_SERVER_EVENT_PING, request_get_identifier(request)) != 0) {
					if (client->options->callbacks.publish != NULL) {
						struct mbus_client_message msg;
						msg.type = mbus_client_message_type_event;
						msg.u.event.payload = request_get_json(request);
						mbus_client_unlock(client);
						client->options->callbacks.publish(client, client->options->callbacks.context, &msg, mbus_client_publish_status_canceled);
						mbus_client_lock(client);
					}
				}
			} else if (strcasecmp(request_get_type(request), MBUS_METHOD_TYPE_COMMAND) == 0) {
				if (strcasecmp(request_get_identifier(request), MBUS_SERVER_COMMAND_EVENT) == 0) {
					if (client->options->callbacks.publish != NULL) {
						struct mbus_client_message msg;
						msg.type = mbus_client_message_type_event;
						msg.u.event.payload = request_get_payload(request);
						mbus_client_unlock(client);
						client->options->callbacks.publish(client, client->options->callbacks.context, &msg, mbus_client_publish_status_canceled);
						mbus_client_lock(client);
					}
				} else if (strcasecmp(request_get_identifier(request), MBUS_SERVER_COMMAND_SUBSCRIBE) == 0) {
					if (client->options->callbacks.subscribe != NULL) {
						mbus_client_unlock(client);
						client->options->callbacks.subscribe(client, client->options->callbacks.context,
								mbus_json_get_string_value(request_get_payload(request), "source", NULL),
								mbus_json_get_string_value(request_get_payload(request), "event", NULL),
								mbus_client_subscribe_status_canceled);
						mbus_client_lock(client);
					}
				} else if (strcasecmp(request_get_identifier(request), MBUS_SERVER_COMMAND_UNSUBSCRIBE) == 0) {
					if (client->options->callbacks.unsubscribe != NULL) {
						mbus_client_unlock(client);
						client->options->callbacks.unsubscribe(client, client->options->callbacks.context,
								mbus_json_get_string_value(request_get_payload(request), "source", NULL),
								mbus_json_get_string_value(request_get_payload(request), "event", NULL),
								mbus_client_unsubscribe_status_canceled);
						mbus_client_lock(client);
					}
				} else if (strcasecmp(request_get_identifier(request), MBUS_SERVER_COMMAND_REGISTER) == 0) {
					if (client->options->callbacks.registered != NULL) {
						mbus_client_unlock(client);
						client->options->callbacks.registered(client, client->options->callbacks.context,
								mbus_json_get_string_value(request_get_payload(request), "command", NULL),
								mbus_client_register_status_canceled);
						mbus_client_lock(client);
					}
				} else if (strcasecmp(request_get_identifier(request), MBUS_SERVER_COMMAND_UNREGISTER) == 0) {
					if (client->options->callbacks.unregistered != NULL) {
						mbus_client_unlock(client);
						client->options->callbacks.unregistered(client, client->options->callbacks.context,
								mbus_json_get_string_value(request_get_payload(request), "command", NULL),
								mbus_client_unregister_status_canceled);
						mbus_client_lock(client);
					}
				} else {
					if (request_get_callback(request) != NULL) {
						struct mbus_client_message message;
						message.type = mbus_client_message_type_command;
						message.u.command.request = request_get_json(request);
						message.u.command.response = NULL;
						mbus_client_unlock(client);
						request_get_callback(request)(client, request_get_context(request), &message, mbus_client_command_status_canceled);
						mbus_client_lock(client);
					}
				}
			}
			request_destroy(request);
		}
	}
	TAILQ_FOREACH_SAFE(routine, &client->routines, routines, nroutine) {
		TAILQ_REMOVE(&client->routines, routine, routines);
		if (client->options->callbacks.unregistered != NULL) {
			mbus_client_unlock(client);
			client->options->callbacks.unregistered(client, client->options->callbacks.context,
					routine_get_identifier(routine),
					mbus_client_unregister_status_canceled);
			mbus_client_lock(client);
		}
		routine_destroy(routine);
	}
	TAILQ_FOREACH_SAFE(subscription, &client->subscriptions, subscriptions, nsubscription) {
		TAILQ_REMOVE(&client->subscriptions, subscription, subscriptions);
		if (client->options->callbacks.unsubscribe != NULL) {
			mbus_client_unlock(client);
			client->options->callbacks.unsubscribe(client, client->options->callbacks.context,
					subscription_get_source(subscription),
					subscription_get_identifier(subscription),
					mbus_client_unsubscribe_status_canceled);
			mbus_client_lock(client);
		}
		subscription_destroy(subscription);
	}
	if (client->identifier != NULL) {
		free(client->identifier);
		client->identifier = NULL;
	}
	client->connect_tsms = 0;
	client->ping_interval = 0;
	client->ping_timeout = 0;
	client->ping_threshold = 0;
	client->ping_send_tsms = 0;
	client->pong_recv_tsms = 0;
	client->ping_wait_pong = 0;
	client->pong_missed_count = 0;
	client->sequence = MBUS_METHOD_SEQUENCE_START;
	client->compression = mbus_compress_method_none;
	client->socket_connected = 0;
}

static void mbus_client_notify_connect (struct mbus_client *client, enum mbus_client_connect_status status)
{
	if (client->options->callbacks.connect != NULL) {
		mbus_client_unlock(client);
		client->options->callbacks.connect(client, client->options->callbacks.context, status);
		mbus_client_lock(client);
	}
}

static void mbus_client_notify_disconnect (struct mbus_client *client, enum mbus_client_disconnect_status status)
{
	if (client->options->callbacks.disconnect != NULL) {
		mbus_client_unlock(client);
		client->options->callbacks.disconnect(client, client->options->callbacks.context, status);
		mbus_client_lock(client);
	}
}

static void mbus_client_command_register_response (struct mbus_client *client, void *context, struct mbus_client_message *message, enum mbus_client_command_status status)
{
	enum mbus_client_register_status cstatus;
	struct routine *routine = context;
	mbus_client_lock(client);
	if (status != mbus_client_command_status_success) {
		if (status == mbus_client_command_status_internal_error) {
			cstatus = mbus_client_register_status_internal_error;
		} else if (status == mbus_client_command_status_timeout) {
			cstatus = mbus_client_register_status_timeout;
		} else {
			cstatus = mbus_client_register_status_internal_error;
		}
		if (routine != NULL) {
			routine_destroy(routine);
		}
	} else if (mbus_client_message_command_response_result(message) == 0) {
		cstatus = mbus_client_register_status_success;
		if (routine != NULL) {
			TAILQ_INSERT_TAIL(&client->routines, routine, routines);
		}
	} else {
		cstatus = mbus_client_register_status_internal_error;
		if (routine != NULL) {
			routine_destroy(routine);
		}
	}
	if (client->options->callbacks.registered != NULL) {
		mbus_client_unlock(client);
		client->options->callbacks.registered(client, client->options->callbacks.context,
				mbus_json_get_string_value(mbus_client_message_command_request_payload(message), "command", NULL),
				cstatus);
		mbus_client_lock(client);
	}
	mbus_client_unlock(client);
}

static void mbus_client_command_unregister_response (struct mbus_client *client, void *context, struct mbus_client_message *message, enum mbus_client_command_status status)
{
	enum mbus_client_unregister_status cstatus;
	(void) context;
	mbus_client_lock(client);
	if (status != mbus_client_command_status_success) {
		if (status == mbus_client_command_status_internal_error) {
			cstatus = mbus_client_unregister_status_internal_error;
		} else if (status == mbus_client_command_status_timeout) {
			cstatus = mbus_client_unregister_status_timeout;
		} else {
			cstatus = mbus_client_unregister_status_internal_error;
		}
	} else if (mbus_client_message_command_response_result(message) == 0) {
		cstatus = mbus_client_unregister_status_success;
	} else {
		cstatus = mbus_client_unregister_status_internal_error;
	}
	if (client->options->callbacks.unregistered != NULL) {
		mbus_client_unlock(client);
		client->options->callbacks.unregistered(client, client->options->callbacks.context,
				mbus_json_get_string_value(mbus_client_message_command_request_payload(message), "identifier", NULL),
				cstatus);
		mbus_client_lock(client);
	}
	mbus_client_unlock(client);
}

static void mbus_client_command_subscribe_response (struct mbus_client *client, void *context, struct mbus_client_message *message, enum mbus_client_command_status status)
{
	enum mbus_client_subscribe_status cstatus;
	struct subscription *subscription = context;
	mbus_client_lock(client);
	if (status != mbus_client_command_status_success) {
		if (status == mbus_client_command_status_internal_error) {
			cstatus = mbus_client_subscribe_status_internal_error;
		} else if (status == mbus_client_command_status_timeout) {
			cstatus = mbus_client_subscribe_status_timeout;
		} else {
			cstatus = mbus_client_subscribe_status_internal_error;
		}
		if (subscription != NULL) {
			subscription_destroy(subscription);
		}
	} else if (mbus_client_message_command_response_result(message) == 0) {
		cstatus = mbus_client_subscribe_status_success;
		if (subscription != NULL) {
			TAILQ_INSERT_TAIL(&client->subscriptions, subscription, subscriptions);
		}
	} else {
		cstatus = mbus_client_subscribe_status_internal_error;
		if (subscription != NULL) {
			subscription_destroy(subscription);
		}
	}
	if (client->options->callbacks.subscribe != NULL) {
		mbus_client_unlock(client);
		client->options->callbacks.subscribe(client, client->options->callbacks.context,
				mbus_json_get_string_value(mbus_client_message_command_request_payload(message), "source", NULL),
				mbus_json_get_string_value(mbus_client_message_command_request_payload(message), "event", NULL),
				cstatus);
		mbus_client_lock(client);
	}
	mbus_client_unlock(client);
}

static void mbus_client_command_unsubscribe_response (struct mbus_client *client, void *context, struct mbus_client_message *message, enum mbus_client_command_status status)
{
	enum mbus_client_unsubscribe_status cstatus;
	(void) context;
	mbus_client_lock(client);
	if (status != mbus_client_command_status_success) {
		if (status == mbus_client_command_status_internal_error) {
			cstatus = mbus_client_unsubscribe_status_internal_error;
		} else if (status == mbus_client_command_status_timeout) {
			cstatus = mbus_client_unsubscribe_status_timeout;
		} else {
			cstatus = mbus_client_unsubscribe_status_internal_error;
		}
	} else if (mbus_client_message_command_response_result(message) == 0) {
		cstatus = mbus_client_unsubscribe_status_success;
	} else {
		cstatus = mbus_client_unsubscribe_status_internal_error;
	}
	if (client->options->callbacks.unsubscribe != NULL) {
		mbus_client_unlock(client);
		client->options->callbacks.unsubscribe(client, client->options->callbacks.context,
				mbus_json_get_string_value(mbus_client_message_command_request_payload(message), "source", NULL),
				mbus_json_get_string_value(mbus_client_message_command_request_payload(message), "event", NULL),
				cstatus);
		mbus_client_lock(client);
	}
	mbus_client_unlock(client);
}

static void mbus_client_command_event_response (struct mbus_client *client, void *context, struct mbus_client_message *message, enum mbus_client_command_status status)
{
	struct mbus_client_message msg;
	enum mbus_client_publish_status cstatus;
	(void) context;
	mbus_client_lock(client);
	if (status != mbus_client_command_status_success) {
		if (status == mbus_client_command_status_internal_error) {
			cstatus = mbus_client_publish_status_internal_error;
		} else if (status == mbus_client_command_status_timeout) {
			cstatus = mbus_client_publish_status_timeout;
		} else {
			cstatus = mbus_client_publish_status_internal_error;
		}
	} else if (mbus_client_message_command_response_result(message) == 0) {
		cstatus = mbus_client_publish_status_success;
	} else {
		cstatus = mbus_client_publish_status_internal_error;
	}
	if (client->options->callbacks.publish != NULL) {
		mbus_client_unlock(client);
		msg.type = mbus_client_message_type_event;
		msg.u.event.payload = mbus_client_message_command_request_payload(message);
		client->options->callbacks.publish(client, client->options->callbacks.context, &msg, cstatus);
		mbus_client_lock(client);
	}
	mbus_client_unlock(client);
}

static void mbus_client_command_create_response (struct mbus_client *client, void *context, struct mbus_client_message *message, enum mbus_client_command_status status)
{
	const struct mbus_json *response;
	(void) context;
	mbus_client_lock(client);
	if (status != mbus_client_command_status_success) {
		mbus_errorf("client command create failed");
		if (status == mbus_client_command_status_internal_error) {
			mbus_client_notify_connect(client, mbus_client_connect_status_server_error);
		} else if (status == mbus_client_command_status_timeout) {
			mbus_client_notify_connect(client, mbus_client_connect_status_timeout);
		} else {
			mbus_client_notify_connect(client, mbus_client_connect_status_server_error);
		}
		mbus_client_reset(client);
		client->state = mbus_client_state_disconnected;
		goto bail;
	}
	if (mbus_client_message_command_response_result(message) != 0) {
		mbus_errorf("client command create failed");
		mbus_client_notify_connect(client, mbus_client_connect_status_server_error);
		mbus_client_reset(client);
		client->state = mbus_client_state_disconnected;
		goto bail;
	}
	response = mbus_client_message_command_response_payload(message);
	if (response == NULL) {
		mbus_errorf("message response is invalid");
		goto bail;
	}
	{
		const char *identifier;
		identifier = mbus_json_get_string_value(response, "identifier", NULL);
		if (identifier != NULL) {
			free(client->identifier);
			client->identifier = strdup(identifier);
			if (client->identifier == NULL) {
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
	mbus_infof("  identifier : %s", client->identifier);
	mbus_infof("  compression: %s", mbus_compress_method_string(client->compression));
	mbus_infof("  ping");
	mbus_infof("    interval : %d", client->ping_interval);
	mbus_infof("    timeout  : %d", client->ping_timeout);
	mbus_infof("    threshold: %d", client->ping_threshold);
	client->state = mbus_client_state_connected;
	mbus_client_notify_connect(client, mbus_client_connect_status_success);
	mbus_client_unlock(client);
	return;
bail:	mbus_client_unlock(client);
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

	if (client->options->identifier != NULL) {
		rc = mbus_json_add_string_to_object_cs(payload, "identifier", client->options->identifier);
		if (rc != 0) {
			mbus_errorf("can not add string to json object");
			goto bail;
		}
	}

	payload_ping = mbus_json_create_object();
	if (payload_ping == NULL) {
		mbus_errorf("can not create json object");
		goto bail;
	}
	rc = mbus_json_add_number_to_object_cs(payload_ping, "interval", client->options->ping_interval);
	if (rc != 0) {
		mbus_errorf("can not add number to json object");
		goto bail;
	}
	rc = mbus_json_add_number_to_object_cs(payload_ping, "timeout", client->options->ping_timeout);
	if (rc != 0) {
		mbus_errorf("can not add item to json object");
		goto bail;
	}
	rc = mbus_json_add_number_to_object_cs(payload_ping, "threshold", client->options->ping_threshold);
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

	rc = mbus_client_command_unlocked(client, MBUS_SERVER_IDENTIFIER, MBUS_SERVER_COMMAND_CREATE, payload, mbus_client_command_create_response, client);
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
	enum mbus_socket_type socket_type;
	enum mbus_socket_domain socket_domain;
	enum mbus_client_connect_status status;

	mbus_client_reset(client);
	status = mbus_client_connect_status_success;

	if (strcmp(client->options->server_protocol, MBUS_SERVER_TCP_PROTOCOL) == 0) {
		if (client->options->server_port <= 0) {
			client->options->server_port = MBUS_SERVER_TCP_PORT;
		}
		if (client->options->server_address == NULL) {
			client->options->server_address = MBUS_SERVER_TCP_ADDRESS;
		}
		socket_domain = mbus_socket_domain_af_inet;
		socket_type = mbus_socket_type_sock_stream;
	} else if (strcmp(client->options->server_protocol, MBUS_SERVER_UDS_PROTOCOL) == 0) {
		if (client->options->server_port <= 0) {
			client->options->server_port = MBUS_SERVER_UDS_PORT;
		}
		if (client->options->server_address == NULL) {
			client->options->server_address = MBUS_SERVER_UDS_ADDRESS;
		}
		socket_domain = mbus_socket_domain_af_unix;
		socket_type = mbus_socket_type_sock_stream;
	} else if (strcmp(client->options->server_protocol, MBUS_SERVER_TCPS_PROTOCOL) == 0) {
		if (client->options->server_port <= 0) {
			client->options->server_port = MBUS_SERVER_TCPS_PORT;
		}
		if (client->options->server_address == NULL) {
			client->options->server_address = MBUS_SERVER_TCPS_ADDRESS;
		}
		socket_domain = mbus_socket_domain_af_inet;
		socket_type = mbus_socket_type_sock_stream;
	} else if (strcmp(client->options->server_protocol, MBUS_SERVER_UDSS_PROTOCOL) == 0) {
		if (client->options->server_port <= 0) {
			client->options->server_port = MBUS_SERVER_UDSS_PORT;
		}
		if (client->options->server_address == NULL) {
			client->options->server_address = MBUS_SERVER_UDSS_ADDRESS;
		}
		socket_domain = mbus_socket_domain_af_unix;
		socket_type = mbus_socket_type_sock_stream;
	} else {
		mbus_errorf("invalid server protocol: %s", client->options->server_protocol);
		status = mbus_client_connect_status_invalid_protocol;
		goto bail;
	}

	mbus_infof("connecting to server: '%s:%s:%d'", client->options->server_protocol, client->options->server_address, client->options->server_port);
	client->socket = mbus_socket_create(socket_domain, socket_type, mbus_socket_protocol_any);
	if (client->socket == NULL) {
		mbus_errorf("can not create event socket");
		status = mbus_client_connect_status_internal_error;
		goto bail;
	}

	rc = mbus_socket_set_reuseaddr(client->socket, 1);
	if (rc != 0) {
		mbus_errorf("can not reuse address");
		status = mbus_client_connect_status_internal_error;
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

	rc = mbus_socket_set_blocking(client->socket, 0);
	if (rc != 0) {
		mbus_errorf("can not set socket to nonblocking");
		status = mbus_client_connect_status_internal_error;
		goto bail;
	}
	rc = mbus_socket_connect(client->socket, client->options->server_address, client->options->server_port);
	if (rc == 0) {
		client->socket_connected = 1;
		mbus_debugf("connected to server: '%s:%s:%d'", client->options->server_protocol, client->options->server_address, client->options->server_port);
#if defined(SSL_ENABLE) && (SSL_ENABLE == 1)
		if (client->ssl.ssl != NULL) {
			SSL_set_fd(client->ssl.ssl, mbus_socket_get_fd(client->socket));
			rc = SSL_connect(client->ssl.ssl);
			if (rc <= 0) {
				int error;
				error = SSL_get_error(client->ssl.ssl, rc);
				if (error == SSL_ERROR_WANT_READ) {
					client->ssl.want_read = 1;
				} else if (error == SSL_ERROR_WANT_WRITE) {
					client->ssl.want_write = 1;
				} else if (error == SSL_ERROR_SYSCALL) {
					client->ssl.want_read = 1;
				} else {
					char ebuf[256];
					mbus_errorf("can not connect ssl: %d", error);
					error = ERR_get_error();
					while (error) {
						mbus_errorf("  error: %d, %s", error, ERR_error_string(error, ebuf));
						error = ERR_get_error();
					}
					goto bail;
				}
			}
		}
#endif
		rc = mbus_client_command_create_request(client);
		if (rc != 0) {
			mbus_errorf("can not create create request");
			status = mbus_client_connect_status_internal_error;
			goto bail;
		}
	} else if (rc == -EINPROGRESS) {
	} else if (rc == -ECONNREFUSED) {
		mbus_errorf("can not connect to server: '%s:%s:%d', rc: %d, %s", client->options->server_protocol, client->options->server_address, client->options->server_port, rc, strerror(-rc));
		status = mbus_client_connect_status_connection_refused;
	} else if (rc == -ENOENT) {
		mbus_errorf("can not connect to server: '%s:%s:%d', rc: %d, %s", client->options->server_protocol, client->options->server_address, client->options->server_port, rc, strerror(-rc));
		status = mbus_client_connect_status_server_unavailable;
	} else if (rc != 0) {
		mbus_errorf("can not connect to server: '%s:%s:%d', rc: %d, %s", client->options->server_protocol, client->options->server_address, client->options->server_port, rc, strerror(-rc));
		status = mbus_client_connect_status_internal_error;
		goto bail;
	}

	if (status != mbus_client_connect_status_success) {
		mbus_client_notify_connect(client, status);
		mbus_client_reset(client);
		if (client->options->connect_interval > 0) {
			client->state = mbus_client_state_connecting;
		} else {
			client->state = mbus_client_state_disconnected;
		}
	}
	return 0;
bail:	mbus_client_notify_connect(client, status);
	mbus_client_reset(client);
	return -1;
}

static int mbus_client_handle_result (struct mbus_client *client, const struct mbus_json *json)
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
		goto out;
	}

	if (request_get_callback(request) != NULL) {
		message.type = mbus_client_message_type_command;
		message.u.command.request = request_get_json(request);
		message.u.command.response = json;
		mbus_client_unlock(client);
		request_get_callback(request)(client, request_get_context(request), &message, mbus_client_command_status_success);
		mbus_client_lock(client);
	}

	request_destroy(request);
out:	return 0;
}

static int mbus_client_handle_event (struct mbus_client *client, const struct mbus_json *json)
{
	const char *source;
	const char *identifier;
	struct subscription *subscription;
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

	if (strcasecmp(MBUS_SERVER_IDENTIFIER, source) == 0 &&
	    strcasecmp(MBUS_SERVER_EVENT_PONG, identifier) == 0) {
		client->ping_wait_pong = 0;
		client->pong_recv_tsms = mbus_clock_get();
		client->pong_missed_count = 0;
	} else {
		message.type = mbus_client_message_type_event;
		message.u.event.payload = json;
		TAILQ_FOREACH(subscription, &client->subscriptions, subscriptions) {
			if ((strcmp(subscription_get_source(subscription), MBUS_METHOD_EVENT_SOURCE_ALL) == 0 ||
			     strcmp(subscription_get_source(subscription), source) == 0) &&
			    (strcmp(subscription_get_identifier(subscription), MBUS_METHOD_EVENT_IDENTIFIER_ALL) == 0 ||
			     strcmp(subscription_get_identifier(subscription), identifier) == 0)) {
				subscription_get_callback(subscription)(client, subscription_get_context(subscription), &message);
				break;
			}
		}
		if (subscription == NULL &&
		    client->options->callbacks.message != NULL) {
			mbus_client_unlock(client);
			client->options->callbacks.message(client, client->options->callbacks.context, &message);
			mbus_client_lock(client);
		}
	}

	return 0;
bail:	return -1;
}

static int mbus_client_handle_command (struct mbus_client *client, const struct mbus_json *json)
{
	int rc;
	const char *source;
	const char *identifier;
	struct routine *routine;

	struct mbus_client_message message;
	struct mbus_json *result_payload;

	int (*callback) (struct mbus_client *client, void *context, struct mbus_client_message *message);
	void *callback_context;

	callback = NULL;
	callback_context = NULL;
	result_payload = NULL;
	memset(&message, 0, sizeof(struct mbus_client_message));

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

	if (strcasecmp(MBUS_SERVER_IDENTIFIER, source) == 0 &&
	    strcasecmp(MBUS_SERVER_EVENT_PONG, identifier) == 0) {
		client->ping_wait_pong = 0;
		client->pong_recv_tsms = mbus_clock_get();
		client->pong_missed_count = 0;
	} else {
		TAILQ_FOREACH(routine, &client->routines, routines) {
			if (strcmp(routine_get_identifier(routine), identifier) == 0) {
				callback = routine_get_callback(routine);
				callback_context = routine_get_context(routine);
				break;
			}
		}
		if (routine == NULL &&
		    client->options->callbacks.routine != NULL) {
			callback = client->options->callbacks.routine;
			callback_context = client->options->callbacks.context;
		}
		if (callback != NULL) {
			message.type = mbus_client_message_type_routine;
			message.u.routine.request = json;
			message.u.routine.response = NULL;
			mbus_client_unlock(client);
			rc = callback(client, callback_context, &message);
			mbus_client_lock(client);
			result_payload = mbus_json_create_object();
			if (result_payload == NULL) {
				mbus_errorf("can not create result payload");
				goto bail;
			}
			rc = mbus_json_add_string_to_object_cs(result_payload, "destination", mbus_json_get_string_value(json, "source", NULL));
			if (rc != 0) {
				mbus_errorf("can not add destination to payload");
				goto bail;
			}
			rc = mbus_json_add_string_to_object_cs(result_payload, "identifier", mbus_json_get_string_value(json, "identifier", NULL));
			if (rc != 0) {
				mbus_errorf("can not add identifier to payload");
				goto bail;
			}
			rc = mbus_json_add_number_to_object_cs(result_payload, "sequence", mbus_json_get_int_value(json, "sequence", -1));
			if (rc != 0) {
				mbus_errorf("can not add sequence to payload");
				goto bail;
			}
			rc = mbus_json_add_number_to_object_cs(result_payload, "return", rc);
			if (rc != 0) {
				mbus_errorf("can not add return to payload");
				goto bail;
			}
			if (message.u.routine.response != NULL) {
				rc = mbus_json_add_item_to_object_cs(result_payload, "result", mbus_json_duplicate(message.u.routine.response, 1));
				if (rc != 0) {
					mbus_errorf("can not add result to payload");
					goto bail;
				}
			}
			rc = mbus_client_command_unlocked(client, MBUS_SERVER_IDENTIFIER, MBUS_SERVER_COMMAND_RESULT, result_payload, NULL, NULL);
			if (rc != 0) {
				mbus_errorf("can not call command");
				goto bail;
			}
		}
	}

	if (result_payload != NULL) {
		mbus_json_delete(result_payload);
	}
	if (message.u.routine.response != NULL) {
		mbus_json_delete(message.u.routine.response);
	}
	return 0;
bail:	if (message.u.routine.response != NULL) {
		mbus_json_delete(message.u.routine.response);
	}
	if (result_payload != NULL) {
		mbus_json_delete(result_payload);
	}
	return -1;
}

static void mbus_client_options_destroy (struct mbus_client_options *options)
{
	if (options == NULL) {
		return;
	}
	if (options->server_protocol != NULL) {
		free(options->server_protocol);
	}
	if (options->server_address != NULL) {
		free(options->server_address);
	}
	if (options->identifier != NULL) {
		free(options->identifier);
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
		if (options->server_protocol != NULL) {
			duplicate->server_protocol = strdup(options->server_protocol);
			if (duplicate->server_protocol == NULL) {
				mbus_errorf("can not allocate memory");
				goto bail;
			}
		}
		if (options->server_address != NULL) {
			duplicate->server_address = strdup(options->server_address);
			if (duplicate->server_address == NULL) {
				mbus_errorf("can not allocate memory");
				goto bail;
			}
		}
		duplicate->server_port = options->server_port;

		if (options->identifier != NULL) {
			duplicate->identifier = strdup(options->identifier);
			if (duplicate->identifier == NULL) {
				mbus_errorf("can not allocate memory");
				goto bail;
			}
		}
		duplicate->connect_timeout = options->connect_timeout;
		duplicate->connect_interval = options->connect_interval;
		duplicate->subscribe_timeout = options->subscribe_timeout;
		duplicate->register_timeout = options->register_timeout;
		duplicate->command_timeout = options->command_timeout;
		duplicate->publish_timeout = options->publish_timeout;
		duplicate->ping_interval = options->ping_interval;
		duplicate->ping_timeout = options->ping_timeout;
		duplicate->ping_threshold = options->ping_threshold;
		memcpy(&duplicate->callbacks, &options->callbacks, sizeof(options->callbacks));
	}
	return duplicate;
bail:	if (duplicate != NULL) {
		mbus_client_options_destroy(duplicate);
	}
	return NULL;
}

static int mbus_client_wakeup (struct mbus_client *client, enum wakeup_reason reason)
{
	int rc;
	rc = write(client->wakeup[1], &reason, sizeof(reason));
	if (rc != sizeof(reason)) {
		return -1;
	}
	return 0;
}

void mbus_client_usage (void)
{
	fprintf(stdout, "mbus client arguments:\n");
	fprintf(stdout, "  --mbus-debug-level             : debug level (default: %s)\n", mbus_debug_level_to_string(mbus_debug_level));
	fprintf(stdout, "  --mbus-client-identifier       : client identifier\n");
	fprintf(stdout, "  --mbus-client-server-protocol  : server protocol (default: %s)\n", MBUS_SERVER_PROTOCOL);
	fprintf(stdout, "  --mbus-client-server-address   : server address (default: %s)\n", MBUS_SERVER_ADDRESS);
	fprintf(stdout, "  --mbus-client-server-port      : server port (default: %d)\n", MBUS_SERVER_PORT);
	fprintf(stdout, "  --mbus-client-connect-timeout  : client connect timeout (default: %d)\n", MBUS_CLIENT_DEFAULT_CONNECT_TIMEOUT);
	fprintf(stdout, "  --mbus-client-connect-interval : client connect interval (default: %d)\n", MBUS_CLIENT_DEFAULT_CONNECT_INTERVAL);
	fprintf(stdout, "  --mbus-client-subscribe-timeout: client subscribe timeout (default: %d)\n", MBUS_CLIENT_DEFAULT_SUBSCRIBE_TIMEOUT);
	fprintf(stdout, "  --mbus-client-register-timeout : client register timeout (default: %d)\n", MBUS_CLIENT_DEFAULT_REGISTER_TIMEOUT);
	fprintf(stdout, "  --mbus-client-command-timeout  : client command timeout (default: %d)\n", MBUS_CLIENT_DEFAULT_COMMAND_TIMEOUT);
	fprintf(stdout, "  --mbus-client-publish-timeout  : client publish timeout (default: %d)\n", MBUS_CLIENT_DEFAULT_PUBLISH_TIMEOUT);
	fprintf(stdout, "  --mbus-client-ping-interval    : ping interval (default: %d)\n", MBUS_CLIENT_DEFAULT_PING_INTERVAL);
	fprintf(stdout, "  --mbus-client-ping-timeout     : ping timeout (default: %d)\n", MBUS_CLIENT_DEFAULT_PING_TIMEOUT);
	fprintf(stdout, "  --mbus-client-ping-threshold   : ping threshold (default: %d)\n", MBUS_CLIENT_DEFAULT_PING_THRESHOLD);
	fprintf(stdout, "  --mbus-help                    : this text\n");
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
			case OPTION_IDENTIFIER:
				options->identifier = optarg;
				break;
			case OPTION_SERVER_PROTOCOL:
				options->server_protocol = optarg;
				break;
			case OPTION_SERVER_ADDRESS:
				options->server_address = optarg;
				break;
			case OPTION_SERVER_PORT:
				options->server_port = atoi(optarg);
				break;
			case OPTION_CONNECT_TIMEOUT:
				options->connect_timeout = atoi(optarg);
				break;
			case OPTION_CONNECT_INTERVAL:
				options->connect_interval = atoi(optarg);
				break;
			case OPTION_SUBSCRIBE_TIMEOUT:
				options->subscribe_timeout = atoi(optarg);
				break;
			case OPTION_REGISTER_TIMEOUT:
				options->register_timeout = atoi(optarg);
				break;
			case OPTION_COMMAND_TIMEOUT:
				options->command_timeout = atoi(optarg);
				break;
			case OPTION_PUBLISH_TIMEOUT:
				options->publish_timeout = atoi(optarg);
				break;
			case OPTION_PING_INTERVAL:
				options->ping_interval = atoi(optarg);
				break;
			case OPTION_PING_TIMEOUT:
				options->ping_timeout = atoi(optarg);
				break;
			case OPTION_PING_THRESHOLD:
				options->ping_threshold = atoi(optarg);
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

	if (options.identifier == NULL) {
		options.identifier = "";
	}
	if (options.connect_timeout <= 0) {
		options.connect_timeout = MBUS_CLIENT_DEFAULT_CONNECT_TIMEOUT;
	}
	if (options.connect_interval <= 0) {
		options.connect_interval = MBUS_CLIENT_DEFAULT_CONNECT_INTERVAL;
	}
	if (options.subscribe_timeout <= 0) {
		options.subscribe_timeout = MBUS_CLIENT_DEFAULT_SUBSCRIBE_TIMEOUT;
	}
	if (options.register_timeout <= 0) {
		options.register_timeout = MBUS_CLIENT_DEFAULT_REGISTER_TIMEOUT;
	}
	if (options.command_timeout <= 0) {
		options.command_timeout = MBUS_CLIENT_DEFAULT_COMMAND_TIMEOUT;
	}
	if (options.publish_timeout <= 0) {
		options.publish_timeout = MBUS_CLIENT_DEFAULT_PUBLISH_TIMEOUT;
	}

	if (options.server_protocol == NULL) {
		options.server_protocol = MBUS_SERVER_PROTOCOL;
	}

	if (options.ping_interval == 0) {
		options.ping_interval = MBUS_CLIENT_DEFAULT_PING_INTERVAL;
	}
	if (options.ping_timeout == 0) {
		options.ping_timeout = MBUS_CLIENT_DEFAULT_PING_TIMEOUT;
	}
	if (options.ping_threshold == 0) {
		options.ping_threshold = MBUS_CLIENT_DEFAULT_PING_THRESHOLD;
	}
	if (options.ping_timeout > (options.ping_interval * 2) / 3) {
		options.ping_timeout = (options.ping_interval * 2) / 3;
	}

	if (strcmp(options.server_protocol, MBUS_SERVER_TCP_PROTOCOL) == 0) {
		if (options.server_port <= 0) {
			options.server_port = MBUS_SERVER_TCP_PORT;
		}
		if (options.server_address == NULL) {
			options.server_address = MBUS_SERVER_TCP_ADDRESS;
		}
	} else if (strcmp(options.server_protocol, MBUS_SERVER_UDS_PROTOCOL) == 0) {
		if (options.server_port <= 0) {
			options.server_port = MBUS_SERVER_UDS_PORT;
		}
		if (options.server_address == NULL) {
			options.server_address = MBUS_SERVER_UDS_ADDRESS;
		}
	} else if (strcmp(options.server_protocol, MBUS_SERVER_TCPS_PROTOCOL) == 0) {
		if (options.server_port <= 0) {
			options.server_port = MBUS_SERVER_TCPS_PORT;
		}
		if (options.server_address == NULL) {
			options.server_address = MBUS_SERVER_TCPS_ADDRESS;
		}
	} else if (strcmp(options.server_protocol, MBUS_SERVER_UDSS_PROTOCOL) == 0) {
		if (options.server_port <= 0) {
			options.server_port = MBUS_SERVER_UDSS_PORT;
		}
		if (options.server_address == NULL) {
			options.server_address = MBUS_SERVER_UDSS_ADDRESS;
		}
	} else {
		mbus_errorf("invalid server protocol: %s", options.server_protocol);
		goto bail;
	}

	mbus_infof("creating client: '%s'", options.identifier);
	mbus_infof("using mbus version '%s, %s'", mbus_git_commit(), mbus_git_revision());

	client = malloc(sizeof(struct mbus_client));
	if (client == NULL) {
		mbus_errorf("can not allocate memory");
		goto bail;
	}
	memset(client, 0, sizeof(struct mbus_client));
	pthread_mutex_init(&client->mutex,NULL);
	client->state = mbus_client_state_disconnected;
	client->socket = NULL;
	client->wakeup[0] = -1;
	client->wakeup[1] = -1;
	TAILQ_INIT(&client->requests);
	TAILQ_INIT(&client->pendings);
	TAILQ_INIT(&client->routines);
	TAILQ_INIT(&client->subscriptions);

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

#if defined(SSL_ENABLE) && (SSL_ENABLE == 1)
	if (strcmp(options.server_protocol, MBUS_SERVER_TCPS_PROTOCOL) == 0 ||
	    strcmp(options.server_protocol, MBUS_SERVER_UDSS_PROTOCOL) == 0) {
		mbus_infof("using openssl version '%s'", SSLeay_version(SSLEAY_VERSION));
		client->ssl.method = SSLv23_method();
		if (client->ssl.method == NULL) {
			mbus_errorf("ssl client method is invalid");
			goto bail;
		}
		client->ssl.context = SSL_CTX_new(client->ssl.method);
		if (client->ssl.context == NULL) {
			mbus_errorf("can not create ssl");
			goto bail;
		}
		client->ssl.ssl = SSL_new(client->ssl.context);
		if (client->ssl.ssl == NULL) {
			mbus_errorf("can not create ssl");
			goto bail;
		}
	}
#endif

	return client;
bail:	if (client != NULL) {
		mbus_client_destroy(client);
	}
	return NULL;
}

void mbus_client_destroy (struct mbus_client *client)
{
	if (client == NULL) {
		return;
	}
	mbus_client_reset(client);
	if (client->incoming != NULL) {
		mbus_buffer_destroy(client->incoming);
	}
	if (client->outgoing != NULL) {
		mbus_buffer_destroy(client->outgoing);
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
#if defined(SSL_ENABLE) && (SSL_ENABLE == 1)
	if (client->ssl.ssl != NULL) {
		SSL_free(client->ssl.ssl);
	}
	if (client->ssl.context != NULL) {
		SSL_CTX_free(client->ssl.context);
	}
#endif
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

enum mbus_client_state mbus_client_get_state (struct mbus_client *client)
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

const char * mbus_client_get_identifier (struct mbus_client *client)
{
	const char *identifier;
	if (client == NULL) {
		mbus_errorf("client is invalid");
		goto bail;
	}
	mbus_client_lock(client);
	identifier = client->identifier;
	mbus_client_unlock(client);
	return identifier;
bail:	if (client != NULL) {
		mbus_client_unlock(client);
	}
	return NULL;
}

int mbus_client_get_wakeup_fd (struct mbus_client *client)
{
	int rc;
	if (client == NULL) {
		mbus_errorf("client is invalid");
		goto bail;
	}
	mbus_client_lock(client);
	if (client->socket == NULL) {
		goto bail;
	}
	rc = client->wakeup[0];
	mbus_client_unlock(client);
	return rc;
bail:	if (client != NULL) {
		mbus_client_unlock(client);
	}
	return -1;
}

int mbus_client_get_wakeup_fd_events (struct mbus_client *client)
{
	int rc;
	if (client == NULL) {
		mbus_errorf("client is invalid");
		goto bail;
	}
	mbus_client_lock(client);
	if (client->socket == NULL) {
		goto bail;
	}
	rc = POLLIN;
	mbus_client_unlock(client);
	return rc;
bail:	if (client != NULL) {
		mbus_client_unlock(client);
	}
	return 0;
}

int mbus_client_get_connection_fd (struct mbus_client *client)
{
	int rc;
	if (client == NULL) {
		mbus_errorf("client is invalid");
		goto bail;
	}
	mbus_client_lock(client);
	if (client->socket == NULL) {
		goto bail;
	}
	rc = mbus_socket_get_fd(client->socket);
	mbus_client_unlock(client);
	return rc;
bail:	if (client != NULL) {
		mbus_client_unlock(client);
	}
	return -1;
}

int mbus_client_get_connection_fd_events (struct mbus_client *client)
{
	int rc;
	if (client == NULL) {
		mbus_errorf("client is invalid");
		goto bail;
	}
	mbus_client_lock(client);
	if (client->socket == NULL) {
		goto bail;
	}
	rc = POLLIN;
	if (mbus_buffer_length(client->outgoing) > 0
#if defined(SSL_ENABLE) && (SSL_ENABLE == 1)
	    || client->ssl.want_write != 0
#endif
	) {
		rc |= POLLOUT;
	}
	mbus_client_unlock(client);
	return rc;
bail:	if (client != NULL) {
		mbus_client_unlock(client);
	}
	return 0;
}

int mbus_client_has_pending (struct mbus_client *client)
{
	int rc;
	if (client == NULL) {
		mbus_errorf("client is invalid");
		goto bail;
	}
	mbus_client_lock(client);
	if (client->requests.count > 0 ||
	    client->pendings.count > 0 ||
	    mbus_buffer_length(client->incoming) > 0 ||
	    mbus_buffer_length(client->outgoing) > 0) {
		rc = 1;
	} else {
		rc = 0;
	}
	mbus_client_unlock(client);
	return rc;
bail:	if (client != NULL) {
		mbus_client_unlock(client);
	}
	return -1;
}

int mbus_client_connect (struct mbus_client *client)
{
	int rc;
	if (client == NULL) {
		mbus_errorf("client is invalid");
		goto bail;
	}
	mbus_client_lock(client);
	if (client->state != mbus_client_state_connected) {
		client->state = mbus_client_state_connecting;
		rc = mbus_client_wakeup(client, wakeup_reason_connect);
		if (rc != 0) {
			mbus_errorf("can not wakeup client");
			goto bail;
		}
	}
	mbus_client_unlock(client);
	return 0;
bail:	if (client != NULL) {
		mbus_client_unlock(client);
	}
	return -1;
}

int mbus_client_disconnect (struct mbus_client *client)
{
	int rc;
	if (client == NULL) {
		mbus_errorf("client is invalid");
		goto bail;
	}
	mbus_client_lock(client);
	if (client->state != mbus_client_state_disconnected) {
		client->state = mbus_client_state_disconnecting;
		rc = mbus_client_wakeup(client, wakeup_reason_disconnect);
		if (rc != 0) {
			mbus_errorf("can not wakeup client");
			goto bail;
		}
	}
	mbus_client_unlock(client);
	return 0;
bail:	if (client != NULL) {
		mbus_client_unlock(client);
	}
	return -1;
}

int mbus_client_subscribe (struct mbus_client *client, const char *source, const char *event)
{
	return mbus_client_subscribe_timeout(client, source, event, client->options->subscribe_timeout);
}

int mbus_client_subscribe_timeout (struct mbus_client *client, const char *source, const char *event, int timeout)
{
	return mbus_client_subscribe_callback_timeout(client, source, event, NULL, NULL, timeout);
}

int mbus_client_subscribe_callback (struct mbus_client *client, const char *source, const char *event, void (*callback) (struct mbus_client *client, void *context, struct mbus_client_message *message), void *context)
{
	return mbus_client_subscribe_callback_timeout(client, source, event, callback, context, client->options->subscribe_timeout);
}

int mbus_client_subscribe_callback_timeout (struct mbus_client *client, const char *source, const char *event, void (*callback) (struct mbus_client *client, void *context, struct mbus_client_message *message), void *context, int timeout)
{
	int rc;
	struct mbus_json *payload;
	struct subscription *subscription;
	struct subscription *nsubscription;
	payload = NULL;
	subscription = NULL;
	if (client == NULL) {
		mbus_errorf("client is invalid");
		goto bail;
	}
	if (client->state != mbus_client_state_connected) {
		mbus_errorf("client is not connected");
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
	if (timeout < 0) {
		mbus_debugf("timeout is invalid, using: %d", client->options->subscribe_timeout);
		timeout = client->options->subscribe_timeout;
	}
	TAILQ_FOREACH_SAFE(subscription, &client->subscriptions, subscriptions, nsubscription) {
		if (strcmp(subscription_get_source(subscription), source) == 0 &&
		    strcmp(subscription_get_identifier(subscription), event) == 0) {
			goto out;
		}
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
	if (callback != NULL) {
		subscription = subscription_create(source, event, callback, context);
		if (subscription == NULL) {
			mbus_errorf("can not create subscription");
			goto bail;
		}
	}
	rc = mbus_client_command_timeout(client, MBUS_SERVER_IDENTIFIER, MBUS_SERVER_COMMAND_SUBSCRIBE, payload, mbus_client_command_subscribe_response, subscription, timeout);
	if (rc != 0) {
		mbus_errorf("can not execute command");
		goto bail;
	}
	mbus_json_delete(payload);
out:	return 0;
bail:	if (payload != NULL) {
		mbus_json_delete(payload);
	}
	if (subscription != NULL) {
		subscription_destroy(subscription);
	}
	return -1;
}

int mbus_client_unsubscribe (struct mbus_client *client, const char *source, const char *event)
{
	return mbus_client_unsubscribe_timeout(client, source, event, client->options->subscribe_timeout);
}

int mbus_client_unsubscribe_timeout (struct mbus_client *client, const char *source, const char *event, int timeout)
{
	int rc;
	struct mbus_json *payload;
	struct subscription *subscription;
	struct subscription *nsubscription;
	payload = NULL;
	if (client == NULL) {
		mbus_errorf("client is invalid");
		goto bail;
	}
	if (client->state != mbus_client_state_connected) {
		mbus_errorf("client is not connected");
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
	if (timeout < 0) {
		mbus_debugf("timeout is invalid, using: %d", client->options->subscribe_timeout);
		timeout = client->options->subscribe_timeout;
	}
	TAILQ_FOREACH_SAFE(subscription, &client->subscriptions, subscriptions, nsubscription) {
		if (strcmp(subscription_get_source(subscription), source) == 0 &&
		    strcmp(subscription_get_identifier(subscription), event) == 0) {
			TAILQ_REMOVE(&client->subscriptions, subscription, subscriptions);
			subscription_destroy(subscription);
		}
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
	rc = mbus_client_command_timeout(client, MBUS_SERVER_IDENTIFIER, MBUS_SERVER_COMMAND_UNSUBSCRIBE, payload, mbus_client_command_unsubscribe_response, NULL, timeout);
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
	return mbus_client_publish_timeout(client, event, payload, client->options->publish_timeout);
}

int mbus_client_publish_unlocked (struct mbus_client *client, const char *event, const struct mbus_json *payload)
{
	return mbus_client_publish_timeout_unlocked(client, event, payload, client->options->publish_timeout);
}

int mbus_client_publish_timeout (struct mbus_client *client, const char *event, const struct mbus_json *payload, int timeout)
{
	return mbus_client_publish_to_timeout(client, NULL, event, payload, timeout);
}

int mbus_client_publish_timeout_unlocked (struct mbus_client *client, const char *event, const struct mbus_json *payload, int timeout)
{
	return mbus_client_publish_to_timeout_unlocked(client, NULL, event, payload, timeout);
}

int mbus_client_publish_to (struct mbus_client *client, const char *destination, const char *event, const struct mbus_json *payload)
{
	return mbus_client_publish_to_timeout(client, destination, event, payload, client->options->publish_timeout);
}

int mbus_client_publish_to_unlocked (struct mbus_client *client, const char *destination, const char *event, const struct mbus_json *payload)
{
	return mbus_client_publish_to_timeout_unlocked(client, destination, event, payload, client->options->publish_timeout);
}

int mbus_client_publish_to_timeout (struct mbus_client *client, const char *destination, const char *event, const struct mbus_json *payload, int timeout)
{
	int rc;
	if (client == NULL) {
		mbus_errorf("client is invalid");
		goto bail;
	}
	mbus_client_lock(client);
	rc = mbus_client_publish_to_timeout_unlocked(client, destination, event, payload, timeout);
	mbus_client_unlock(client);
	return rc;
bail:	if (client != NULL) {
		mbus_client_unlock(client);
	}
	return -1;
}

int mbus_client_publish_to_timeout_unlocked (struct mbus_client *client, const char *destination, const char *event, const struct mbus_json *payload, int timeout)
{
	struct request *request;
	request = NULL;
	if (client == NULL) {
		mbus_errorf("client is invalid");
		goto bail;
	}
	if (client->state != mbus_client_state_connected) {
		mbus_errorf("client is not connected");
		goto bail;
	}
	if (destination == NULL) {
		mbus_debugf("destination is invalid, using: %s", MBUS_METHOD_EVENT_DESTINATION_SUBSCRIBERS);
		destination = MBUS_METHOD_EVENT_DESTINATION_SUBSCRIBERS;
	}
	if (event == NULL) {
		mbus_errorf("command is invalid");
		goto bail;
	}
	if (timeout < 0) {
		mbus_debugf("timeout is invalid, using: %d", client->options->publish_timeout);
		timeout = client->options->publish_timeout;
	}
	request = request_create(MBUS_METHOD_TYPE_EVENT, destination, event, client->sequence, payload, NULL, NULL, timeout);
	if (request == NULL) {
		mbus_errorf("can not create request");
		goto bail;
	}
	client->sequence += 1;
	if (client->sequence >= MBUS_METHOD_SEQUENCE_END) {
		client->sequence = MBUS_METHOD_SEQUENCE_START;
	}
	TAILQ_INSERT_TAIL(&client->requests, request, requests);
	return 0;
bail:	return -1;
}

int mbus_client_publish_sync (struct mbus_client *client, const char *event, const struct mbus_json *payload)
{
	return mbus_client_publish_sync_timeout(client, event, payload, client->options->publish_timeout);
}

int mbus_client_publish_sync_unlocked (struct mbus_client *client, const char *event, const struct mbus_json *payload)
{
	return mbus_client_publish_sync_timeout_unlocked(client, event, payload, client->options->publish_timeout);
}

int mbus_client_publish_sync_timeout (struct mbus_client *client, const char *event, const struct mbus_json *payload, int timeout)
{
	return mbus_client_publish_sync_to_timeout(client, NULL, event, payload, timeout);
}

int mbus_client_publish_sync_timeout_unlocked (struct mbus_client *client, const char *event, const struct mbus_json *payload, int timeout)
{
	return mbus_client_publish_sync_to_timeout_unlocked(client, NULL, event, payload, timeout);
}

int mbus_client_publish_sync_to (struct mbus_client *client, const char *destination, const char *event, const struct mbus_json *payload)
{
	return mbus_client_publish_sync_to_timeout(client, destination, event, payload, client->options->publish_timeout);
}

int mbus_client_publish_sync_to_unlocked (struct mbus_client *client, const char *destination, const char *event, const struct mbus_json *payload)
{
	return mbus_client_publish_sync_to_timeout_unlocked(client, destination, event, payload, client->options->publish_timeout);
}

int mbus_client_publish_sync_to_timeout (struct mbus_client *client, const char *destination, const char *event, const struct mbus_json *payload, int timeout)
{
	int rc;
	if (client == NULL) {
		mbus_errorf("client is invalid");
		goto bail;
	}
	mbus_client_lock(client);
	rc = mbus_client_publish_sync_to_timeout_unlocked(client, destination, event, payload, timeout);
	mbus_client_unlock(client);
	return rc;
bail:	if (client != NULL) {
		mbus_client_unlock(client);
	}
	return -1;
}

int mbus_client_publish_sync_to_timeout_unlocked (struct mbus_client *client, const char *destination, const char *event, const struct mbus_json *payload, int timeout)
{
	struct request *request;
	struct mbus_json *jdata;
	struct mbus_json *jpayload;
	jdata = NULL;
	jpayload = NULL;
	request = NULL;
	if (client == NULL) {
		mbus_errorf("client is invalid");
		goto bail;
	}
	if (client->state != mbus_client_state_connected) {
		mbus_errorf("client is not connected");
		goto bail;
	}
	if (destination == NULL) {
		mbus_debugf("destination is invalid, using: %s", MBUS_METHOD_EVENT_DESTINATION_SUBSCRIBERS);
		destination = MBUS_METHOD_EVENT_DESTINATION_SUBSCRIBERS;
	}
	if (event == NULL) {
		mbus_errorf("event is invalid");
		goto bail;
	}
	if (timeout < 0) {
		mbus_debugf("timeout is invalid, using: %d", client->options->publish_timeout);
		timeout = client->options->publish_timeout;
	}
	if (payload == NULL) {
		jdata = mbus_json_create_object();
	} else {
		jdata = mbus_json_duplicate(payload, 1);
	}
	if (jdata == NULL) {
		mbus_errorf("can not create data");
		goto bail;
	}
	jpayload = mbus_json_create_object();
	if (jpayload == NULL) {
		mbus_errorf("can not create command payload");
		goto bail;
	}
	mbus_json_add_string_to_object_cs(jpayload, "destination", destination);
	mbus_json_add_string_to_object_cs(jpayload, "identifier", event);
	mbus_json_add_item_to_object_cs(jpayload, "payload", jdata);
	jdata = NULL;
	request = request_create(MBUS_METHOD_TYPE_COMMAND, MBUS_SERVER_IDENTIFIER, MBUS_SERVER_COMMAND_EVENT, client->sequence, jpayload, mbus_client_command_event_response, client, timeout);
	if (request == NULL) {
		mbus_errorf("can not create request");
		goto bail;
	}
	client->sequence += 1;
	if (client->sequence >= MBUS_METHOD_SEQUENCE_END) {
		client->sequence = MBUS_METHOD_SEQUENCE_START;
	}
	TAILQ_INSERT_TAIL(&client->requests, request, requests);
	mbus_json_delete(jpayload);
	return 0;
bail:	if (jpayload != NULL) {
		mbus_json_delete(jpayload);
	}
	if (jdata != NULL) {
		mbus_json_delete(jdata);
	}
	return -1;
}

int mbus_client_command (struct mbus_client *client, const char *destination, const char *command, const struct mbus_json *payload, void (*callback) (struct mbus_client *client, void *context, struct mbus_client_message *message, enum mbus_client_command_status status), void *context)
{
	return mbus_client_command_timeout(client, destination, command, payload, callback, context, client->options->command_timeout);
}

int mbus_client_command_unlocked (struct mbus_client *client, const char *destination, const char *command, const struct mbus_json *payload, void (*callback) (struct mbus_client *client, void *context, struct mbus_client_message *message, enum mbus_client_command_status status), void *context)
{
	return mbus_client_command_timeout_unlocked(client, destination, command, payload, callback, context, client->options->command_timeout);
}

int mbus_client_command_timeout (struct mbus_client *client, const char *destination, const char *command, const struct mbus_json *payload, void (*callback) (struct mbus_client *client, void *context, struct mbus_client_message *message, enum mbus_client_command_status status), void *context, int timeout)
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

int mbus_client_command_timeout_unlocked (struct mbus_client *client, const char *destination, const char *command, const struct mbus_json *payload, void (*callback) (struct mbus_client *client, void *context, struct mbus_client_message *message, enum mbus_client_command_status status), void *context, int timeout)
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
	if (strcmp(command, MBUS_SERVER_COMMAND_CREATE) == 0) {
		if (client->state != mbus_client_state_connecting) {
			mbus_errorf("client is not connecting: %d", client->state);
			goto bail;
		}
	} else {
		if (client->state != mbus_client_state_connected) {
			mbus_errorf("client is not connected");
			goto bail;
		}
	}
	if (timeout < 0) {
		mbus_debugf("timeout is invalid, using: %d", client->options->command_timeout);
		timeout = client->options->command_timeout;
	}
	request = request_create(MBUS_METHOD_TYPE_COMMAND, destination, command, client->sequence, payload, callback, context, timeout);
	if (request == NULL) {
		mbus_errorf("can not create request");
		goto bail;
	}
	client->sequence += 1;
	if (client->sequence >= MBUS_METHOD_SEQUENCE_END) {
		client->sequence = MBUS_METHOD_SEQUENCE_START;
	}
	TAILQ_INSERT_TAIL(&client->requests, request, requests);
	return 0;
bail:	return -1;
}

int mbus_client_register (struct mbus_client *client, const char *command)
{
	return mbus_client_register_timeout(client, command, client->options->register_timeout);
}

int mbus_client_register_timeout (struct mbus_client *client, const char *command, int timeout)
{
	return mbus_client_register_callback_timeout(client, command, NULL, NULL, timeout);
}

int mbus_client_register_callback (struct mbus_client *client, const char *command, int (*callback) (struct mbus_client *client, void *context, struct mbus_client_message *message), void *context)
{
	return mbus_client_register_callback_timeout(client, command, callback, context, client->options->register_timeout);
}

int mbus_client_register_callback_timeout (struct mbus_client *client, const char *command, int (*callback) (struct mbus_client *client, void *context, struct mbus_client_message *message), void *context, int timeout)
{
	int rc;
	struct routine *routine;
	struct mbus_json *payload;
	routine = NULL;
	payload = NULL;
	if (client == NULL) {
		mbus_errorf("client is invalid");
		goto bail;
	}
	if (client->state != mbus_client_state_connected) {
		mbus_errorf("client is not connected");
		goto bail;
	}
	if (command == NULL) {
		mbus_errorf("command is invalid");
		goto bail;
	}
	if (timeout < 0) {
		mbus_debugf("timeout is invalid, using: %d", client->options->register_timeout);
		timeout = client->options->register_timeout;
	}
	payload = mbus_json_create_object();
	if (payload == NULL) {
		mbus_errorf("can not create json object");
		goto bail;
	}
	rc = mbus_json_add_string_to_object_cs(payload, "command", command);
	if (rc != 0) {
		mbus_errorf("can not add string to json object");
		goto bail;
	}
	if (callback != NULL) {
		routine = routine_create(command, callback, context);
		if (routine == NULL) {
			mbus_errorf("can not create routine");
			goto bail;
		}
	}
	rc = mbus_client_command_timeout(client, MBUS_SERVER_IDENTIFIER, MBUS_SERVER_COMMAND_REGISTER, payload, mbus_client_command_register_response, routine, timeout);
	if (rc != 0) {
		mbus_errorf("can not execute command");
		goto bail;
	}
	mbus_json_delete(payload);
	return 0;
bail:	if (payload != NULL) {
		mbus_json_delete(payload);
	}
	if (routine != NULL) {
		routine_destroy(routine);
	}
	return -1;
}

int mbus_client_unregister (struct mbus_client *client, const char *command)
{
	return mbus_client_unregister_timeout(client, command, client->options->register_timeout);
}

int mbus_client_unregister_timeout (struct mbus_client *client, const char *command, int timeout)
{
	int rc;
	struct mbus_json *payload;
	struct routine *routine;
	struct routine *nroutine;
	payload = NULL;
	if (client == NULL) {
		mbus_errorf("client is invalid");
		goto bail;
	}
	if (client->state != mbus_client_state_connected) {
		mbus_errorf("client is not connected");
		goto bail;
	}
	if (command == NULL) {
		mbus_errorf("command is invalid");
		goto bail;
	}
	if (timeout < 0) {
		mbus_debugf("timeout is invalid, using: %d", client->options->register_timeout);
		timeout = client->options->register_timeout;
	}
	TAILQ_FOREACH_SAFE(routine, &client->routines, routines, nroutine) {
		if (strcmp(routine_get_identifier(routine), command) == 0) {
			TAILQ_REMOVE(&client->routines, routine, routines);
			routine_destroy(routine);
		}
	}
	payload = mbus_json_create_object();
	if (payload == NULL) {
		mbus_errorf("can not create json object");
		goto bail;
	}
	rc = mbus_json_add_string_to_object_cs(payload, "command", command);
	if (rc != 0) {
		mbus_errorf("can not add string to json object");
		goto bail;
	}
	rc = mbus_client_command_timeout(client, MBUS_SERVER_IDENTIFIER, MBUS_SERVER_COMMAND_UNREGISTER, payload, mbus_client_command_unregister_response, NULL, timeout);
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

int mbus_client_break (struct mbus_client *client)
{
	int rc;
	if (client == NULL) {
		mbus_errorf("client is invalid");
		goto bail;
	}
	mbus_client_lock(client);
	rc = mbus_client_wakeup(client, wakeup_reason_break);
	if (rc != 0) {
		mbus_errorf("can not wakeup client");
		goto bail;
	}
	mbus_client_unlock(client);
	return rc;
bail:	if (client != NULL) {
		mbus_client_unlock(client);
	}
	return -1;
}

int mbus_client_get_run_timeout_unlocked (struct mbus_client *client)
{
	int timeout;
	unsigned long current;
	struct request *request;
	timeout = MBUS_CLIENT_DEFAULT_RUN_TIMEOUT;
	current = mbus_clock_get();
	if (client == NULL) {
		mbus_errorf("client is invalid");
		goto bail;
	}
	if (client->state == mbus_client_state_connecting) {
		if (client->socket == NULL) {
			timeout = MIN(timeout, client->options->connect_interval);
		} else if (client->socket_connected == 0) {
			timeout = MIN(timeout, client->options->connect_timeout);
		}
	} else if (client->state == mbus_client_state_connected) {
		if (client->ping_interval > 0) {
			if (mbus_clock_after(current, client->ping_send_tsms + client->ping_interval)) {
				timeout = 0;
			} else {
				timeout = MIN(timeout, (long) ((client->ping_send_tsms + client->ping_interval) - (current)));
			}
		}
		TAILQ_FOREACH(request, &client->requests, requests) {
			if (request_get_timeout(request) >= 0) {
				if (mbus_clock_after(current, request_get_created_at(request) + request_get_timeout(request))) {
					timeout = 0;
				} else {
					timeout = MIN(timeout, (long) ((request_get_created_at(request) + request_get_timeout(request)) - (current)));
				}
			}
		}
		TAILQ_FOREACH(request, &client->pendings, requests) {
			if (request_get_timeout(request) >= 0) {
				if (mbus_clock_after(current, request_get_created_at(request) + request_get_timeout(request))) {
					timeout = 0;
				} else {
					timeout = MIN(timeout, (long) ((request_get_created_at(request) + request_get_timeout(request)) - (current)));
				}
			}
		}
	} else if (client->state == mbus_client_state_disconnecting) {
		timeout = 0;
	} else if (client->state == mbus_client_state_disconnected) {
		if (client->options->connect_interval > 0) {
			if (mbus_clock_after(current, client->connect_tsms + client->options->connect_interval)) {
				timeout = 0;
			} else {
				timeout = MIN(timeout, (long) ((client->connect_tsms + client->options->connect_interval) - (current)));
			}
		}
	}
	return timeout;
bail:	return -1;
}

int mbus_client_get_run_timeout (struct mbus_client *client)
{
	int timeout;
	timeout = -1;
	if (client == NULL) {
		mbus_errorf("client is invalid");
		goto bail;
	}
	mbus_client_lock(client);
	timeout = mbus_client_get_run_timeout_unlocked(client);
	mbus_client_unlock(client);
	return timeout;
bail:	if (client != NULL) {
		mbus_client_unlock(client);
	}
	return -1;
}

int mbus_client_run (struct mbus_client *client, int timeout)
{
	int rc;
	unsigned long current;

	struct request *request;
	struct request *nrequest;

	int read_rc;
	int write_rc;
	int ptimeout;
	int npollfds;
	struct pollfd pollfds[2];

	if (client == NULL) {
		mbus_errorf("client is invalid");
		goto bail;
	}

	mbus_client_lock(client);

	if (client->state == mbus_client_state_connecting) {
		if (client->socket == NULL) {
			current = mbus_clock_get();
			if (client->options->connect_interval <= 0 ||
			    mbus_clock_after(current, client->connect_tsms + client->options->connect_interval)) {
				mbus_debugf("connecting");
				client->connect_tsms = mbus_clock_get();
				rc = mbus_client_run_connect(client);
				if (rc != 0) {
					mbus_errorf("can not connect client");;
					goto bail;
				}
			}
		}
	} else if (client->state == mbus_client_state_connected) {
	} else if (client->state == mbus_client_state_disconnecting) {
		mbus_client_reset(client);
		mbus_client_notify_disconnect(client, mbus_client_disconnect_status_success);
		client->state = mbus_client_state_disconnected;
		goto out;
	} else if (client->state == mbus_client_state_disconnected) {
		if (client->options->connect_interval > 0) {
			client->state = mbus_client_state_connecting;
			goto out;
		}
	} else {
		mbus_errorf("client state: %d is invalid", client->state);
		goto bail;
	}

	npollfds = 0;
	memset(pollfds, 0, sizeof(pollfds));

	pollfds[npollfds].events = POLLIN;
	pollfds[npollfds].revents = 0;
	pollfds[npollfds].fd = client->wakeup[0];
	npollfds += 1;
	if (client->socket != NULL) {
		pollfds[npollfds].revents = 0;
		pollfds[npollfds].fd = mbus_socket_get_fd(client->socket);
		if (client->state == mbus_client_state_connecting &&
		    client->socket_connected == 0) {
			pollfds[npollfds].events |= POLLOUT;
		} else {
			pollfds[npollfds].events |= POLLIN;
			if (mbus_buffer_length(client->outgoing) > 0) {
				pollfds[npollfds].events |= POLLOUT;
			}
		}
		npollfds += 1;
	}
	ptimeout = mbus_client_get_run_timeout_unlocked(client);
	if (ptimeout < 0 || timeout < 0) {
		ptimeout = MAX(ptimeout, timeout);
	} else {
		ptimeout = MIN(ptimeout, timeout);
	}
	mbus_client_unlock(client);
	rc = poll(pollfds, npollfds, ptimeout);
	mbus_client_lock(client);
	if (rc == 0) {
		goto out;
	}
	if (rc < 0) {
		mbus_errorf("poll error");
		goto bail;
	}

	if (pollfds[0].revents & POLLIN) {
		enum wakeup_reason reason;
		rc = read(pollfds[0].fd, &reason, sizeof(reason));
		if (rc != sizeof(reason)) {
			mbus_errorf("can not read wakeup reason");
			goto bail;
		}
	}

	if (pollfds[1].revents & POLLIN) {
		rc = mbus_buffer_reserve(client->incoming, mbus_buffer_length(client->incoming) + 1024);
		if (rc != 0) {
			mbus_errorf("can not reserve client buffer");
			goto bail;
		}
#if defined(SSL_ENABLE) && (SSL_ENABLE == 1)
			if (client->ssl.ssl == NULL) {
#endif
				read_rc = read(mbus_socket_get_fd(client->socket),
					       mbus_buffer_base(client->incoming) + mbus_buffer_length(client->incoming),
					       mbus_buffer_size(client->incoming) - mbus_buffer_length(client->incoming));
#if defined(SSL_ENABLE) && (SSL_ENABLE == 1)
			} else {
				read_rc = 0;
				do {
					rc = mbus_buffer_reserve(client->incoming, (mbus_buffer_length(client->incoming) + read_rc) + 1024);
					if (rc != 0) {
						mbus_errorf("can not reserve client buffer");
						goto bail;
					}
					client->ssl.want_read = 0;
					rc = SSL_read(client->ssl.ssl,
							mbus_buffer_base(client->incoming) + (mbus_buffer_length(client->incoming) + read_rc),
							mbus_buffer_size(client->incoming) - (mbus_buffer_length(client->incoming) + read_rc));
					if (rc <= 0) {
						int error;
						error = SSL_get_error(client->ssl.ssl, rc);
						if (error == SSL_ERROR_WANT_READ) {
							read_rc = 0;
							errno = EAGAIN;
							client->ssl.want_read = 1;
						} else if (error == SSL_ERROR_WANT_WRITE) {
							read_rc = 0;
							errno = EAGAIN;
							client->ssl.want_write = 1;
						} else if (error == SSL_ERROR_SYSCALL) {
							read_rc = -1;
							errno = EIO;
						} else {
							char ebuf[256];
							mbus_errorf("can not read ssl: %d", error);
							error = ERR_get_error();
							while (error) {
								mbus_errorf("  error: %d, %s", error, ERR_error_string(error, ebuf));
								error = ERR_get_error();
							}
							read_rc = -1;
							errno = EIO;
						}
					} else {
						read_rc += rc;
					}
				} while (SSL_pending(client->ssl.ssl));
			}
#endif
		if (read_rc <= 0) {
			if (errno == EINTR) {
			} else if (errno == EAGAIN) {
			} else if (errno == EWOULDBLOCK) {
			} else {
				mbus_errorf("connection reset by server");
				mbus_client_notify_disconnect(client, mbus_client_disconnect_status_connection_closed);
				mbus_client_reset(client);
				client->state = mbus_client_state_disconnected;
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

	if (pollfds[1].revents & POLLOUT) {
		if (client->state == mbus_client_state_connecting &&
		    client->socket_connected == 0) {
			rc = mbus_socket_get_error(client->socket);
			if (rc == 0) {
				client->socket_connected = 1;
				mbus_debugf("connected to server: '%s:%s:%d'", client->options->server_protocol, client->options->server_address, client->options->server_port);
#if defined(SSL_ENABLE) && (SSL_ENABLE == 1)
				if (client->ssl.ssl != NULL) {
					SSL_set_fd(client->ssl.ssl, mbus_socket_get_fd(client->socket));
					rc = SSL_connect(client->ssl.ssl);
					if (rc <= 0) {
						int error;
						error = SSL_get_error(client->ssl.ssl, rc);
						if (error == SSL_ERROR_WANT_READ) {
							client->ssl.want_read = 1;
						} else if (error == SSL_ERROR_WANT_WRITE) {
							client->ssl.want_write = 1;
						} else if (error == SSL_ERROR_SYSCALL) {
							client->ssl.want_read = 1;
						} else {
							char ebuf[256];
							mbus_errorf("can not connect ssl: %d", error);
							error = ERR_get_error();
							while (error) {
								mbus_errorf("  error: %d, %s", error, ERR_error_string(error, ebuf));
								error = ERR_get_error();
							}
							goto bail;
						}
					}
				}
#endif
				rc = mbus_client_command_create_request(client);
				if (rc != 0) {
					mbus_errorf("can not create create request");
					goto bail;
				}
			} else if (rc == -ECONNREFUSED) {
				mbus_errorf("can not connect to server: '%s:%s:%d', rc: %d, %s", client->options->server_protocol, client->options->server_address, client->options->server_port, rc, strerror(-rc));
				if (client->options->connect_interval > 0) {
					mbus_client_notify_connect(client, mbus_client_connect_status_connection_refused);
					mbus_client_reset(client);
					client->state = mbus_client_state_connecting;
					goto out;
				} else {
					mbus_client_notify_connect(client, mbus_client_connect_status_connection_refused);
					mbus_client_reset(client);
					client->state = mbus_client_state_disconnected;
					goto out;
				}
			} else {
				mbus_errorf("can not connect to server: '%s:%s:%d', rc: %d, %s", client->options->server_protocol, client->options->server_address, client->options->server_port, rc, strerror(-rc));
				mbus_client_notify_connect(client, mbus_client_connect_status_internal_error);
				goto bail;
			}
		} else if (mbus_buffer_length(client->outgoing) > 0) {
#if defined(SSL_ENABLE) && (SSL_ENABLE == 1)
			if (client->ssl.ssl == NULL) {
#endif
				write_rc = write(mbus_socket_get_fd(client->socket), mbus_buffer_base(client->outgoing), mbus_buffer_length(client->outgoing));
#if defined(SSL_ENABLE) && (SSL_ENABLE == 1)
			} else {
				client->ssl.want_write = 0;
				write_rc = SSL_write(client->ssl.ssl, mbus_buffer_base(client->outgoing), mbus_buffer_length(client->outgoing));
				if (write_rc <= 0) {
					int error;
					error = SSL_get_error(client->ssl.ssl, write_rc);
					if (error == SSL_ERROR_WANT_READ) {
						write_rc = 0;
						errno = EAGAIN;
						client->ssl.want_read = 1;
					} else if (error == SSL_ERROR_WANT_WRITE) {
						write_rc = 0;
						errno = EAGAIN;
						client->ssl.want_write = 1;
					} else if (error == SSL_ERROR_SYSCALL) {
						write_rc = -1;
						errno = EIO;
					} else {
						char ebuf[256];
						mbus_errorf("can not write ssl: %d", error);
						error = ERR_get_error();
						while (error) {
							mbus_errorf("  error: %d, %s", error, ERR_error_string(error, ebuf));
							error = ERR_get_error();
						}
						goto bail;
					}
				}
			}
#endif
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
				rc = mbus_client_handle_result(client, json);
				if (rc != 0) {
					mbus_errorf("can not handle message result");
					goto incoming_bail;
				}
			} else if (strcasecmp(type, MBUS_METHOD_TYPE_EVENT) == 0) {
				rc = mbus_client_handle_event(client, json);
				if (rc != 0) {
					mbus_errorf("can not handle message event");
					goto incoming_bail;
				}
			} else if (strcasecmp(type, MBUS_METHOD_TYPE_COMMAND) == 0) {
				rc = mbus_client_handle_command(client, json);
				if (rc != 0) {
					mbus_errorf("can not handle message command");
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
incoming_bail:		if (json != NULL) {
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

out:
	current = mbus_clock_get();
	if (client->state == mbus_client_state_connecting &&
	    client->socket != NULL &&
	    client->socket_connected == 0) {
		if (mbus_clock_after(current, client->connect_tsms + client->options->connect_timeout)) {
			if (client->options->connect_interval > 0) {
				mbus_client_notify_connect(client, mbus_client_connect_status_timeout);
				mbus_client_reset(client);
				client->state = mbus_client_state_connecting;
				goto out;
			} else {
				mbus_client_notify_connect(client, mbus_client_connect_status_timeout);
				mbus_client_reset(client);
				client->state = mbus_client_state_disconnected;
				goto out;
			}
		}
	}

	if (client->state == mbus_client_state_connected &&
	    client->ping_interval > 0) {
		if (mbus_clock_after(current, client->ping_send_tsms + client->ping_interval)) {
			mbus_debugf("send ping current: %ld, %ld, %d, %d", current, client->ping_send_tsms, client->ping_interval, client->ping_timeout);
			client->ping_send_tsms = current;
			client->pong_recv_tsms = 0;
			client->ping_wait_pong = 1;
			rc = mbus_client_publish_to_unlocked(client, MBUS_SERVER_IDENTIFIER, MBUS_SERVER_EVENT_PING, NULL);
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

	int i;
	struct requests *requests[2];

	requests[0] = &client->requests;
	requests[1] = &client->pendings;
	for (i = 0; i < (int) (sizeof(requests) / sizeof(requests[0])); i++) {
		TAILQ_FOREACH_SAFE(request, requests[i], requests, nrequest) {
			if (request_get_timeout(request) < 0 ||
			    mbus_clock_before(current, request_get_created_at(request) + request_get_timeout(request))) {
				continue;
			}
			mbus_debugf("request timeout to server: %s, %s", mbus_compress_method_string(client->compression), request_get_string(request));
			TAILQ_REMOVE(requests[i], request, requests);
			if (strcasecmp(request_get_type(request), MBUS_METHOD_TYPE_EVENT) == 0) {
				if (strcasecmp(MBUS_SERVER_IDENTIFIER, request_get_destination(request)) != 0 &&
				    strcasecmp(MBUS_SERVER_EVENT_PING, request_get_identifier(request)) != 0) {
					if (client->options->callbacks.publish != NULL) {
						struct mbus_client_message msg;
						msg.type = mbus_client_message_type_event;
						msg.u.event.payload = request_get_json(request);
						mbus_client_unlock(client);
						client->options->callbacks.publish(client, client->options->callbacks.context, &msg, mbus_client_publish_status_timeout);
						mbus_client_lock(client);
					}
				}
			} else if (strcasecmp(request_get_type(request), MBUS_METHOD_TYPE_COMMAND) == 0) {
				if (strcasecmp(request_get_identifier(request), MBUS_SERVER_COMMAND_EVENT) == 0) {
					if (client->options->callbacks.publish != NULL) {
						struct mbus_client_message msg;
						msg.type = mbus_client_message_type_event;
						msg.u.event.payload = request_get_payload(request);
						mbus_client_unlock(client);
						client->options->callbacks.publish(client, client->options->callbacks.context, &msg, mbus_client_publish_status_timeout);
						mbus_client_lock(client);
					}
				} else if (strcasecmp(request_get_identifier(request), MBUS_SERVER_COMMAND_SUBSCRIBE) == 0) {
					if (client->options->callbacks.subscribe != NULL) {
						mbus_client_unlock(client);
						client->options->callbacks.subscribe(client, client->options->callbacks.context,
								mbus_json_get_string_value(request_get_payload(request), "source", NULL),
								mbus_json_get_string_value(request_get_payload(request), "event", NULL),
								mbus_client_subscribe_status_timeout);
						mbus_client_lock(client);
					}
				} else if (strcasecmp(request_get_identifier(request), MBUS_SERVER_COMMAND_UNSUBSCRIBE) == 0) {
					if (client->options->callbacks.unsubscribe != NULL) {
						mbus_client_unlock(client);
						client->options->callbacks.unsubscribe(client, client->options->callbacks.context,
								mbus_json_get_string_value(request_get_payload(request), "source", NULL),
								mbus_json_get_string_value(request_get_payload(request), "event", NULL),
								mbus_client_unsubscribe_status_timeout);
						mbus_client_lock(client);
					}
				} else if (strcasecmp(request_get_identifier(request), MBUS_SERVER_COMMAND_REGISTER) == 0) {
					if (client->options->callbacks.registered != NULL) {
						mbus_client_unlock(client);
						client->options->callbacks.registered(client, client->options->callbacks.context,
								mbus_json_get_string_value(request_get_payload(request), "command", NULL),
								mbus_client_register_status_timeout);
						mbus_client_lock(client);
					}
				} else if (strcasecmp(request_get_identifier(request), MBUS_SERVER_COMMAND_UNREGISTER) == 0) {
					if (client->options->callbacks.unregistered != NULL) {
						mbus_client_unlock(client);
						client->options->callbacks.unregistered(client, client->options->callbacks.context,
								mbus_json_get_string_value(request_get_payload(request), "command", NULL),
								mbus_client_unregister_status_timeout);
						mbus_client_lock(client);
					}
				} else {
					if (request_get_callback(request) != NULL) {
						struct mbus_client_message message;
						message.type = mbus_client_message_type_command;
						message.u.command.request = request_get_json(request);
						message.u.command.response = NULL;
						mbus_client_unlock(client);
						request_get_callback(request)(client, request_get_context(request), &message, mbus_client_command_status_timeout);
						mbus_client_lock(client);
					}
				}
			}
			request_destroy(request);
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
			if (strcasecmp(MBUS_SERVER_IDENTIFIER, request_get_destination(request)) != 0 &&
			    strcasecmp(MBUS_SERVER_EVENT_PING, request_get_identifier(request)) != 0) {
				if (client->options->callbacks.publish != NULL) {
					struct mbus_client_message msg;
					mbus_client_unlock(client);
					msg.type = mbus_client_message_type_event;
					msg.u.event.payload = request_get_json(request);
					client->options->callbacks.publish(client, client->options->callbacks.context, &msg, mbus_client_publish_status_success);
					mbus_client_lock(client);
				}
			}
			request_destroy(request);
		} else {
			TAILQ_INSERT_TAIL(&client->pendings, request, requests);
		}
	}

	mbus_client_unlock(client);
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

const char * mbus_client_message_event_destination (struct mbus_client_message *message)
{
	if (message == NULL) {
		mbus_errorf("message is invalid");
		goto bail;
	}
	if (message->type != mbus_client_message_type_event) {
		mbus_errorf("message is invalid");
		goto bail;
	}
	return mbus_json_get_string_value(message->u.event.payload, "destination", NULL);
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

const char * mbus_client_message_command_request_destination (struct mbus_client_message *message)
{
	if (message == NULL) {
		mbus_errorf("message is invalid");
		goto bail;
	}
	if (message->type != mbus_client_message_type_command) {
		mbus_errorf("message is invalid");
		goto bail;
	}
	return mbus_json_get_string_value(message->u.command.request, "destination", NULL);
bail:	return NULL;
}

const char * mbus_client_message_command_request_identifier (struct mbus_client_message *message)
{
	if (message == NULL) {
		mbus_errorf("message is invalid");
		goto bail;
	}
	if (message->type != mbus_client_message_type_command) {
		mbus_errorf("message is invalid");
		goto bail;
	}
	return mbus_json_get_string_value(message->u.command.request, "identifier", NULL);
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

const char * mbus_client_message_routine_request_source (struct mbus_client_message *message)
{
	if (message == NULL) {
		mbus_errorf("message is invalid");
		goto bail;
	}
	if (message->type != mbus_client_message_type_routine) {
		mbus_errorf("message is invalid");
		goto bail;
	}
	return mbus_json_get_string_value(message->u.routine.request, "source", NULL);
bail:	return NULL;
}

const char * mbus_client_message_routine_request_identifier (struct mbus_client_message *message)
{
	if (message == NULL) {
		mbus_errorf("message is invalid");
		goto bail;
	}
	if (message->type != mbus_client_message_type_routine) {
		mbus_errorf("message is invalid");
		goto bail;
	}
	return mbus_json_get_string_value(message->u.command.request, "identifier", NULL);
bail:	return NULL;
}

const struct mbus_json * mbus_client_message_routine_request_payload (struct mbus_client_message *message)
{
	if (message == NULL) {
		mbus_errorf("message is invalid");
		goto bail;
	}
	if (message->type != mbus_client_message_type_routine) {
		mbus_errorf("message is invalid");
		goto bail;
	}
	return mbus_json_get_object(message->u.routine.request, "payload");
bail:	return NULL;
}

int mbus_client_message_routine_set_response_payload (struct mbus_client_message *message, const struct mbus_json *payload)
{
	if (message == NULL) {
		mbus_errorf("message is invalid");
		goto bail;
	}
	if (message->type != mbus_client_message_type_routine) {
		mbus_errorf("message is invalid");
		goto bail;
	}
	if (message->u.routine.response != NULL) {
		mbus_json_delete(message->u.routine.response);
		message->u.routine.response = NULL;
	}
	if (payload != NULL) {
		message->u.routine.response = mbus_json_duplicate(payload, 1);
		if (message->u.routine.response == NULL) {
			mbus_errorf("can not duplicate payload");
			goto bail;
		}
	}
	return 0;
bail:	return -1;
}

const char * mbus_client_state_string (enum mbus_client_state state)
{
	switch (state) {
		case mbus_client_state_unknown:					return "unknown";
		case mbus_client_state_connecting:				return "connecting";
		case mbus_client_state_connected:				return "connected";
		case mbus_client_state_disconnecting:				return "disconnecting";
		case mbus_client_state_disconnected:				return "disconnected";
	}
	return "unknown";
}

const char * mbus_client_connect_status_string (enum mbus_client_connect_status status)
{
	switch (status) {
		case mbus_client_connect_status_success:			return "success";
		case mbus_client_connect_status_internal_error:			return "internal error";
		case mbus_client_connect_status_invalid_protocol:		return "invalid protocol";
		case mbus_client_connect_status_connection_refused:		return "connection refused";
		case mbus_client_connect_status_server_unavailable:		return "server unavailable";
		case mbus_client_connect_status_timeout:			return "connection timeout";
		case mbus_client_connect_status_invalid_protocol_version:	return "invalid protocol version";
		case mbus_client_connect_status_invalid_client_identfier:	return "invalid client identifier";
		case mbus_client_connect_status_server_error:			return "server error";
	}
	return "internal error";
}

const char * mbus_client_disconnect_status_string (enum mbus_client_disconnect_status status)
{
	switch (status) {
		case mbus_client_disconnect_status_success:			return "success";
		case mbus_client_disconnect_status_internal_error:		return "internal error";
		case mbus_client_disconnect_status_connection_closed:		return "connection closed";
	}
	return "internal error";
}

const char * mbus_client_publish_status_string (enum mbus_client_publish_status status)
{
	switch (status) {
		case mbus_client_publish_status_success:			return "success";
		case mbus_client_publish_status_internal_error:			return "internal error";
		case mbus_client_publish_status_timeout:			return "timeout";
		case mbus_client_publish_status_canceled:			return "canceled";
	}
	return "internal error";
}

const char * mbus_client_subscribe_status_string (enum mbus_client_subscribe_status status)
{
	switch (status) {
		case mbus_client_subscribe_status_success:			return "success";
		case mbus_client_subscribe_status_internal_error:		return "internal error";
		case mbus_client_subscribe_status_timeout:			return "timeout";
		case mbus_client_subscribe_status_canceled:			return "canceled";
	}
	return "internal error";
}

const char * mbus_client_unsubscribe_status_string (enum mbus_client_unsubscribe_status status)
{
	switch (status) {
		case mbus_client_unsubscribe_status_success:			return "success";
		case mbus_client_unsubscribe_status_internal_error:		return "internal error";
		case mbus_client_unsubscribe_status_timeout:			return "timeout";
		case mbus_client_unsubscribe_status_canceled:			return "canceled";
	}
	return "internal error";
}

const char * mbus_client_register_status_string (enum mbus_client_register_status status)
{
	switch (status) {
		case mbus_client_register_status_success:			return "success";
		case mbus_client_register_status_internal_error:		return "internal error";
		case mbus_client_register_status_timeout:			return "timeout";
		case mbus_client_register_status_canceled:			return "canceled";
	}
	return "internal error";
}

const char * mbus_client_unregister_status_string (enum mbus_client_unregister_status status)
{
	switch (status) {
		case mbus_client_unregister_status_success:			return "success";
		case mbus_client_unregister_status_internal_error:		return "internal error";
		case mbus_client_unregister_status_timeout:			return "timeout";
		case mbus_client_unregister_status_canceled:			return "canceled";
	}
	return "internal error";
}

const char * mbus_client_command_status_string (enum mbus_client_command_status status)
{
	switch (status) {
		case mbus_client_command_status_success:			return "success";
		case mbus_client_command_status_internal_error:			return "internal error";
		case mbus_client_command_status_timeout:			return "timeout";
		case mbus_client_command_status_canceled:			return "canceled";
	}
	return "internal error";
}
