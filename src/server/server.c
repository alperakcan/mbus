
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
#include <signal.h>

#include <arpa/inet.h>

#if defined(SSL_ENABLE) && (SSL_ENABLE == 1)
#include <openssl/ssl.h>
#include <openssl/err.h>
#endif

#if defined(WS_ENABLE) && (WS_ENABLE == 1)
#include <libwebsockets.h>
#endif

#define MBUS_DEBUG_NAME	"mbus-server"

#include "mbus/debug.h"
#include "mbus/compress.h"
#include "mbus/buffer.h"
#include "mbus/clock.h"
#include "mbus/tailq.h"
#include "mbus/json.h"
#include "mbus/method.h"
#include "mbus/socket.h"
#include "mbus/version.h"
#include "server.h"

#define BUFFER_OUT_CHUNK_SIZE (64 * 1024)
#define BUFFER_IN_CHUNK_SIZE (64 * 1024)

#if defined(WS_ENABLE) && (WS_ENABLE == 1)
static __attribute__((__unused__)) char __sizeof_check_buffer_out[BUFFER_OUT_CHUNK_SIZE < LWS_PRE ? -1 : 0];
static __attribute__((__unused__)) char __sizeof_check_buffer_in[BUFFER_IN_CHUNK_SIZE < LWS_PRE ? -1 : 0];
#endif

struct method {
	TAILQ_ENTRY(method) methods;
	struct {
		struct mbus_json *json;
		char *string;
	} request;
	struct {
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

struct ws_client_data {
	struct lws *wsi;
	struct client *client;
};

enum listener_type {
	listener_type_unknown,
	listener_type_tcp,
	listener_type_uds,
#if defined(WS_ENABLE) && (WS_ENABLE == 1)
	listener_type_ws,
#endif
};

struct listener {
	TAILQ_ENTRY(listener) listeners;
	enum listener_type type;
	union {
		struct {
			struct mbus_socket *socket;
		} uds;
		struct {
			struct mbus_socket *socket;
		} tcp;
#if defined(WS_ENABLE) && (WS_ENABLE == 1)
		struct {
			struct lws_context *context;
			struct {
				unsigned int length;
				unsigned int size;
				struct pollfd *pollfds;
			} pollfds;
		} ws;
#endif
	} u;
#if defined(SSL_ENABLE) && (SSL_ENABLE == 1)
	struct {
		SSL_CTX *context;
	} ssl;
#endif
};
TAILQ_HEAD(listeners, listener);

enum client_status {
	client_status_accepted		= 0x00000001,
	client_status_connected		= 0x00000002,
};

static const struct {
	const char *name;
	enum mbus_compress_method value;
} compression_methods[] = {
#if defined(ZLIB_ENABLE) && (ZLIB_ENABLE == 1)
	{ "zlib", mbus_compress_method_zlib },
#endif
	{ "none", mbus_compress_method_none },
};

struct client {
	TAILQ_ENTRY(client) clients;
	char *name;
	enum client_status status;
	enum mbus_compress_method compression;
	enum listener_type type;
	struct mbus_socket *socket;
	struct {
		struct mbus_buffer *in;
		struct mbus_buffer *out;
		uint8_t out_chunk[BUFFER_OUT_CHUNK_SIZE];
	} buffer;
	struct {
		int enabled;
		int interval;
		int timeout;
		int threshold;
		unsigned long ping_recv_tsms;
		int ping_missed_count;
	} ping;
#if defined(SSL_ENABLE) && (SSL_ENABLE == 1)
	struct {
		SSL *ssl;
		int want_read;
		int want_write;
	} ssl;
#endif
	struct subscriptions subscriptions;
	struct commands commands;
	struct methods requests;
	struct methods results;
	struct methods events;
	struct methods waits;
	int ssequence;
	int esequence;
	int refcount;
};
TAILQ_HEAD(clients, client);

struct mbus_server {
	struct mbus_server_options options;
	struct listeners listeners;
	struct clients clients;
	struct methods methods;
	struct {
		unsigned int length;
		unsigned int size;
		struct pollfd *pollfds;
	} pollfds;
	int running;
};

static struct mbus_server *g_server;

#define OPTION_HELP				0x100

#define OPTION_DEBUG_LEVEL			0x101

#define OPTION_SERVER_TCP_ENABLE		0x201
#define OPTION_SERVER_TCP_ADDRESS		0x202
#define OPTION_SERVER_TCP_PORT			0x203

#define OPTION_SERVER_UDS_ENABLE		0x301
#define OPTION_SERVER_UDS_ADDRESS		0x302
#define OPTION_SERVER_UDS_PORT			0x303

#define OPTION_SERVER_WS_ENABLE			0x401
#define OPTION_SERVER_WS_ADDRESS		0x402
#define OPTION_SERVER_WS_PORT			0x403

#define OPTION_SERVER_TCPS_ENABLE		0x501
#define OPTION_SERVER_TCPS_ADDRESS		0x502
#define OPTION_SERVER_TCPS_PORT			0x503
#define OPTION_SERVER_TCPS_CERTIFICATE		0x504
#define OPTION_SERVER_TCPS_PRIVATEKEY		0x505

#define OPTION_SERVER_UDSS_ENABLE		0x601
#define OPTION_SERVER_UDSS_ADDRESS		0x602
#define OPTION_SERVER_UDSS_PORT			0x603
#define OPTION_SERVER_UDSS_CERTIFICATE		0x604
#define OPTION_SERVER_UDSS_PRIVATEKEY		0x605

#define OPTION_SERVER_WSS_ENABLE		0x701
#define OPTION_SERVER_WSS_ADDRESS		0x702
#define OPTION_SERVER_WSS_PORT			0x703
#define OPTION_SERVER_WSS_CERTIFICATE		0x704
#define OPTION_SERVER_WSS_PRIVATEKEY		0x705

static struct option longopts[] = {
	{ "mbus-help",				no_argument,		NULL,	OPTION_HELP },
	{ "mbus-debug-level",			required_argument,	NULL,	OPTION_DEBUG_LEVEL },

	{ "mbus-server-tcp-enable",		required_argument,	NULL,	OPTION_SERVER_TCP_ENABLE },
	{ "mbus-server-tcp-address",		required_argument,	NULL,	OPTION_SERVER_TCP_ADDRESS },
	{ "mbus-server-tcp-port",		required_argument,	NULL,	OPTION_SERVER_TCP_PORT },

	{ "mbus-server-uds-enable",		required_argument,	NULL,	OPTION_SERVER_UDS_ENABLE },
	{ "mbus-server-uds-address",		required_argument,	NULL,	OPTION_SERVER_UDS_ADDRESS },
	{ "mbus-server-uds-port",		required_argument,	NULL,	OPTION_SERVER_UDS_PORT },

#if defined(WS_ENABLE) && (WS_ENABLE == 1)
	{ "mbus-server-ws-enable",		required_argument,	NULL,	OPTION_SERVER_WS_ENABLE },
	{ "mbus-server-ws-address",		required_argument,	NULL,	OPTION_SERVER_WS_ADDRESS },
	{ "mbus-server-ws-port",		required_argument,	NULL,	OPTION_SERVER_WS_PORT },
#endif

#if defined(SSL_ENABLE) && (SSL_ENABLE == 1)
	{ "mbus-server-tcps-enable",		required_argument,	NULL,	OPTION_SERVER_TCPS_ENABLE },
	{ "mbus-server-tcps-address",		required_argument,	NULL,	OPTION_SERVER_TCPS_ADDRESS },
	{ "mbus-server-tcps-port",		required_argument,	NULL,	OPTION_SERVER_TCPS_PORT },
	{ "mbus-server-tcps-certificate",	required_argument,	NULL,	OPTION_SERVER_TCPS_CERTIFICATE },
	{ "mbus-server-tcps-privatekey",	required_argument,	NULL,	OPTION_SERVER_TCPS_PRIVATEKEY },

	{ "mbus-server-udss-enable",		required_argument,	NULL,	OPTION_SERVER_UDSS_ENABLE },
	{ "mbus-server-udss-address",		required_argument,	NULL,	OPTION_SERVER_UDSS_ADDRESS },
	{ "mbus-server-udss-port",		required_argument,	NULL,	OPTION_SERVER_UDSS_PORT },
	{ "mbus-server-udss-certificate",	required_argument,	NULL,	OPTION_SERVER_UDSS_CERTIFICATE },
	{ "mbus-server-udss-privatekey",	required_argument,	NULL,	OPTION_SERVER_UDSS_PRIVATEKEY },

#if defined(WS_ENABLE) && (WS_ENABLE == 1)
	{ "mbus-server-wss-enable",		required_argument,	NULL,	OPTION_SERVER_WSS_ENABLE },
	{ "mbus-server-wss-address",		required_argument,	NULL,	OPTION_SERVER_WSS_ADDRESS },
	{ "mbus-server-wss-port",		required_argument,	NULL,	OPTION_SERVER_WSS_PORT },
	{ "mbus-server-wss-certificate",	required_argument,	NULL,	OPTION_SERVER_WSS_CERTIFICATE },
	{ "mbus-server-wss-privatekey",		required_argument,	NULL,	OPTION_SERVER_WSS_PRIVATEKEY },
#endif
#endif

	{ NULL,					0,			NULL,	0 },
};

void mbus_server_usage (void)
{
	fprintf(stdout, "mbus server arguments:\n");
	fprintf(stdout, "  --mbus-debug-level             : debug level (default: %s)\n", mbus_debug_level_to_string(mbus_debug_level));

	fprintf(stdout, "  --mbus-server-tcp-enable       : server tcp enable (default: %d)\n", MBUS_SERVER_TCP_ENABLE);
	fprintf(stdout, "  --mbus-server-tcp-address      : server tcp address (default: %s)\n", MBUS_SERVER_TCP_ADDRESS);
	fprintf(stdout, "  --mbus-server-tcp-port         : server tcp port (default: %d)\n", MBUS_SERVER_TCP_PORT);

	fprintf(stdout, "  --mbus-server-uds-enable       : server uds enable (default: %d)\n", MBUS_SERVER_UDS_ENABLE);
	fprintf(stdout, "  --mbus-server-uds-address      : server uds address (default: %s)\n", MBUS_SERVER_UDS_ADDRESS);
	fprintf(stdout, "  --mbus-server-uds-port         : server uds port (default: %d)\n", MBUS_SERVER_UDS_PORT);

#if defined(WS_ENABLE) && (WS_ENABLE == 1)
	fprintf(stdout, "  --mbus-server-ws-enable        : server ws enable (default: %d)\n", MBUS_SERVER_WS_ENABLE);
	fprintf(stdout, "  --mbus-server-ws-address       : server ws address (default: %s)\n", MBUS_SERVER_WS_ADDRESS);
	fprintf(stdout, "  --mbus-server-ws-port          : server ws port (default: %d)\n", MBUS_SERVER_WS_PORT);
#endif

#if defined(SSL_ENABLE) && (SSL_ENABLE == 1)
	fprintf(stdout, "  --mbus-server-tcps-enable      : server tcps enable (default: %d)\n", MBUS_SERVER_TCPS_ENABLE);
	fprintf(stdout, "  --mbus-server-tcps-address     : server tcps address (default: %s)\n", MBUS_SERVER_TCPS_ADDRESS);
	fprintf(stdout, "  --mbus-server-tcps-port        : server tcps port (default: %d)\n", MBUS_SERVER_TCPS_PORT);
	fprintf(stdout, "  --mbus-server-tcps-certificate : server tcps certificate (default: %s)\n", MBUS_SERVER_TCPS_CERTIFICATE);
	fprintf(stdout, "  --mbus-server-tcps-privatekey  : server tcps privatekey (default: %s)\n", MBUS_SERVER_TCPS_PRIVATEKEY);

	fprintf(stdout, "  --mbus-server-udss-enable      : server udss enable (default: %d)\n", MBUS_SERVER_UDSS_ENABLE);
	fprintf(stdout, "  --mbus-server-udss-address     : server udss address (default: %s)\n", MBUS_SERVER_UDSS_ADDRESS);
	fprintf(stdout, "  --mbus-server-udss-port        : server udss port (default: %d)\n", MBUS_SERVER_UDSS_PORT);
	fprintf(stdout, "  --mbus-server-udss-certificate : server udss certificate (default: %s)\n", MBUS_SERVER_UDSS_CERTIFICATE);
	fprintf(stdout, "  --mbus-server-udss-privatekey  : server udss privatekey (default: %s)\n", MBUS_SERVER_UDSS_PRIVATEKEY);

#if defined(WS_ENABLE) && (WS_ENABLE == 1)
	fprintf(stdout, "  --mbus-server-wss-enable       : server ws enable (default: %d)\n", MBUS_SERVER_WSS_ENABLE);
	fprintf(stdout, "  --mbus-server-wss-address      : server ws address (default: %s)\n", MBUS_SERVER_WSS_ADDRESS);
	fprintf(stdout, "  --mbus-server-wss-port         : server ws port (default: %d)\n", MBUS_SERVER_WSS_PORT);
	fprintf(stdout, "  --mbus-server-wss-certificate  : server ws certificate (default: %s)\n", MBUS_SERVER_WSS_CERTIFICATE);
	fprintf(stdout, "  --mbus-server-wss-privatekey   : server ws privatekey (default: %s)\n", MBUS_SERVER_WSS_PRIVATEKEY);
#endif
#endif

	fprintf(stdout, "  --mbus-help                    : this text\n");
}

static void client_destroy (struct client *client);
static struct client * client_retain (struct client *client);

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

#if defined(WS_ENABLE) && (WS_ENABLE == 1)

static int ws_protocol_mbus_callback (struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len);

static struct lws_protocols ws_protocols[] = {
	{
		"mbus",
		ws_protocol_mbus_callback,
		sizeof(struct ws_client_data),
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

static const struct lws_extension ws_extensions[] = {
#if 0
	{
		"permessage-deflate",
		lws_extension_callback_pm_deflate,
		"permessage-deflate"
	},
#endif
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

#if defined(SSL_ENABLE) && (SSL_ENABLE == 1)

static struct lws_protocols wss_protocols[] = {
	{
		"mbus",
		ws_protocol_mbus_callback,
		sizeof(struct ws_client_data),
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

static const struct lws_extension wss_extensions[] = {
#if 0
	{
		"permessage-deflate",
		lws_extension_callback_pm_deflate,
		"permessage-deflate"
	},
#endif
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

#endif
#endif

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

static const char * method_get_request_type (struct method *method)
{
	if (method == NULL) {
		return NULL;
	}
	return mbus_json_get_string_value(method->request.json, "type", NULL);
}

static const char * method_get_request_destination (struct method *method)
{
	if (method == NULL) {
		return NULL;
	}
	return mbus_json_get_string_value(method->request.json, "destination", NULL);
}

static const char * method_get_request_identifier (struct method *method)
{
	if (method == NULL) {
		return NULL;
	}
	return mbus_json_get_string_value(method->request.json, "identifier", NULL);
}

static int method_get_request_sequence (struct method *method)
{
	if (method == NULL) {
		return -1;
	}
	return mbus_json_get_int_value(method->request.json, "sequence", -1);
}

static struct mbus_json * method_get_request_payload (struct method *method)
{
	if (method == NULL) {
		return NULL;
	}
	return mbus_json_get_object(method->request.json, "payload");
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

static int method_set_result_code (struct method *method, int code)
{
	if (method == NULL) {
		return -1;
	}
	mbus_json_add_number_to_object_cs(method->result.json, "result", code);
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
	if (method->source != NULL) {
		client_destroy(method->source);
	}
	free(method);
}

static struct method * method_create_request (struct client *source, const char *string)
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
	method->request.json = mbus_json_parse(string);
	if (method->request.json == NULL) {
		mbus_errorf("can not parse method");
		goto bail;
	}
	if (mbus_json_get_string_value(method->request.json, "type", NULL) == NULL) {
		mbus_errorf("invalid method type: '%s'", string);
		goto bail;
	}
	if (mbus_json_get_string_value(method->request.json, "destination", NULL) == NULL) {
		mbus_errorf("invalid method destination: '%s'", string);
		goto bail;
	}
	if (mbus_json_get_string_value(method->request.json, "identifier", NULL) == NULL) {
		mbus_errorf("invalid method identifier: '%s'", string);
		goto bail;
	}
	if (mbus_json_get_int_value(method->request.json, "sequence", -1) == -1) {
		mbus_errorf("invalid method sequence: '%s'", string);
		goto bail;
	}
	if (mbus_json_get_object(method->request.json, "payload") == NULL) {
		mbus_errorf("invalid method payload: '%s'", string);
		goto bail;
	}
	method->result.json = mbus_json_create_object();
	if (method->result.json == NULL) {
		mbus_errorf("can not create object");
		goto bail;
	}
	mbus_json_add_string_to_object_cs(method->result.json, "type", MBUS_METHOD_TYPE_RESULT);
	mbus_json_add_number_to_object_cs(method->result.json, "sequence", method_get_request_sequence(method));
	mbus_json_add_item_to_object_cs(method->result.json, "payload", mbus_json_create_object());
	method->source = client_retain(source);
	return method;
bail:	method_destroy(method);
	return NULL;
}

static struct method * method_create_response (const char *type, const char *source, const char *identifier, int sequence, const struct mbus_json *payload)
{
	struct method *method;
	struct mbus_json *data;
	method = NULL;
	if (type == NULL) {
		mbus_errorf("type is null");
		goto bail;
	}
	if (source == NULL) {
		mbus_errorf("source is null");
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
		data = mbus_json_create_object();
	} else {
		data = mbus_json_duplicate(payload, 1);
	}
	if (data == NULL) {
		mbus_errorf("can not create payload");
		goto bail;
	}
	method = malloc(sizeof(struct method));
	if (method == NULL) {
		mbus_errorf("can not allocate memory");
		goto bail;
	}
	memset(method, 0, sizeof(struct method));
	method->request.json = mbus_json_create_object();
	if (method->request.json == NULL) {
		mbus_errorf("can not create method object");
		goto bail;
	}
	mbus_json_add_string_to_object_cs(method->request.json, "type", type);
	mbus_json_add_string_to_object_cs(method->request.json, "source", source);
	mbus_json_add_string_to_object_cs(method->request.json, "identifier", identifier);
	mbus_json_add_number_to_object_cs(method->request.json, "sequence", sequence);
	mbus_json_add_item_to_object_cs(method->request.json, "payload", data);
	return method;
bail:	if (method != NULL) {
		method_destroy(method);
	}
	return NULL;
}

static enum listener_type listener_get_type (struct listener *listener)
{
	if (listener == NULL) {
		mbus_errorf("listener is null");
		return listener_type_unknown;
	}
	return listener->type;
}

static void listener_destroy (struct listener *listener)
{
	if (listener == NULL) {
		return;
	}
	if (listener->type == listener_type_tcp) {
		if (listener->u.tcp.socket != NULL) {
			mbus_socket_destroy(listener->u.tcp.socket);
		}
	}
	if (listener->type == listener_type_uds) {
		if (listener->u.uds.socket != NULL) {
			mbus_socket_destroy(listener->u.uds.socket);
		}
	}
#if defined(WS_ENABLE) && (WS_ENABLE == 1)
	if (listener->type == listener_type_ws) {
		if (listener->u.ws.context != NULL) {
			lws_context_destroy(listener->u.ws.context);
		}
		if (listener->u.ws.pollfds.pollfds != NULL) {
			free(listener->u.ws.pollfds.pollfds);
		}
	}
#endif
#if defined(SSL_ENABLE) && (SSL_ENABLE == 1)
	if (listener->ssl.context != NULL) {
		SSL_CTX_free(listener->ssl.context);
	}
#endif
	free(listener);
}

static struct listener * listener_create (enum listener_type type, const char *address, int port, const char *certificate, const char *privatekey)
{
	int rc;
	struct listener *listener;
	listener = NULL;
	if (certificate != NULL ||
	    privatekey != NULL) {
		if (certificate == NULL) {
			mbus_errorf("ssl certificate is invalid");
			goto bail;
		}
		if (privatekey == NULL) {
			mbus_errorf("ssl privatekey is invalid");
			goto bail;
		}
	}
	listener = malloc(sizeof(struct listener));
	if (listener == NULL) {
		mbus_errorf("can not allocate memory");
		goto bail;
	}
	memset(listener, 0, sizeof(struct listener));
	listener->type = type;
	if (type == listener_type_tcp) {
		listener->u.tcp.socket = mbus_socket_create(mbus_socket_domain_af_inet, mbus_socket_type_sock_stream, mbus_socket_protocol_any);
		if (listener->u.tcp.socket == NULL) {
			mbus_errorf("can not create socket: '%s:%s:%d'", "tcp", address, port);
			goto bail;
		}
		rc = mbus_socket_set_reuseaddr(listener->u.tcp.socket, 1);
		if (rc != 0) {
			mbus_errorf("can not reuse socket: '%s:%s:%d'", "tcp", address, port);
			goto bail;
		}
		mbus_socket_set_keepalive(listener->u.tcp.socket, 1);
#if 0
		mbus_socket_set_keepcnt(listener->u.tcp.socket, 5);
		mbus_socket_set_keepidle(listener->u.tcp.socket, 180);
		mbus_socket_set_keepintvl(listener->u.tcp.socket, 60);
#endif
		rc = mbus_socket_bind(listener->u.tcp.socket, address, port);
		if (rc != 0) {
			mbus_errorf("can not bind socket: '%s:%s:%d'", "tcp", address, port);
			goto bail;
		}
		rc = mbus_socket_listen(listener->u.tcp.socket, 1024);
		if (rc != 0) {
			mbus_errorf("can not listen socket: '%s:%s:%d'", "tcp", address, port);
			goto bail;
		}
	} else if (type == listener_type_uds) {
		listener->u.uds.socket = mbus_socket_create(mbus_socket_domain_af_unix, mbus_socket_type_sock_stream, mbus_socket_protocol_any);
		if (listener->u.uds.socket == NULL) {
			mbus_errorf("can not create socket");
			goto bail;
		}
		rc = mbus_socket_set_reuseaddr(listener->u.uds.socket, 1);
		if (rc != 0) {
			mbus_errorf("can not reuse socket");
			goto bail;
		}
		rc = mbus_socket_bind(listener->u.uds.socket, address, port);
		if (rc != 0) {
			mbus_errorf("can not bind socket: '%s:%s:%d'", "uds", address, port);
			goto bail;
		}
		rc = mbus_socket_listen(listener->u.uds.socket, 1024);
		if (rc != 0) {
			mbus_errorf("can not listen socket");
			goto bail;
		}
#if defined(WS_ENABLE) && (WS_ENABLE == 1)
	} else if (type == listener_type_ws &&
		   certificate == NULL &&
		   privatekey == NULL) {
		struct lws_context_creation_info info;
		ws_protocols[0].user = listener;
		memset(&info, 0, sizeof(info));
		info.iface = NULL;
		info.port = port;
		info.protocols = ws_protocols;
		info.extensions = ws_extensions;
		info.gid = -1;
		info.uid = -1;
		listener->u.ws.context = lws_create_context(&info);
		if (listener->u.ws.context == NULL) {
			mbus_errorf("can not create ws context for: '%s:%s:%d'", "ws", address, port);
			goto bail;
		}
#if defined(SSL_ENABLE) && (SSL_ENABLE == 1)
	} else if (type == listener_type_ws &&
		   certificate != NULL &&
		   privatekey != NULL) {
		struct lws_context_creation_info info;
		wss_protocols[0].user = listener;
		memset(&info, 0, sizeof(info));
		info.iface = NULL;
		info.port = port;
		info.protocols = wss_protocols;
		info.extensions = wss_extensions;
		info.gid = -1;
		info.uid = -1;
		info.ssl_ca_filepath = NULL; //"ca.crt";
		info.ssl_cert_filepath = certificate;
		info.ssl_private_key_filepath = privatekey;
//		info.options |= LWS_SERVER_OPTION_REQUIRE_VALID_OPENSSL_CLIENT_CERT;
		info.options |= LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
		listener->u.ws.context = lws_create_context(&info);
		if (listener->u.ws.context == NULL) {
			mbus_errorf("can not create ws context for: '%s:%s:%d'", "wss", address, port);
			goto bail;
		}
#endif
#endif
	} else {
		mbus_errorf("unknown type: %d", type);
		goto bail;
	}
#if defined(SSL_ENABLE) && (SSL_ENABLE == 1)
	if (certificate != NULL ||
	    privatekey != NULL) {
		const SSL_METHOD *method;
		method = SSLv23_server_method();
		if (method == NULL) {
			mbus_errorf("can not get server method");
			goto bail;
		}
		listener->ssl.context = SSL_CTX_new(method);
		if (listener->ssl.context == NULL) {
			mbus_errorf("can not create ssl context");
			goto bail;
		}
#if (OPENSSL_VERSION_NUMBER >= 0x10100000L)
		SSL_CTX_set_ecdh_auto(listener->ssl.context, 1);
#endif
		rc = SSL_CTX_use_certificate_file(listener->ssl.context, certificate, SSL_FILETYPE_PEM);
		if (rc <= 0) {
			mbus_errorf("can not use ssl certificate: %s", certificate);
			goto bail;
		}
		rc = SSL_CTX_use_PrivateKey_file(listener->ssl.context, privatekey, SSL_FILETYPE_PEM);
		if (rc <= 0) {
			mbus_errorf("can not use ssl privatekey: %s", privatekey);
			goto bail;
		}
	}
#endif
	return listener;
bail:	if (listener != NULL) {
		listener_destroy(listener);
	}
	return NULL;
}

static struct mbus_socket * client_get_socket (struct client *client)
{
	if (client == NULL) {
		mbus_errorf("client is null");
		return NULL;
	}
	return client->socket;
}

static int client_set_socket (struct client *client, struct mbus_socket *socket)
{
	if (client == NULL) {
		mbus_errorf("client is null");
		return -1;
	}
	if (socket == NULL) {
		if (client->socket != NULL) {
			if (client->type == listener_type_tcp) {
				mbus_socket_shutdown(client->socket, mbus_socket_shutdown_rdwr);
				mbus_socket_destroy(client->socket);
				client->socket = NULL;
			}
			if (client->type == listener_type_uds) {
				mbus_socket_shutdown(client->socket, mbus_socket_shutdown_rdwr);
				mbus_socket_destroy(client->socket);
				client->socket = NULL;
			}
#if defined(WS_ENABLE) && (WS_ENABLE == 1)
			if (client->type == listener_type_ws) {
				struct ws_client_data *data;
				data = (struct ws_client_data *) client_get_socket(client);
				data->client = NULL;
				lws_callback_on_writable(data->wsi);
			}
#endif
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

static enum listener_type client_get_listener_type (struct client *client)
{
	if (client == NULL) {
		mbus_errorf("client is null");
		return listener_type_unknown;
	}
	return client->type;
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

static int client_set_compression (struct client *client, enum mbus_compress_method compression)
{
	if (client == NULL) {
		mbus_errorf("client is null");
		return -1;
	}
	client->compression = compression;
	return 0;
}

static enum mbus_compress_method client_get_compression (struct client *client)
{
	if (client == NULL) {
		mbus_errorf("client is null");
		return 0;
	}
	return client->compression;
}

static int client_set_name (struct client *client, const char *name)
{
	if (client == NULL) {
		mbus_errorf("client is null");
		goto bail;
	}
	if (name == NULL) {
		mbus_errorf("name is null");
		goto bail;
	}
	if (client->name != NULL) {
		free(client->name);
	}
	client->name = strdup(name);
	if (client->name == NULL) {
		mbus_errorf("can not allocate memory");
		goto bail;
	}
	return 0;
bail:	return -1;
}

static const char * client_get_name (struct client *client)
{
	if (client == NULL) {
		mbus_errorf("client is null");
		return NULL;
	}
	return client->name;
}

static int client_del_subscription (struct client *client, const char *source, const char *event)
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
			break;
		}
	}
	if (subscription != NULL) {
		TAILQ_REMOVE(&client->subscriptions, subscription, subscriptions);
		subscription_destroy(subscription);
		mbus_infof("unsubscribed '%s' from '%s', '%s'", client_get_name(client), source, event);
		return 0;
	}
bail:	return -1;
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

static int client_del_command (struct client *client, const char *identifier)
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
			break;
		}
	}
	TAILQ_REMOVE(&client->commands, command, commands);
	command_destroy(command);
	mbus_infof("unregistered '%s' '%s'", client_get_name(client), identifier);
	return 0;
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

static int client_push_wait (struct client *client, struct method *wait)
{
	if (client == NULL) {
		mbus_errorf("client is null");
		goto bail;
	}
	if (wait == NULL) {
		mbus_errorf("wait is null");
		goto bail;
	}
	TAILQ_INSERT_TAIL(&client->waits, wait, methods);
	return 0;
bail:	return -1;
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
	if (--client->refcount > 0) {
		return;
	}
	if (client->socket != NULL) {
		if (client_get_listener_type(client) == listener_type_tcp) {
			mbus_socket_destroy(client->socket);
		}
		if (client_get_listener_type(client) == listener_type_uds) {
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
	if (client->buffer.in != NULL) {
		mbus_buffer_destroy(client->buffer.in);
	}
	if (client->buffer.out != NULL) {
		mbus_buffer_destroy(client->buffer.out);
	}
#if defined(SSL_ENABLE) && (SSL_ENABLE == 1)
	if (client->ssl.ssl != NULL) {
		SSL_free(client->ssl.ssl);
	}
#endif
	free(client);
}

static struct client * client_retain (struct client *client)
{
	if (client == NULL) {
		return NULL;
	}
	client->refcount += 1;
	return client;
}

static struct client * client_accept (enum listener_type type)
{
	struct client *client;
	client = NULL;
	client = malloc(sizeof(struct client));
	if (client == NULL) {
		mbus_errorf("can not allocate memory");
		goto bail;
	}
	memset(client, 0, sizeof(struct client));
	client->refcount = 1;
	TAILQ_INIT(&client->subscriptions);
	TAILQ_INIT(&client->commands);
	TAILQ_INIT(&client->requests);
	TAILQ_INIT(&client->results);
	TAILQ_INIT(&client->events);
	TAILQ_INIT(&client->waits);
	client->status = 0;
	client->type = type;
	client->ssequence = MBUS_METHOD_SEQUENCE_START;
	client->esequence = MBUS_METHOD_SEQUENCE_START;
	client->buffer.in = mbus_buffer_create();
	if (client->buffer.in == NULL) {
		mbus_errorf("can not create buffer");
		goto bail;
	}
	client->buffer.out = mbus_buffer_create();
	if (client->buffer.out == NULL) {
		mbus_errorf("can not create buffer");
		goto bail;
	}
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
		if (client_get_name(client) == NULL) {
			continue;
		}
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
		if (client_get_socket(client) == NULL) {
			continue;
		}
#if defined(WS_ENABLE) && (WS_ENABLE == 1)
		if (client_get_listener_type(client) == listener_type_ws) {
			struct ws_client_data *data;
			data = (struct ws_client_data *) client_get_socket(client);
			if (lws_get_socket_fd(data->wsi) == fd) {
				return client;
			}
		}
#endif
		if (client_get_listener_type(client) == listener_type_uds) {
			if (mbus_socket_get_fd(client_get_socket(client)) == fd) {
				return client;
			}
		}
		if (client_get_listener_type(client) == listener_type_tcp) {
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
#if 0
	if (payload == NULL) {
		mbus_errorf("payload is null");
		goto bail;
	}
#endif
	if (strcmp(destination, MBUS_SERVER_NAME) == 0) {
		if (strcmp(identifier, MBUS_SERVER_EVENT_PING) == 0) {
			client = server_find_client_by_name(server, source);
			if (client != NULL) {
				client->ping.ping_recv_tsms = mbus_clock_get();
			}
			rc = server_send_event_to(server, MBUS_SERVER_NAME, source, MBUS_SERVER_EVENT_PONG, NULL);
			if (rc != 0) {
				mbus_errorf("can not send pong to: %s", source);
				goto bail;
			}
		}
	} else if (strcmp(destination, MBUS_METHOD_EVENT_DESTINATION_ALL) == 0) {
		TAILQ_FOREACH(client, &server->clients, clients) {
			if (client_get_name(client) == NULL) {
				continue;
			}
			if (strcmp(client_get_name(client), source) == 0) {
				continue;
			}
			method = method_create_response(MBUS_METHOD_TYPE_EVENT, source, identifier, client->esequence, payload);
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
				method = method_create_response(MBUS_METHOD_TYPE_EVENT, source, identifier, client->esequence, payload);
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
			if (client_get_name(client) == NULL) {
				continue;
			}
			if (strcmp(client_get_name(client), destination) != 0) {
				continue;
			}
			method = method_create_response(MBUS_METHOD_TYPE_EVENT, source, identifier, client->esequence, payload);
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
	mbus_json_add_string_to_object_cs(payload, "source", source);
	mbus_json_add_string_to_object_cs(payload, "destination", destination);
	mbus_json_add_string_to_object_cs(payload, "identifier", identifier);
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

static int server_send_event_unsubscribed (struct mbus_server *server, const char *source, const char *destination, const char *identifier)
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
	mbus_json_add_string_to_object_cs(payload, "destination", destination);
	mbus_json_add_string_to_object_cs(payload, "identifier", identifier);
	rc = server_send_event_to(server, MBUS_SERVER_NAME, MBUS_METHOD_EVENT_DESTINATION_SUBSCRIBERS, MBUS_SERVER_EVENT_UNSUBSCRIBED, payload);
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

static int server_accept_client (struct mbus_server *server, struct listener *listener, struct mbus_socket *from)
{
	int rc;
	struct client *client;
	struct mbus_socket *socket;
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
	rc = mbus_socket_set_blocking(socket, 0);
	if (rc != 0) {
		mbus_errorf("can not set socket to nonblocking");
		goto bail;
	}
	client = client_accept(listener_get_type(listener));
	if (client == NULL) {
		mbus_errorf("can not create client");
		goto bail;
	}
	rc = client_set_socket(client, socket);
	if (rc != 0) {
		mbus_errorf("can not set client socket");
		goto bail;
	}
#if defined(SSL_ENABLE) && (SSL_ENABLE == 1)
	if (listener->ssl.context != NULL) {
		client->ssl.ssl = SSL_new(listener->ssl.context);
		if (client->ssl.ssl == NULL) {
			mbus_errorf("can not create ssl");
			goto bail;
		}
		SSL_set_fd(client->ssl.ssl, mbus_socket_get_fd(socket));
		rc = SSL_accept(client->ssl.ssl);
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
				mbus_errorf("can not accept ssl: %d", error);
				error = ERR_get_error();
				while (error) {
					mbus_errorf("  error: %d, %s", error, ERR_error_string(error, ebuf));
					error = ERR_get_error();
				}
				goto reject;
			}
		}
	}
#endif
	TAILQ_INSERT_TAIL(&server->clients, client, clients);
	return 0;
bail:	if (socket != NULL) {
		if (client == NULL ||
		    client_get_socket(client) != socket) {
			mbus_socket_destroy(socket);
		}
	}
	if (client != NULL) {
		client_destroy(client);
	}
	return -2;
#if defined(SSL_ENABLE) && (SSL_ENABLE == 1)
reject:	if (socket != NULL) {
		if (client == NULL ||
		    client_get_socket(client) != socket) {
			mbus_socket_destroy(socket);
		}
	}
	if (client != NULL) {
		client_destroy(client);
	}
	return -2;
#endif
}

static int server_handle_command_create (struct mbus_server *server, struct method *method)
{
	int rc;
	char rname[64];
	const char *name;
	struct client *client;
	if (server == NULL) {
		mbus_errorf("server is null");
		goto bail;
	}
	if (method == NULL) {
		mbus_errorf("method is null");
		goto bail;
	}
	name = client_get_name(method_get_source(method));
	if (name != NULL) {
		mbus_errorf("invalid client state");
		goto bail;
	}
	{
		struct mbus_json *payload;
		payload = method_get_request_payload(method);
		{
			name = mbus_json_get_string_value(payload, "name", NULL);
			if (name == NULL ||
			    strlen(name) == 0) {
				mbus_infof("empty name, creating a random name for client");
				while (1) {
					snprintf(rname, sizeof(rname), "org.mbus.client-%08x", rand());
					client = server_find_client_by_name(server, rname);
					if (client == NULL) {
						break;
					}
				}
				name = rname;
			}
			client = server_find_client_by_name(server, name);
			if (client != NULL) {
				mbus_errorf("client with name: %s already exists", name);
				goto bail;
			}
			rc = client_set_name(method_get_source(method), name);
			if (rc != 0) {
				mbus_errorf("can not set client name");
				goto bail;
			}
			client = server_find_client_by_name(server, name);
			if (client == NULL) {
				mbus_errorf("client name logic error");
				goto bail;
			}
		}
		{
			struct mbus_json *ping;
			ping = mbus_json_get_object(payload, "ping");
			client->ping.interval = mbus_json_get_int_value(ping, "interval", -1);
			client->ping.timeout = mbus_json_get_int_value(ping, "timeout", -1);
			client->ping.threshold = mbus_json_get_int_value(ping, "threshold", -1);
			if (client->ping.interval <= 0) {
				client->ping.interval = 0;
			}
			if (client->ping.timeout > (client->ping.interval * 2) / 3) {
				client->ping.timeout = (client->ping.interval * 2) / 3;
			}
			if (client->ping.threshold <= 0) {
				client->ping.threshold = 0;
			}
			if (client->ping.interval > 0) {
				client->ping.enabled = 1;
			}
			client->ping.ping_recv_tsms = mbus_clock_get() - client->ping.interval;
		}
		{
			int i;
			int j;
			struct mbus_json *compression;
			compression = mbus_json_get_object(payload, "compression");
			for (i = 0; i < (int) (sizeof(compression_methods) / sizeof(compression_methods[0])); i++) {
				for (j = 0; j < mbus_json_get_array_size(compression); j++) {
					if (mbus_json_get_value_string(mbus_json_get_array_item(compression, j)) == NULL) {
						continue;
					}
					if (strcmp(compression_methods[i].name, mbus_json_get_value_string(mbus_json_get_array_item(compression, j))) == 0) {
						break;
					}
				}
				if (j < mbus_json_get_array_size(compression)) {
					break;
				}
			}
			if (i < (int) (sizeof(compression_methods) / sizeof(compression_methods[0]))) {
				mbus_debugf("compression: %s", compression_methods[i].name);
				client_set_compression(method_get_source(method), compression_methods[i].value);
			} else {
				mbus_debugf("compression: %s", "none");
				client_set_compression(method_get_source(method), mbus_compress_method_none);
			}
		}
	}
	mbus_infof("client created");
	mbus_infof("  name       : %s", client_get_name(method_get_source(method)));
	mbus_infof("  compression: %s", mbus_compress_method_string(client_get_compression(method_get_source(method))));
	mbus_infof("  ping");
	mbus_infof("    enabled  : %d", client->ping.enabled);
	mbus_infof("    interval : %d", client->ping.interval);
	mbus_infof("    timeout  : %d", client->ping.timeout);
	mbus_infof("    threshold: %d", client->ping.threshold);
	{
		struct mbus_json *payload;
		struct mbus_json *ping;
		payload = mbus_json_create_object();
		mbus_json_add_string_to_object_cs(payload, "name", client_get_name(method_get_source(method)));
		mbus_json_add_string_to_object_cs(payload, "compression", mbus_compress_method_string(client_get_compression(method_get_source(method))));
		ping = mbus_json_create_object();
		mbus_json_add_number_to_object_cs(ping, "interval", client->ping.interval);
		mbus_json_add_number_to_object_cs(ping, "timeout", client->ping.timeout);
		mbus_json_add_number_to_object_cs(ping, "threshold", client->ping.threshold);
		mbus_json_add_item_to_object_cs(payload, "ping", ping);
		method_set_result_payload(method, payload);
	}
	return 0;
bail:	return -1;
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
	source = mbus_json_get_string_value(method_get_request_payload(method), "source", NULL);
	event = mbus_json_get_string_value(method_get_request_payload(method), "event", NULL);
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
		mbus_errorf("can not send subscribed event");
		goto bail;
	}
	return 0;
bail:	return -1;
}

static int server_handle_command_unsubscribe (struct mbus_server *server, struct method *method)
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
	source = mbus_json_get_string_value(method_get_request_payload(method), "source", NULL);
	event = mbus_json_get_string_value(method_get_request_payload(method), "event", NULL);
	if ((source == NULL) ||
	    (event == NULL)) {
		mbus_errorf("invalid request");
		goto bail;
	}
	rc = client_del_subscription(method_get_source(method), source, event);
	if (rc != 0) {
		mbus_errorf("can not del subscription");
		goto bail;
	}
	rc = server_send_event_unsubscribed(server, client_get_name(method_get_source(method)), source, event);
	if (rc != 0) {
		mbus_errorf("can not send unsubscribed event");
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
	command = mbus_json_get_string_value(method_get_request_payload(method), "command", NULL);
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

static int server_handle_command_unregister (struct mbus_server *server, struct method *method)
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
	command = mbus_json_get_string_value(method_get_request_payload(method), "command", NULL);
	if (command == NULL) {
		mbus_errorf("invalid request");
		goto bail;
	}
	rc = client_del_command(method_get_source(method), command);
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
	struct mbus_json *payload;
	if (server == NULL) {
		mbus_errorf("server is null");
		goto bail;
	}
	if (method == NULL) {
		mbus_errorf("method is null");
		goto bail;
	}
	destination = mbus_json_get_string_value(method_get_request_payload(method), "destination", NULL);
	identifier = mbus_json_get_string_value(method_get_request_payload(method), "identifier", NULL);
	payload = mbus_json_get_object(method_get_request_payload(method), "payload");
	if ((destination == NULL) ||
	    (identifier == NULL) ||
	    (payload == NULL)) {
		mbus_errorf("invalid request");
		goto bail;
	}
	rc = server_send_event_to(server, client_get_name(method_get_source(method)), destination, identifier, payload);
	if (rc != 0) {
		mbus_errorf("can not send event");
	}
	return 0;
bail:	return -1;
}

static int server_handle_command_status (struct mbus_server *server, struct method *method)
{
	char address[1024];
	struct mbus_json *object;
	struct mbus_json *source;
	struct mbus_json *clients;
	struct mbus_json *commands;
	struct mbus_json *subscribes;
	struct client *client;
	struct command *command;
	struct subscription *subscription;
	clients = NULL;
	if (server == NULL) {
		mbus_errorf("server is null");
		goto bail;
	}
	if (method == NULL) {
		mbus_errorf("method is null");
		goto bail;
	}
	clients = mbus_json_create_array();
	if (clients == NULL) {
		goto bail;
	}
	TAILQ_FOREACH(client, &server->clients, clients) {
		source = mbus_json_create_object();
		if (source == NULL) {
			goto bail;
		}
		mbus_json_add_item_to_array(clients, source);
		mbus_json_add_string_to_object_cs(source, "source", client_get_name(client));
		if (client_get_listener_type(client) == listener_type_tcp) {
			mbus_json_add_string_to_object_cs(source, "address", mbus_socket_get_address(client_get_socket(client), address, 1024));
		}
#if defined(WS_ENABLE) && (WS_ENABLE == 1)
		if (client_get_listener_type(client) == listener_type_ws) {
			struct ws_client_data *data;
			data = (struct ws_client_data *) client_get_socket(client);
			mbus_json_add_string_to_object_cs(source, "address", mbus_socket_fd_get_address(lws_get_socket_fd(data->wsi), address, 1024));
		}
#endif
		subscribes = mbus_json_create_array();
		if (subscribes == NULL) {
			goto bail;
		}
		mbus_json_add_item_to_object_cs(source, "subscriptions", subscribes);
		TAILQ_FOREACH(subscription, &client->subscriptions, subscriptions) {
			object = mbus_json_create_object();
			if (object == NULL) {
				goto bail;
			}
			mbus_json_add_item_to_array(subscribes, object);
			mbus_json_add_string_to_object_cs(object, "source", subscription_get_source(subscription));
			mbus_json_add_string_to_object_cs(object, "identifier", subscription_get_event(subscription));
		}
		commands = mbus_json_create_array();
		if (commands == NULL) {
			goto bail;
		}
		mbus_json_add_item_to_object_cs(source, "commands", commands);
		TAILQ_FOREACH(command, &client->commands, commands) {
			object = mbus_json_create_object();
			if (object == NULL) {
				goto bail;
			}
			mbus_json_add_item_to_array(commands, object);
			mbus_json_add_string_to_object_cs(object, "identifier", command_get_identifier(command));
		}
	}
	method_set_result_payload(method, clients);
	return 0;
bail:	if (clients != NULL) {
		mbus_json_delete(clients);
	}
	return -1;
}

static int server_handle_command_client (struct mbus_server *server, struct method *method)
{
	char address[1024];
	struct mbus_json *result;
	struct mbus_json *object;
	struct mbus_json *commands;
	struct mbus_json *subscribes;
	struct client *client;
	struct command *command;
	struct subscription *subscription;
	const char *source;
	result = NULL;
	if (server == NULL) {
		mbus_errorf("server is null");
		goto bail;
	}
	if (method == NULL) {
		mbus_errorf("method is null");
		goto bail;
	}
	source = mbus_json_get_string_value(method_get_request_payload(method), "source", NULL);
	if (source == NULL) {
		mbus_errorf("method request source is null");
		goto bail;
	}
	client = server_find_client_by_name(server, source);
	if (client == NULL) {
		mbus_errorf("client: %s is not connected", source);
		goto bail;
	}
	result = mbus_json_create_object();
	if (result == NULL) {
		goto bail;
	}
	mbus_json_add_string_to_object_cs(result, "source", client_get_name(client));
	if (client_get_listener_type(client) == listener_type_tcp) {
		mbus_json_add_string_to_object_cs(result, "address", mbus_socket_get_address(client_get_socket(client), address, 1024));
	}
	subscribes = mbus_json_create_array();
	if (subscribes == NULL) {
		goto bail;
	}
	mbus_json_add_item_to_object_cs(result, "subscriptions", subscribes);
	TAILQ_FOREACH(subscription, &client->subscriptions, subscriptions) {
		object = mbus_json_create_object();
		if (object == NULL) {
			goto bail;
		}
		mbus_json_add_item_to_array(subscribes, object);
		mbus_json_add_string_to_object_cs(object, "source", subscription_get_source(subscription));
		mbus_json_add_string_to_object_cs(object, "identifier", subscription_get_event(subscription));
	}
	commands = mbus_json_create_array();
	if (commands == NULL) {
		goto bail;
	}
	mbus_json_add_item_to_object_cs(result, "commands", commands);
	TAILQ_FOREACH(command, &client->commands, commands) {
		object = mbus_json_create_object();
		if (object == NULL) {
			goto bail;
		}
		mbus_json_add_item_to_array(commands, object);
		mbus_json_add_string_to_object_cs(object, "identifier", command_get_identifier(command));
	}
	method_set_result_payload(method, result);
	return 0;
bail:	if (result != NULL) {
		mbus_json_delete(result);
	}
	return -1;
}

static int server_handle_command_clients (struct mbus_server *server, struct method *method)
{
	struct mbus_json *source;
	struct mbus_json *clients;
	struct client *client;
	clients = NULL;
	if (server == NULL) {
		mbus_errorf("server is null");
		goto bail;
	}
	if (method == NULL) {
		mbus_errorf("method is null");
		goto bail;
	}
	clients = mbus_json_create_array();
	if (clients == NULL) {
		goto bail;
	}
	TAILQ_FOREACH(client, &server->clients, clients) {
		if (client_get_name(client) == NULL) {
			continue;
		}
		source = mbus_json_create_string(client_get_name(client));
		if (source == NULL) {
			goto bail;
		}
		mbus_json_add_item_to_array(clients, source);
	}
	method_set_result_payload(method, clients);
	return 0;
bail:	if (clients != NULL) {
		mbus_json_delete(clients);
	}
	return -1;
}

static int server_handle_command_close (struct mbus_server *server, struct method *method)
{
	struct client *client;
	const char *source;
	if (server == NULL) {
		mbus_errorf("server is null");
		goto bail;
	}
	if (method == NULL) {
		mbus_errorf("method is null");
		goto bail;
	}
	source = mbus_json_get_string_value(method_get_request_payload(method), "source", NULL);
	if (source == NULL) {
		mbus_errorf("method request source is null");
		goto bail;
	}
	if (strcmp(source, MBUS_SERVER_NAME) == 0) {
		server->running = 0;
	} else {
		TAILQ_FOREACH(client, &server->clients, clients) {
			if (client_get_name(client) == NULL) {
				continue;
			}
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
	return 0;
bail:	return -1;
}

static int server_handle_command_call (struct mbus_server *server, struct method *method)
{
	struct method *request;
	struct client *client;
	struct command *command;
	if (server == NULL) {
		mbus_errorf("server is null");
		goto bail;
	}
	if (method == NULL) {
		mbus_errorf("method is null");
		goto bail;
	}
	TAILQ_FOREACH(client, &server->clients, clients) {
		if (client_get_name(client) == NULL) {
			continue;
		}
		if (strcmp(method_get_request_destination(method), client_get_name(client)) != 0) {
			continue;
		}
		break;
	}
	if (client == NULL) {
		mbus_errorf("client %s does not exists", method_get_request_destination(method));
		goto bail;
	}
	TAILQ_FOREACH(command, &client->commands, commands) {
		if (strcmp(method_get_request_identifier(method), command->identifier) == 0) {
			break;
		}
	}
	if (command == NULL) {
		mbus_errorf("client: %s does not have such command: %s", method_get_request_destination(method), method_get_request_identifier(method));
		goto bail;
	}
	request = method_create_response(MBUS_METHOD_TYPE_COMMAND, client_get_name(method_get_source(method)), method_get_request_identifier(method), method_get_request_sequence(method), method_get_request_payload(method));
	if (request == NULL) {
		mbus_errorf("can not create call method");
		goto bail;
	}
	client_push_request(client, request);
	return 0;
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
	destination = mbus_json_get_string_value(method_get_request_payload(method), "destination", NULL);
	identifier = mbus_json_get_string_value(method_get_request_payload(method), "identifier", NULL);
	sequence = mbus_json_get_int_value(method_get_request_payload(method), "sequence", -1);
	rc = mbus_json_get_int_value(method_get_request_payload(method), "return", -1);
	TAILQ_FOREACH(client, &server->clients, clients) {
		if (client_get_name(client) == NULL) {
			continue;
		}
		if (strcmp(client_get_name(client), destination) != 0) {
			continue;
		}
		TAILQ_FOREACH_SAFE(wait, &client->waits, methods, nwait) {
			if (sequence != method_get_request_sequence(wait)) {
				continue;
			}
			if (strcmp(method_get_request_destination(wait), source) != 0) {
				continue;
			}
			if (strcmp(method_get_request_identifier(wait), identifier) != 0) {
				continue;
			}
			TAILQ_REMOVE(&client->waits, wait, methods);
			method_set_result_code(wait, rc);
			method_set_result_payload(wait, mbus_json_duplicate(mbus_json_get_object(method_get_request_payload(method), "result"), 1));
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
		mbus_debugf("handle method: %s, %s, %s", method_get_request_type(method), method_get_request_identifier(method), method_get_request_destination(method));
		TAILQ_REMOVE(&server->methods, method, methods);
		if (strcmp(method_get_request_type(method), MBUS_METHOD_TYPE_COMMAND) == 0) {
			if (strcmp(method_get_request_destination(method), MBUS_SERVER_NAME) == 0) {
				response = 1;
				if (strcmp(method_get_request_identifier(method), MBUS_SERVER_COMMAND_CREATE) == 0) {
					rc = server_handle_command_create(server, method);
				} else if (strcmp(method_get_request_identifier(method), MBUS_SERVER_COMMAND_SUBSCRIBE) == 0) {
					rc = server_handle_command_subscribe(server, method);
				} else if (strcmp(method_get_request_identifier(method), MBUS_SERVER_COMMAND_UNSUBSCRIBE) == 0) {
					rc = server_handle_command_unsubscribe(server, method);
				} else if (strcmp(method_get_request_identifier(method), MBUS_SERVER_COMMAND_REGISTER) == 0) {
					rc = server_handle_command_register(server, method);
				} else if (strcmp(method_get_request_identifier(method), MBUS_SERVER_COMMAND_UNREGISTER) == 0) {
					rc = server_handle_command_unregister(server, method);
				} else if (strcmp(method_get_request_identifier(method), MBUS_SERVER_COMMAND_RESULT) == 0) {
					rc = server_handle_command_result(server, method);
				} else if (strcmp(method_get_request_identifier(method), MBUS_SERVER_COMMAND_EVENT) == 0) {
					rc = server_handle_command_event(server, method);
				} else if (strcmp(method_get_request_identifier(method), MBUS_SERVER_COMMAND_STATUS) == 0) {
					rc = server_handle_command_status(server, method);
				} else if (strcmp(method_get_request_identifier(method), MBUS_SERVER_COMMAND_CLIENT) == 0) {
					rc = server_handle_command_client(server, method);
				} else if (strcmp(method_get_request_identifier(method), MBUS_SERVER_COMMAND_CLIENTS) == 0) {
					rc = server_handle_command_clients(server, method);
				} else if (strcmp(method_get_request_identifier(method), MBUS_SERVER_COMMAND_CLOSE) == 0) {
					rc = server_handle_command_close(server, method);
				} else {
					rc = -1;
				}
			} else {
				response = 0;
				rc = server_handle_command_call(server, method);
			}
			if (rc != 0) {
				mbus_errorf("can not execute method type: '%s', identifier: '%s'", method_get_request_type(method), method_get_request_identifier(method));
			}
			if (response == 1 || rc != 0) {
				mbus_debugf("  push to result");
				method_set_result_code(method, rc);
				client_push_result(method_get_source(method), method);
			} else {
				mbus_debugf("  push to wait");
				client_push_wait(method_get_source(method), method);
			}
		}
		if (strcmp(method_get_request_type(method), MBUS_METHOD_TYPE_EVENT) == 0) {
			mbus_debugf("  push to trash");
			rc = server_send_event_to(server, client_get_name(method_get_source(method)), method_get_request_destination(method), method_get_request_identifier(method), method_get_request_payload(method));
			if (rc != 0) {
				mbus_errorf("can not send event: %s", method_get_source(method));
			}
			method_destroy(method);
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

static int server_handle_method_event (struct mbus_server *server, struct method *method)
{
	TAILQ_INSERT_TAIL(&server->methods, method, methods);
	return 0;
}

static int server_handle_method (struct mbus_server *server, struct client *client, const char *string)
{
	int rc;
	struct method *method;
	method = method_create_request(client, string);
	if (method == NULL) {
		mbus_errorf("invalid method");
		goto bail;
	}
	if (strcmp(method_get_request_type(method), MBUS_METHOD_TYPE_COMMAND) == 0) {
		rc = server_handle_method_command(server, method);
	} else if (strcmp(method_get_request_type(method), MBUS_METHOD_TYPE_EVENT) == 0) {
		rc = server_handle_method_event(server, method);
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

#if defined(WS_ENABLE) && (WS_ENABLE == 1)

static int ws_protocol_mbus_callback (struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
	int rc;
	struct mbus_server *server;
	struct listener *listener;
	struct ws_client_data *data;
	const struct lws_protocols *protocol;
	(void) wsi;
	(void) reason;
	(void) user;
	(void) in;
	(void) len;
	mbus_debugf("ws callback");
	server = g_server;
	listener = NULL;
	data = (struct ws_client_data *) user;
	protocol = lws_get_protocol(wsi);
	if (protocol != NULL) {
		listener = protocol->user;
	}
	switch (reason) {
		case LWS_CALLBACK_LOCK_POLL:
			mbus_debugf("  lock poll");
			break;
		case LWS_CALLBACK_ADD_POLL_FD:
			mbus_debugf("  add poll fd");
			{
				struct pollfd *tmp;
				struct lws_pollargs *pa = (struct lws_pollargs *) in;
				mbus_debugf("    fd: %d", pa->fd);
				{
					unsigned int i;
					struct lws_pollargs *pa = (struct lws_pollargs *) in;
					for (i = 0; i < listener->u.ws.pollfds.length; i++) {
						if (listener->u.ws.pollfds.pollfds[i].fd == pa->fd) {
							listener->u.ws.pollfds.pollfds[i].events = pa->events;
							break;
						}
					}
					if (i < listener->u.ws.pollfds.length) {
						break;
					}
				}
				if (listener->u.ws.pollfds.length + 1 > listener->u.ws.pollfds.size) {
					while (listener->u.ws.pollfds.length + 1 > listener->u.ws.pollfds.size) {
						listener->u.ws.pollfds.size += 1024;
					}
					tmp = realloc(listener->u.ws.pollfds.pollfds, sizeof(struct pollfd) * listener->u.ws.pollfds.size);
					if (tmp == NULL) {
						tmp = malloc(sizeof(int) * listener->u.ws.pollfds.size);
						if (tmp == NULL) {
							mbus_errorf("can not allocate memory");
							goto bail;
						}
						memcpy(tmp, listener->u.ws.pollfds.pollfds, sizeof(struct pollfd) * listener->u.ws.pollfds.length);
						free(listener->u.ws.pollfds.pollfds);
					}
					listener->u.ws.pollfds.pollfds = tmp;
				}
				listener->u.ws.pollfds.pollfds[listener->u.ws.pollfds.length].fd = pa->fd;
				listener->u.ws.pollfds.pollfds[listener->u.ws.pollfds.length].events = pa->events;
				listener->u.ws.pollfds.pollfds[listener->u.ws.pollfds.length].revents = 0;
				listener->u.ws.pollfds.length += 1;
			}
			break;
		case LWS_CALLBACK_CHANGE_MODE_POLL_FD:
			mbus_debugf("  change mode poll fd");
			{
				unsigned int i;
				struct lws_pollargs *pa = (struct lws_pollargs *) in;
				mbus_debugf("    fd: %d, events: 0x%08x", pa->fd, pa->events);
				for (i = 0; i < listener->u.ws.pollfds.length; i++) {
					if (listener->u.ws.pollfds.pollfds[i].fd == pa->fd) {
						listener->u.ws.pollfds.pollfds[i].events = pa->events;
						mbus_debugf("      %d", i);
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
				mbus_debugf("    fd: %d", pa->fd);
				for (i = 0; i < listener->u.ws.pollfds.length; i++) {
					if (listener->u.ws.pollfds.pollfds[i].fd == pa->fd) {
						memmove(&listener->u.ws.pollfds.pollfds[i], &listener->u.ws.pollfds.pollfds[i + 1], sizeof(struct pollfd) * (listener->u.ws.pollfds.length - i - 1));
						listener->u.ws.pollfds.length -= 1;
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
			memset(data, 0, sizeof(struct ws_client_data));
			data->wsi = wsi;
			data->client = client_accept(listener_get_type(listener));
			if (data->client == NULL) {
				mbus_errorf("can not accept client");
				goto bail;
			}
			rc = client_set_socket(data->client, (struct mbus_socket *) data);
			if (rc != 0) {
				mbus_errorf("can not set client socket");
				client_destroy(data->client);
				data->client = NULL;
				goto bail;
			}
			TAILQ_INSERT_TAIL(&server->clients, data->client, clients);
			lws_callback_on_writable(data->wsi);
			break;
#if 0
		case LWS_CALLBACK_HTTP_DROP_PROTOCOL:
			mbus_debugf("  http drop protocol");
			break;
#endif
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
			if (data->client != NULL) {
				client_set_socket(data->client, NULL);
				data->client = NULL;
			}
			data->wsi = NULL;
			break;
		case LWS_CALLBACK_RECEIVE:
			mbus_debugf("  server receive");
			if (data->client == NULL ||
			    client_get_socket(data->client) == NULL) {
				mbus_debugf("client is closed");
				goto bail;
			}
			mbus_debugf("    data: %p", data);
			mbus_debugf("      wsi   : %p", data->wsi);
			mbus_debugf("      client: %p", data->client);
			mbus_debugf("      buffer:");
			mbus_debugf("        in:");
			mbus_debugf("          length  : %d", mbus_buffer_length(data->client->buffer.in));
			mbus_debugf("          size    : %d", mbus_buffer_size(data->client->buffer.in));
			mbus_debugf("        out:");
			mbus_debugf("          length  : %d", mbus_buffer_length(data->client->buffer.out));
			mbus_debugf("          size    : %d", mbus_buffer_size(data->client->buffer.out));
			mbus_debugf("    in: %p", in);
			mbus_debugf("    len: %zd", len);
			if (data->wsi == NULL &&
			    data->client == NULL) {
			}
			rc = mbus_buffer_push(data->client->buffer.in, in, len);
			if (rc != 0) {
				mbus_errorf("can not push in");
				goto bail;
			}
			{
				uint8_t *ptr;
				uint8_t *end;
				uint32_t expected;
				mbus_debugf("      buffer.in:");
				mbus_debugf("        length  : %d", mbus_buffer_length(data->client->buffer.in));
				mbus_debugf("        size    : %d", mbus_buffer_size(data->client->buffer.in));
				while (1) {
					char *string;
					ptr = mbus_buffer_base(data->client->buffer.in);
					end = ptr + mbus_buffer_length(data->client->buffer.in);
					if (end - ptr < 4) {
						break;
					}
					expected  = *ptr++ << 0x00;
					expected |= *ptr++ << 0x08;
					expected |= *ptr++ << 0x10;
					expected |= *ptr++ << 0x18;
					expected = ntohl(expected);
					mbus_debugf("expected %d", expected);
					if (end - ptr < (int32_t) expected) {
						break;
					}
					mbus_debugf("message: '%.*s'", expected, ptr);
					string = _strndup((char *) ptr, expected);
					if (string == NULL) {
						mbus_errorf("can not allocate memory");
						goto bail;
					}
					mbus_debugf("new request from client: '%s', '%s'", client_get_name(data->client), string);
					rc = server_handle_method(server, data->client, string);
					if (rc != 0) {
						mbus_errorf("can not handle request, closing client: '%s' connection", client_get_name(data->client));
						free(string);
						goto bail;
					}
					free(string);
					rc = mbus_buffer_shift(data->client->buffer.in, sizeof(uint32_t) + expected);
					if (rc != 0) {
						mbus_errorf("can not shift in");
						goto bail;
					}
				}
			}
			break;
		case LWS_CALLBACK_SERVER_WRITEABLE:
			mbus_debugf("  server writable");
			if (data->client == NULL ||
			    client_get_socket(data->client) == NULL) {
				mbus_debugf("client is closed");
				goto bail;
			}
			while (mbus_buffer_length(data->client->buffer.out) > 0 &&
			       lws_send_pipe_choked(data->wsi) == 0) {
				uint8_t *ptr;
				uint8_t *end;
				uint32_t expected;
				ptr = mbus_buffer_base(data->client->buffer.out);
				end = ptr + mbus_buffer_length(data->client->buffer.out);
				expected = end - ptr;
				if (end - ptr < (int32_t) expected) {
					break;
				}
				if (expected > BUFFER_OUT_CHUNK_SIZE - LWS_PRE) {
					expected = BUFFER_OUT_CHUNK_SIZE - LWS_PRE;
				}
				mbus_debugf("write");
				memset(data->client->buffer.out_chunk, 0, LWS_PRE + expected);
				memcpy(data->client->buffer.out_chunk + LWS_PRE, ptr, expected);
				mbus_debugf("payload: %d, %.*s", expected - 4, expected - 4, ptr + 4);
				rc = lws_write(data->wsi, data->client->buffer.out_chunk + LWS_PRE, expected, LWS_WRITE_BINARY);
				mbus_debugf("expected: %d, rc: %d", expected, rc);
				rc = mbus_buffer_shift(data->client->buffer.out, rc);
				if (rc != 0) {
					mbus_errorf("can not shift in");
					goto bail;
				}
				break;
			}
			if (mbus_buffer_length(data->client->buffer.out) > 0) {
				lws_callback_on_writable(data->wsi);
			}
			break;
		case LWS_CALLBACK_OPENSSL_LOAD_EXTRA_SERVER_VERIFY_CERTS:
			mbus_debugf("  load extra server verify certs");
			break;
		case LWS_CALLBACK_FILTER_HTTP_CONNECTION:
			mbus_debugf("  filter http connection");
			break;
		case LWS_CALLBACK_CLOSED_HTTP:
			mbus_debugf("  closed http");
			break;
		default:
			mbus_errorf("unknown reason: %d", reason);
			break;
	}
	return 0;
bail:	return -1;
}

#endif

int mbus_server_run_timeout (struct mbus_server *server, int milliseconds)
{
	int rc;
	int read_rc;
	char *string;
	unsigned int c;
	unsigned int n;
	unsigned long current;
	struct client *client;
	struct client *nclient;
	struct client *wclient;
	struct client *nwclient;
	struct method *method;
	struct method *nmethod;
	current = mbus_clock_get();
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
	if (milliseconds < 0 || milliseconds > MBUS_SERVER_DEFAULT_TIMEOUT) {
		milliseconds = MBUS_SERVER_DEFAULT_TIMEOUT;
	}
	mbus_debugf("  check ping timeout");
	TAILQ_FOREACH_SAFE(client, &server->clients, clients, nclient) {
		if (client_get_socket(client) == NULL) {
			continue;
		}
		if (client->ping.interval <= 0) {
			continue;
		}
		if (client->ping.enabled == 0) {
			continue;
		}
		mbus_debugf("    client: %s, recv: %lu, interval: %d, timeout: %d, missed: %d, threshold: %d",
				client_get_name(client),
				client->ping.ping_recv_tsms,
				client->ping.interval,
				client->ping.timeout,
				client->ping.ping_missed_count,
				client->ping.threshold);
		if (mbus_clock_after(current, client->ping.ping_recv_tsms + client->ping.interval + client->ping.timeout)) {
			mbus_infof("%s ping timeout: %ld, %ld, %d, %d", client_get_name(client), current, client->ping.ping_recv_tsms, client->ping.interval, client->ping.timeout);
			client->ping.ping_missed_count += 1;
			client->ping.ping_recv_tsms = client->ping.ping_recv_tsms + client->ping.interval;
		}
		if (client->ping.ping_missed_count > client->ping.threshold) {
			mbus_errorf("%s missed too many pings, %d > %d. closing connection", client_get_name(client), client->ping.ping_missed_count, client->ping.threshold);
			client_set_socket(client, NULL);
		}
	}
	mbus_debugf("  prepare out buffer");
	TAILQ_FOREACH_SAFE(client, &server->clients, clients, nclient) {
		enum mbus_compress_method compression;
		mbus_debugf("    client: %s", client_get_name(client));
		if (client_get_socket(client) == NULL) {
			continue;
		}
		compression = client_get_compression(client);
		if (client_get_results_count(client) > 0) {
			method = client_pop_result(client);
			if (method == NULL) {
				mbus_errorf("could not pop result from client");
				continue;
			}
			if (strcmp(method_get_request_destination(method), MBUS_SERVER_NAME) == 0) {
				if (strcmp(method_get_request_identifier(method), MBUS_SERVER_COMMAND_CREATE) == 0) {
					compression = mbus_compress_method_none;
				}
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
		mbus_debugf("      message: %s, %s", mbus_compress_method_string(compression), string);
		rc = mbus_buffer_push_string(client->buffer.out, compression, string);
		if (rc != 0) {
			mbus_errorf("can not push string");
			method_destroy(method);
			goto bail;
		}
		method_destroy(method);
	}
	n  = 0;
	n += server->listeners.count;
	n += server->clients.count;
#if defined(WS_ENABLE) && (WS_ENABLE == 1)
	{
		struct listener *listener;
		TAILQ_FOREACH(listener, &server->listeners, listeners) {
			if (listener_get_type(listener) == listener_type_ws) {
				n += listener->u.ws.pollfds.length;
			}
		}
	}
#endif
	if (n > server->pollfds.size) {
		struct pollfd *tmp;
		while (n > server->pollfds.size) {
			server->pollfds.size += 1024;
		}
		tmp = realloc(server->pollfds.pollfds, sizeof(struct pollfd) * server->pollfds.size);
		if (tmp == NULL) {
			tmp = malloc(sizeof(int) * server->pollfds.size);
			if (tmp == NULL) {
				mbus_errorf("can not allocate memory");
				goto bail;
			}
			memcpy(tmp, server->pollfds.pollfds, sizeof(struct pollfd) * server->pollfds.length);
			free(server->pollfds.pollfds);
		}
		server->pollfds.pollfds = tmp;
	}
	n = 0;
	{
		struct listener *listener;
		TAILQ_FOREACH(listener, &server->listeners, listeners) {
			if (listener_get_type(listener) == listener_type_tcp) {
				server->pollfds.pollfds[n].events = POLLIN;
				server->pollfds.pollfds[n].revents = 0;
				server->pollfds.pollfds[n].fd = mbus_socket_get_fd(listener->u.tcp.socket);
				n += 1;
			}
			if (listener_get_type(listener) == listener_type_uds) {
				server->pollfds.pollfds[n].events = POLLIN;
				server->pollfds.pollfds[n].revents = 0;
				server->pollfds.pollfds[n].fd = mbus_socket_get_fd(listener->u.uds.socket);
				n += 1;
			}
		}
	}
	mbus_debugf("  prepare polling");
	TAILQ_FOREACH(client, &server->clients, clients) {
		if (client_get_socket(client) == NULL) {
			continue;
		}
		if (client_get_listener_type(client) == listener_type_tcp) {
			server->pollfds.pollfds[n].events = POLLIN;
			server->pollfds.pollfds[n].revents = 0;
			server->pollfds.pollfds[n].fd = mbus_socket_get_fd(client_get_socket(client));
			mbus_debugf("    in : %s", client_get_name(client));
			if (mbus_buffer_length(client->buffer.out) > 0
#if defined(SSL_ENABLE) && (SSL_ENABLE == 1)
			    || client->ssl.want_write != 0
#endif
			    ) {
				mbus_debugf("    out: %s", client_get_name(client));
				server->pollfds.pollfds[n].events |= POLLOUT;
			}
			n += 1;
		}
		if (client_get_listener_type(client) == listener_type_uds) {
			server->pollfds.pollfds[n].events = POLLIN;
			server->pollfds.pollfds[n].revents = 0;
			server->pollfds.pollfds[n].fd = mbus_socket_get_fd(client_get_socket(client));
			mbus_debugf("    in : %s", client_get_name(client));
			if (mbus_buffer_length(client->buffer.out) > 0
#if defined(SSL_ENABLE) && (SSL_ENABLE == 1)
			    || client->ssl.want_write != 0
#endif
			    ) {
				mbus_debugf("    out: %s", client_get_name(client));
				server->pollfds.pollfds[n].events |= POLLOUT;
			}
			n += 1;
		}
#if defined(WS_ENABLE) && (WS_ENABLE == 1)
		if (client_get_listener_type(client) == listener_type_ws) {
			struct ws_client_data *data;
			data = (struct ws_client_data *) client_get_socket(client);
			if (mbus_buffer_length(client->buffer.out) > 0) {
				mbus_debugf("    out: %s", client_get_name(client));
				lws_callback_on_writable(data->wsi);
			}
		}
#endif
	}
	{
		mbus_debugf("    pollfds: %d", n);
		if (0) {
			unsigned int i;
			for (i = 0; i < n; i++) {
				mbus_debugf("      fd: %d, events: 0x%08x", server->pollfds.pollfds[i].fd, server->pollfds.pollfds[i].events);
			}
		}
	}
#if defined(WS_ENABLE) && (WS_ENABLE == 1)
	{
		struct listener *listener;
		TAILQ_FOREACH(listener, &server->listeners, listeners) {
			if (listener_get_type(listener) == listener_type_ws) {
				if (listener->u.ws.pollfds.length > 0) {
					{
						mbus_debugf("    ws: %d", listener->u.ws.pollfds.length);
						if (0) {
							unsigned int i;
							for (i = 0; i < listener->u.ws.pollfds.length; i++) {
								mbus_debugf("      fd: %d, events: 0x%08x", listener->u.ws.pollfds.pollfds[i].fd, listener->u.ws.pollfds.pollfds[i].events);
							}
						}
					}
					memcpy(&server->pollfds.pollfds[n], listener->u.ws.pollfds.pollfds, sizeof(struct pollfd) * listener->u.ws.pollfds.length);
					n += listener->u.ws.pollfds.length;
				}
			}
		}
	}
#endif
	rc = poll(server->pollfds.pollfds, n, milliseconds);
	if (rc == 0) {
		goto out;
	}
	if (rc < 0) {
		mbus_errorf("poll error");
		goto bail;
	}
	mbus_debugf("  check poll events");
	for (c = 0; c < n; c++) {
		if (server->pollfds.pollfds[c].revents == 0) {
			continue;
		}
		mbus_debugf("    fd: %d, events: 0x%08x, revents: 0x%08x", server->pollfds.pollfds[c].fd, server->pollfds.pollfds[c].events, server->pollfds.pollfds[c].revents);
		{
			struct client *client;
			struct listener *listener;
			TAILQ_FOREACH(listener, &server->listeners, listeners) {
				if (listener_get_type(listener) == listener_type_tcp) {
					if (server->pollfds.pollfds[c].fd == mbus_socket_get_fd(listener->u.tcp.socket)) {
						mbus_debugf("      listener: tcp");
					}
				}
				if (listener_get_type(listener) == listener_type_uds) {
					if (server->pollfds.pollfds[c].fd == mbus_socket_get_fd(listener->u.uds.socket)) {
						mbus_debugf("      listener: uds");
					}
				}
			}
			TAILQ_FOREACH(client, &server->clients, clients) {
				if (client_get_socket(client) == NULL) {
					continue;
				}
#if defined(WS_ENABLE) && (WS_ENABLE == 1)
				if (client_get_listener_type(client) == listener_type_ws) {
					struct ws_client_data *data;
					data = (struct ws_client_data *) client_get_socket(client);
					if (lws_get_socket_fd(data->wsi) == server->pollfds.pollfds[c].fd) {
						mbus_debugf("      ws client: %s", client_get_name(client));
					}
				}
#endif
				if (client_get_listener_type(client) == listener_type_uds) {
					if (mbus_socket_get_fd(client_get_socket(client)) == server->pollfds.pollfds[c].fd) {
						mbus_debugf("      uds client: %s", client_get_name(client));
					}
				}
				if (client_get_listener_type(client) == listener_type_tcp) {
					if (mbus_socket_get_fd(client_get_socket(client)) == server->pollfds.pollfds[c].fd) {
						mbus_debugf("      tcp client: %s", client_get_name(client));
					}
				}
			}
		}
		{
			struct listener *listener;
			TAILQ_FOREACH(listener, &server->listeners, listeners) {
				if (listener_get_type(listener) == listener_type_tcp) {
					if (server->pollfds.pollfds[c].fd == mbus_socket_get_fd(listener->u.tcp.socket)) {
						if (server->pollfds.pollfds[c].revents & POLLIN) {
							rc = server_accept_client(server, listener, listener->u.tcp.socket);
							if (rc == -1) {
								mbus_errorf("can not accept new connection");
								goto bail;
							} else if (rc == -2) {
								mbus_errorf("rejected new connection");
							} else {
								mbus_infof("accepted new client");
							}
						}
					}
				}
				if (listener_get_type(listener) == listener_type_uds) {
					if (server->pollfds.pollfds[c].fd == mbus_socket_get_fd(listener->u.uds.socket)) {
						if (server->pollfds.pollfds[c].revents & POLLIN) {
							rc = server_accept_client(server, listener, listener->u.uds.socket);
							if (rc == -1) {
								mbus_errorf("can not accept new connection");
								goto bail;
							} else if (rc == -2) {
								mbus_errorf("rejected new connection");
							} else {
								mbus_infof("accepted new client");
							}
						}
					}
				}
			}
		}
		client = server_find_client_by_fd(server, server->pollfds.pollfds[c].fd);
		if (client == NULL) {
			continue;
			mbus_errorf("can not find client by socket");
			goto bail;
		}
#if defined(WS_ENABLE) && (WS_ENABLE == 1)
		if (client_get_listener_type(client) == listener_type_ws) {
			continue;
		}
#endif
		if (server->pollfds.pollfds[c].revents & POLLIN) {
			rc = mbus_buffer_reserve(client->buffer.in, mbus_buffer_length(client->buffer.in) + BUFFER_IN_CHUNK_SIZE);
			if (rc != 0) {
				mbus_errorf("can not reserve client buffer");
				client_set_socket(client, NULL);
				break;
			}
#if defined(SSL_ENABLE) && (SSL_ENABLE == 1)
			if (client->ssl.ssl == NULL) {
#endif
				read_rc = read(mbus_socket_get_fd(client->socket),
						mbus_buffer_base(client->buffer.in) + mbus_buffer_length(client->buffer.in),
						mbus_buffer_size(client->buffer.in) - mbus_buffer_length(client->buffer.in));
#if defined(SSL_ENABLE) && (SSL_ENABLE == 1)
			} else {
				read_rc = 0;
				do {
					rc = mbus_buffer_reserve(client->buffer.in, (mbus_buffer_length(client->buffer.in) + read_rc) + 1024);
					if (rc != 0) {
						mbus_errorf("can not reserve client buffer");
						goto bail;
					}
					client->ssl.want_read = 0;
					rc = SSL_read(client->ssl.ssl,
							mbus_buffer_base(client->buffer.in) + (mbus_buffer_length(client->buffer.in) + read_rc),
							mbus_buffer_size(client->buffer.in) - (mbus_buffer_length(client->buffer.in) + read_rc));
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
					goto skip_in;
				} else if (errno == EAGAIN) {
					goto skip_in;
				} else if (errno == EWOULDBLOCK) {
					goto skip_in;
				}
				mbus_debugf("can not read data from client");
				client_set_socket(client, NULL);
				mbus_infof("client: '%s' connection reset by peer", client_get_name(client));
				break;
			} else {
				uint8_t *ptr;
				uint8_t *end;
				uint8_t *data;
				uint32_t expected;
				uint32_t uncompressed;
				rc = mbus_buffer_set_length(client->buffer.in, mbus_buffer_length(client->buffer.in) + read_rc);
				if (rc != 0) {
					mbus_errorf("can not set buffer length, closing client: '%s' connection", client_get_name(client));
					client_set_socket(client, NULL);
					break;
				}
				mbus_debugf("      buffer.in:");
				mbus_debugf("        length  : %d", mbus_buffer_length(client->buffer.in));
				mbus_debugf("        size    : %d", mbus_buffer_size(client->buffer.in));
				while (1) {
					char *string;
					ptr = mbus_buffer_base(client->buffer.in);
					end = ptr + mbus_buffer_length(client->buffer.in);
					if (end - ptr < 4) {
						break;
					}
					memcpy(&expected, ptr, sizeof(expected));
					ptr += sizeof(expected);
					expected = ntohl(expected);
					mbus_debugf("        expected: %d", expected);
					if (end - ptr < (int32_t) expected) {
						break;
					}
					data = ptr;
					if (client_get_compression(client) == mbus_compress_method_none) {
						data = ptr;
						uncompressed = expected;
					} else {
						int uncompressedlen;
						memcpy(&uncompressed, ptr, sizeof(uncompressed));
						uncompressed = ntohl(uncompressed);
						mbus_debugf("        uncompressed: %d", uncompressed);
						uncompressedlen = uncompressed;
						rc = mbus_uncompress_data(client_get_compression(client), (void **) &data, &uncompressedlen, ptr + sizeof(uncompressed), expected - sizeof(uncompressed));
						if (rc != 0) {
							mbus_errorf("can not uncompress data");
							goto bail;
						}
						if (uncompressedlen != (int) uncompressed) {
							mbus_errorf("can not uncompress data");
							goto bail;
						}
					}
					mbus_debugf("        message: '%.*s'", uncompressed, data);
					string = _strndup((char *) data, uncompressed);
					if (string == NULL) {
						mbus_errorf("can not allocate memory, closing client: '%s' connection", client_get_name(client));
						client_set_socket(client, NULL);
						break;
					}
					rc = server_handle_method(server, client, string);
					if (rc != 0) {
						mbus_errorf("can not handle request, closing client: '%s' connection", client_get_name(client));
						client_set_socket(client, NULL);
						free(string);
						if (data != ptr) {
							free(data);
						}
						break;
					}
					rc = mbus_buffer_shift(client->buffer.in, sizeof(uint32_t) + expected);
					if (rc != 0) {
						mbus_errorf("can not shift in, closing client: '%s' connection", client_get_name(client));
						client_set_socket(client, NULL);
						free(string);
						if (data != ptr) {
							free(data);
						}
						break;
					}
					free(string);
					if (data != ptr) {
						free(data);
					}
				}
			}
		}
skip_in:
		if (server->pollfds.pollfds[c].revents & POLLOUT) {
			if (mbus_buffer_length(client->buffer.out) <= 0) {
				mbus_errorf("logic error");
				goto bail;
			}
#if defined(SSL_ENABLE) && (SSL_ENABLE == 1)
			if (client->ssl.ssl == NULL) {
#endif
				rc = write(mbus_socket_get_fd(client_get_socket(client)), mbus_buffer_base(client->buffer.out), mbus_buffer_length(client->buffer.out));
#if defined(SSL_ENABLE) && (SSL_ENABLE == 1)
			} else {
				client->ssl.want_write = 0;
				rc = SSL_write(client->ssl.ssl, mbus_buffer_base(client->buffer.out), mbus_buffer_length(client->buffer.out));
				if (rc <= 0) {
					int error;
					error = SSL_get_error(client->ssl.ssl, rc);
					mbus_errorf("can not write ssl: %d", error);
					if (error == SSL_ERROR_WANT_READ) {
						rc = 0;
						errno = EAGAIN;
						client->ssl.want_read = 1;
					} else if (error == SSL_ERROR_WANT_WRITE) {
						rc = 0;
						errno = EAGAIN;
						client->ssl.want_write = 1;
					} else if (error == SSL_ERROR_SYSCALL) {
						rc = -1;
						errno = EIO;
					} else {
						char ebuf[256];
						mbus_errorf("can not read ssl: %d", error);
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
			if (rc <= 0) {
				if (errno == EINTR) {
					goto skip_out;
				} else if (errno == EAGAIN) {
					goto skip_out;
				} else if (errno == EWOULDBLOCK) {
					goto skip_out;
				}
				mbus_debugf("can not write string to client");
				client_set_socket(client, NULL);
				mbus_infof("client: '%s' connection reset by server", client_get_name(client));
				break;
			} else {
				rc = mbus_buffer_shift(client->buffer.out, rc);
				if (rc != 0) {
					mbus_errorf("can not set buffer length, closing client: '%s' connection", client_get_name(client));
					client_set_socket(client, NULL);
					break;
				}
			}
		}
skip_out:
		if (server->pollfds.pollfds[c].revents & (POLLERR | POLLHUP | POLLNVAL)) {
			client_set_socket(client, NULL);
			mbus_infof("client: '%s' connection reset by server", client_get_name(client));
			break;
		}
	}
out:
#if defined(WS_ENABLE) && (WS_ENABLE == 1)
	{
		struct listener *listener;
		TAILQ_FOREACH(listener, &server->listeners, listeners) {
			if (listener_get_type(listener) == listener_type_ws) {
				lws_service(listener->u.ws.context, 0);
			}
		}
	}
#endif
	rc = server_handle_methods(server);
	if (rc != 0) {
		mbus_errorf("can not handle methods");
		goto bail;
	}
	TAILQ_FOREACH(client, &server->clients, clients) {
		if (client_get_socket(client) == NULL) {
			continue;
		}
		if ((client_get_status(client) & client_status_accepted) != 0) {
			continue;
		}
		client_set_status(client, client_get_status(client) | client_status_accepted);
		mbus_infof("client: '%s' accepted to server", client_get_name(client));
	}
	TAILQ_FOREACH(client, &server->clients, clients) {
		if (client_get_socket(client) == NULL) {
			continue;
		}
		if (client_get_name(client) == NULL) {
			continue;
		}
		if ((client_get_status(client) & client_status_accepted) == 0) {
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
			if (strcmp(method_get_request_type(method), MBUS_METHOD_TYPE_EVENT) == 0) {
				continue;
			}
			TAILQ_REMOVE(&server->methods, method, methods);
			method_destroy(method);
		}
		TAILQ_FOREACH_SAFE(wclient, &server->clients, clients, nwclient) {
			TAILQ_FOREACH_SAFE(method, &wclient->waits, methods, nmethod) {
				if (client_get_name(client) != NULL &&
				    strcmp(method_get_request_destination(method), client_get_name(client)) == 0) {
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
	struct listener *listener;
	if (server == NULL) {
		return;
	}
	mbus_infof("destroying server");
	while (server->listeners.tqh_first != NULL) {
		listener = server->listeners.tqh_first;
		TAILQ_REMOVE(&server->listeners, server->listeners.tqh_first, listeners);
		listener_destroy(listener);
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
	if (server->pollfds.pollfds != NULL) {
		free(server->pollfds.pollfds);
	}
#if defined(SSL_ENABLE) && (SSL_ENABLE == 1)
	EVP_cleanup();
#endif
	free(server);
}

int mbus_server_options_default (struct mbus_server_options *options)
{
	if (options == NULL) {
		mbus_errorf("options is invalid");
		goto bail;
	}
	memset(options, 0, sizeof(struct mbus_server_options));
	options->tcp.enabled = MBUS_SERVER_TCP_ENABLE;
	options->tcp.address = MBUS_SERVER_TCP_ADDRESS;
	options->tcp.port = MBUS_SERVER_TCP_PORT;

	options->uds.enabled = MBUS_SERVER_UDS_ENABLE;
	options->uds.address = MBUS_SERVER_UDS_ADDRESS;
	options->uds.port = MBUS_SERVER_UDS_PORT;

#if defined(WS_ENABLE) && (WS_ENABLE == 1)
	options->ws.enabled = MBUS_SERVER_WS_ENABLE;
	options->ws.address = MBUS_SERVER_WS_ADDRESS;
	options->ws.port = MBUS_SERVER_WS_PORT;
#endif

#if defined(SSL_ENABLE) && (SSL_ENABLE == 1)
	options->tcps.enabled = MBUS_SERVER_TCPS_ENABLE;
	options->tcps.address = MBUS_SERVER_TCPS_ADDRESS;
	options->tcps.port = MBUS_SERVER_TCPS_PORT;
	options->tcps.certificate = MBUS_SERVER_TCPS_CERTIFICATE;
	options->tcps.privatekey = MBUS_SERVER_TCPS_PRIVATEKEY;

	options->udss.enabled = MBUS_SERVER_UDSS_ENABLE;
	options->udss.address = MBUS_SERVER_UDSS_ADDRESS;
	options->udss.port = MBUS_SERVER_UDSS_PORT;
	options->udss.certificate = MBUS_SERVER_TCPS_CERTIFICATE;
	options->udss.privatekey = MBUS_SERVER_TCPS_PRIVATEKEY;

#if defined(WS_ENABLE) && (WS_ENABLE == 1)
	options->wss.enabled = MBUS_SERVER_WSS_ENABLE;
	options->wss.address = MBUS_SERVER_WSS_ADDRESS;
	options->wss.port = MBUS_SERVER_WSS_PORT;
	options->wss.certificate = MBUS_SERVER_TCPS_CERTIFICATE;
	options->wss.privatekey = MBUS_SERVER_TCPS_PRIVATEKEY;
#endif
#endif
	return 0;
bail:	return -1;
}

int mbus_server_options_from_argv (struct mbus_server_options *options, int argc, char *argv[])
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
	mbus_server_options_default(options);

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
			case OPTION_SERVER_TCP_ENABLE:
				options->tcp.enabled = !!atoi(optarg);
				break;
			case OPTION_SERVER_TCP_ADDRESS:
				options->tcp.address = optarg;
				break;
			case OPTION_SERVER_TCP_PORT:
				options->tcp.port = atoi(optarg);
				break;
			case OPTION_SERVER_UDS_ENABLE:
				options->uds.enabled = !!atoi(optarg);
				break;
			case OPTION_SERVER_UDS_ADDRESS:
				options->uds.address = optarg;
				break;
			case OPTION_SERVER_UDS_PORT:
				options->uds.port = atoi(optarg);
				break;
#if defined(WS_ENABLE) && (WS_ENABLE == 1)
			case OPTION_SERVER_WS_ENABLE:
				options->ws.enabled = !!atoi(optarg);
				break;
			case OPTION_SERVER_WS_ADDRESS:
				options->ws.address = optarg;
				break;
			case OPTION_SERVER_WS_PORT:
				options->ws.port = atoi(optarg);
				break;
#endif
#if defined(SSL_ENABLE) && (SSL_ENABLE == 1)
			case OPTION_SERVER_TCPS_ENABLE:
				options->tcps.enabled = !!atoi(optarg);
				break;
			case OPTION_SERVER_TCPS_ADDRESS:
				options->tcps.address = optarg;
				break;
			case OPTION_SERVER_TCPS_PORT:
				options->tcps.port = atoi(optarg);
				break;
			case OPTION_SERVER_TCPS_CERTIFICATE:
				options->tcps.certificate = optarg;
				break;
			case OPTION_SERVER_TCPS_PRIVATEKEY:
				options->tcps.privatekey = optarg;
				break;
			case OPTION_SERVER_UDSS_ENABLE:
				options->udss.enabled = !!atoi(optarg);
				break;
			case OPTION_SERVER_UDSS_ADDRESS:
				options->udss.address = optarg;
				break;
			case OPTION_SERVER_UDSS_PORT:
				options->udss.port = atoi(optarg);
				break;
			case OPTION_SERVER_UDSS_CERTIFICATE:
				options->tcps.certificate = optarg;
				break;
			case OPTION_SERVER_UDSS_PRIVATEKEY:
				options->tcps.privatekey = optarg;
				break;
#if defined(WS_ENABLE) && (WS_ENABLE == 1)
			case OPTION_SERVER_WSS_ENABLE:
				options->wss.enabled = !!atoi(optarg);
				break;
			case OPTION_SERVER_WSS_ADDRESS:
				options->wss.address = optarg;
				break;
			case OPTION_SERVER_WSS_PORT:
				options->wss.port = atoi(optarg);
				break;
			case OPTION_SERVER_WSS_CERTIFICATE:
				options->wss.certificate = optarg;
				break;
			case OPTION_SERVER_WSS_PRIVATEKEY:
				options->wss.privatekey = optarg;
				break;
#endif
#endif
			case OPTION_HELP:
				mbus_server_usage();
				goto bail;
		}
	}

	optind = o_optind;
	free(_argv);
	return 0;
bail:	if (_argv != NULL) {
		free(_argv);
	}
	optind = o_optind;
	return -1;
}

struct mbus_server * mbus_server_create (int argc, char *argv[])
{
	int rc;
	struct mbus_server *server;
	struct mbus_server_options options;
	server = NULL;
	rc = mbus_server_options_default(&options);
	if (rc != 0) {
		mbus_errorf("can not get default options");
		goto bail;
	}
	rc = mbus_server_options_from_argv(&options, argc, argv);
	if (rc != 0) {
		mbus_errorf("can not get options from argv");
		goto bail;
	}
	server = mbus_server_create_with_options(&options);
	if (server == NULL) {
		mbus_errorf("can not create server with options");
		goto bail;
	}
	return server;
bail:	if (server != NULL) {
		mbus_server_destroy(server);
	}
	return NULL;
}

struct mbus_server * mbus_server_create_with_options (const struct mbus_server_options *_options)
{
	struct mbus_server *server;

	server = NULL;

	{
		struct sigaction sa;
		memset(&sa, 0, sizeof(struct sigaction));
		sa.sa_handler = SIG_IGN;
		sa.sa_flags = 0;
		sigaction(SIGPIPE, &sa, 0);
	}

#if defined(SSL_ENABLE) && (SSL_ENABLE == 1)
	SSL_library_init();
	SSL_load_error_strings();
#endif
	server = malloc(sizeof(struct mbus_server));
	if (server == NULL) {
		mbus_errorf("can not allocate memory");
		goto bail;
	}
	g_server = server;
	memset(server, 0, sizeof(struct mbus_server));
	TAILQ_INIT(&server->clients);
	TAILQ_INIT(&server->methods);
	TAILQ_INIT(&server->listeners);

	mbus_server_options_default(&server->options);
	if (_options != NULL) {
		memcpy(&server->options, _options, sizeof(struct mbus_server_options));
	}

	if (server->options.tcp.enabled == 0 &&
	    server->options.uds.enabled == 0 &&
	    server->options.ws.enabled == 0 &&
	    server->options.tcps.enabled == 0 &&
	    server->options.udss.enabled == 0 &&
	    server->options.wss.enabled == 0
	    ) {
		mbus_errorf("at least one protocol must be enabled");
		goto bail;
	}

	mbus_infof("creating server");
	mbus_infof("using mbus version '%s, %s'", mbus_git_commit(), mbus_git_revision());
#if defined(SSL_ENABLE) && (SSL_ENABLE == 1)
	mbus_infof("using openssl version '%s'", SSLeay_version(SSLEAY_VERSION));
#endif

	if (server->options.tcp.enabled == 1) {
		struct listener *listener;
		listener = listener_create(listener_type_tcp, server->options.tcp.address, server->options.tcp.port, NULL, NULL);
		if (listener == NULL) {
			mbus_errorf("can not create listener: tcp");
			goto bail;
		}
		TAILQ_INSERT_TAIL(&server->listeners, listener, listeners);
		mbus_infof("listening from: '%s:%s:%d'", "tcp", server->options.tcp.address, server->options.tcp.port);
	}
	if (server->options.uds.enabled == 1) {
		struct listener *listener;
		listener = listener_create(listener_type_uds, server->options.uds.address, server->options.uds.port, NULL, NULL);
		if (listener == NULL) {
			mbus_errorf("can not create listener: uds");
			goto bail;
		}
		TAILQ_INSERT_TAIL(&server->listeners, listener, listeners);
		mbus_infof("listening from: '%s:%s:%d'", "uds", server->options.uds.address, server->options.uds.port);
	}
#if defined(WS_ENABLE) && (WS_ENABLE == 1)
	if (server->options.ws.enabled == 1) {
		struct listener *listener;
		listener = listener_create(listener_type_ws, server->options.ws.address, server->options.ws.port, NULL, NULL);
		if (listener == NULL) {
			mbus_errorf("can not create listener: ws");
			goto bail;
		}
		TAILQ_INSERT_TAIL(&server->listeners, listener, listeners);
		mbus_infof("listening from: '%s:%s:%d'", "ws", server->options.ws.address, server->options.ws.port);
	}
#endif
	if (server->options.tcps.enabled == 1) {
		struct listener *listener;
		listener = listener_create(listener_type_tcp, server->options.tcps.address, server->options.tcps.port, server->options.tcps.certificate, server->options.tcps.privatekey);
		if (listener == NULL) {
			mbus_infof("can not create listener '%s:%s:%d'", "tcps", server->options.tcps.address, server->options.tcps.port);
		} else {
			TAILQ_INSERT_TAIL(&server->listeners, listener, listeners);
			mbus_infof("listening from: '%s:%s:%d'", "tcps", server->options.tcps.address, server->options.tcps.port);
		}
	}
	if (server->options.udss.enabled == 1) {
		struct listener *listener;
		listener = listener_create(listener_type_uds, server->options.udss.address, server->options.udss.port, server->options.udss.certificate, server->options.udss.privatekey);
		if (listener == NULL) {
			mbus_infof("can not create listener '%s:%s:%d'", "udss", server->options.udss.address, server->options.udss.port);
		} else {
			TAILQ_INSERT_TAIL(&server->listeners, listener, listeners);
			mbus_infof("listening from: '%s:%s:%d'", "udss", server->options.udss.address, server->options.udss.port);
		}
	}
#if defined(WS_ENABLE) && (WS_ENABLE == 1)
#if defined(SSL_ENABLE) && (SSL_ENABLE == 1)
	if (server->options.wss.enabled == 1) {
		struct listener *listener;
		listener = listener_create(listener_type_ws, server->options.wss.address, server->options.wss.port, server->options.wss.certificate, server->options.wss.privatekey);
		if (listener == NULL) {
			mbus_infof("can not create listener '%s:%s:%d'", "wss", server->options.wss.address, server->options.wss.port);
		} else {
			TAILQ_INSERT_TAIL(&server->listeners, listener, listeners);
			mbus_infof("listening from: '%s:%s:%d'", "wss", server->options.wss.address, server->options.wss.port);
		}
	}
#endif
#endif
	{
		unsigned int nlisteners;
		struct listener *listener;
		nlisteners = 0;
		TAILQ_FOREACH(listener, &server->listeners, listeners) {
			nlisteners += 1;
		}
		if (nlisteners == 0) {
			mbus_errorf("attached listener count: %d is invalid", nlisteners);
			goto bail;
		}
	}
	server->running = 1;
	return server;
bail:	mbus_server_destroy(server);
	return NULL;
}

int mbus_server_tcp_enabled (struct mbus_server *server)
{
	if (server == NULL) {
		mbus_errorf("server is null");
		goto bail;
	}
	return server->options.tcp.enabled;
bail:	return -1;
}

const char * mbus_server_tcp_address (struct mbus_server *server)
{
	if (server == NULL) {
		mbus_errorf("server is null");
		goto bail;
	}
	return server->options.tcp.address;
bail:	return NULL;
}

int mbus_server_tcp_port (struct mbus_server *server)
{
	if (server == NULL) {
		mbus_errorf("server is null");
		goto bail;
	}
	return server->options.tcp.port;
bail:	return -1;
}

int mbus_server_uds_enabled (struct mbus_server *server)
{
	if (server == NULL) {
		mbus_errorf("server is null");
		goto bail;
	}
	return server->options.uds.enabled;
bail:	return -1;
}

const char * mbus_server_uds_address (struct mbus_server *server)
{
	if (server == NULL) {
		mbus_errorf("server is null");
		goto bail;
	}
	return server->options.uds.address;
bail:	return NULL;
}

int mbus_server_uds_port (struct mbus_server *server)
{
	if (server == NULL) {
		mbus_errorf("server is null");
		goto bail;
	}
	return server->options.uds.port;
bail:	return -1;
}

int mbus_server_ws_enabled (struct mbus_server *server)
{
	if (server == NULL) {
		mbus_errorf("server is null");
		goto bail;
	}
#if defined(WS_ENABLE) && (WS_ENABLE == 1)
	return server->options.ws.enabled;
#endif
bail:	return -1;
}

const char * mbus_server_ws_address (struct mbus_server *server)
{
	if (server == NULL) {
		mbus_errorf("server is null");
		goto bail;
	}
#if defined(WS_ENABLE) && (WS_ENABLE == 1)
	return server->options.ws.address;
#endif
bail:	return NULL;
}

int mbus_server_ws_port (struct mbus_server *server)
{
	if (server == NULL) {
		mbus_errorf("server is null");
		goto bail;
	}
#if defined(WS_ENABLE) && (WS_ENABLE == 1)
	return server->options.ws.port;
#endif
bail:	return -1;
}
