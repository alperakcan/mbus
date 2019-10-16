
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

#if !defined(MAX)
#define MAX(a, b)       (((a) > (b)) ? (a) : (b))
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
#include "command.h"
#include "subscription.h"
#include "method.h"
#include "listener.h"
#include "server.h"

enum client_status {
	client_status_accepted		= 0x00000001,
	client_status_connected		= 0x00000002,
};

enum client_connection_close_code {
        client_connection_close_code_unknown            = 0,
        client_connection_close_code_close_comand       = 1,
        client_connection_close_code_ping_threshold     = 2,
        client_connection_close_code_connection_closed  = 3,
        client_connection_close_code_internal_error     = 4
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
	char *identifier;
	enum client_status status;
	enum mbus_compress_method compression;
	struct listener *listener;
	struct connection *connection;
	enum client_connection_close_code connection_close_code;
	struct mbus_buffer *buffer_in;
	struct mbus_buffer *buffer_out;
	int ping_enabled;
	int ping_interval;
	int ping_timeout;
	int ping_threshold;
	unsigned long long ping_recv_tsms;
	int ping_missed_count;
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
	struct mbus_server_options options;
	struct listeners listeners;
	struct clients clients;
	struct methods methods;
	struct {
		unsigned int length;
		unsigned int size;
		struct pollfd *pollfds;
	} pollfds;
	struct {
		unsigned int length;
		unsigned int size;
		struct pollfd *pollfds;
	} ws_pollfds;
	char *password;
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

#define OPTION_SERVER_PASSWORD                  0x801

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

	{ "mbus-server-password",               required_argument,      NULL,   OPTION_SERVER_PASSWORD },

	{ NULL,					0,			NULL,	0 },
};

__attribute__ ((__visibility__("default"))) void mbus_server_usage (void)
{
	fprintf(stdout, "mbus server arguments:\n");
	fprintf(stdout, "  --mbus-debug-level            : debug level (default: %s)\n", mbus_debug_level_to_string(mbus_debug_level));

	fprintf(stdout, "  --mbus-server-tcp-enable      : server tcp enable (default: %d)\n", MBUS_SERVER_TCP_ENABLE);
	fprintf(stdout, "  --mbus-server-tcp-address     : server tcp address (default: %s)\n", MBUS_SERVER_TCP_ADDRESS);
	fprintf(stdout, "  --mbus-server-tcp-port        : server tcp port (default: %d)\n", MBUS_SERVER_TCP_PORT);

	fprintf(stdout, "  --mbus-server-uds-enable      : server uds enable (default: %d)\n", MBUS_SERVER_UDS_ENABLE);
	fprintf(stdout, "  --mbus-server-uds-address     : server uds address (default: %s)\n", MBUS_SERVER_UDS_ADDRESS);
	fprintf(stdout, "  --mbus-server-uds-port        : server uds port (default: %d)\n", MBUS_SERVER_UDS_PORT);

#if defined(WS_ENABLE) && (WS_ENABLE == 1)
	fprintf(stdout, "  --mbus-server-ws-enable       : server ws enable (default: %d)\n", MBUS_SERVER_WS_ENABLE);
	fprintf(stdout, "  --mbus-server-ws-address      : server ws address (default: %s)\n", MBUS_SERVER_WS_ADDRESS);
	fprintf(stdout, "  --mbus-server-ws-port         : server ws port (default: %d)\n", MBUS_SERVER_WS_PORT);
#endif

#if defined(SSL_ENABLE) && (SSL_ENABLE == 1)
	fprintf(stdout, "  --mbus-server-tcps-enable     : server tcps enable (default: %d)\n", MBUS_SERVER_TCPS_ENABLE);
	fprintf(stdout, "  --mbus-server-tcps-address    : server tcps address (default: %s)\n", MBUS_SERVER_TCPS_ADDRESS);
	fprintf(stdout, "  --mbus-server-tcps-port       : server tcps port (default: %d)\n", MBUS_SERVER_TCPS_PORT);
	fprintf(stdout, "  --mbus-server-tcps-certificate: server tcps certificate (default: %s)\n", MBUS_SERVER_TCPS_CERTIFICATE);
	fprintf(stdout, "  --mbus-server-tcps-privatekey : server tcps privatekey (default: %s)\n", MBUS_SERVER_TCPS_PRIVATEKEY);

	fprintf(stdout, "  --mbus-server-udss-enable     : server udss enable (default: %d)\n", MBUS_SERVER_UDSS_ENABLE);
	fprintf(stdout, "  --mbus-server-udss-address    : server udss address (default: %s)\n", MBUS_SERVER_UDSS_ADDRESS);
	fprintf(stdout, "  --mbus-server-udss-port       : server udss port (default: %d)\n", MBUS_SERVER_UDSS_PORT);
	fprintf(stdout, "  --mbus-server-udss-certificate: server udss certificate (default: %s)\n", MBUS_SERVER_UDSS_CERTIFICATE);
	fprintf(stdout, "  --mbus-server-udss-privatekey : server udss privatekey (default: %s)\n", MBUS_SERVER_UDSS_PRIVATEKEY);

#if defined(WS_ENABLE) && (WS_ENABLE == 1)
	fprintf(stdout, "  --mbus-server-wss-enable      : server ws enable (default: %d)\n", MBUS_SERVER_WSS_ENABLE);
	fprintf(stdout, "  --mbus-server-wss-address     : server ws address (default: %s)\n", MBUS_SERVER_WSS_ADDRESS);
	fprintf(stdout, "  --mbus-server-wss-port        : server ws port (default: %d)\n", MBUS_SERVER_WSS_PORT);
	fprintf(stdout, "  --mbus-server-wss-certificate : server ws certificate (default: %s)\n", MBUS_SERVER_WSS_CERTIFICATE);
	fprintf(stdout, "  --mbus-server-wss-privatekey  : server ws privatekey (default: %s)\n", MBUS_SERVER_WSS_PRIVATEKEY);
#endif
#endif

	fprintf(stdout, "  --mbus-server-password        : server password (default: %s)\n", "(null)");
	fprintf(stdout, "  --mbus-help                   : this text\n");
}

static size_t _strnlen (const char *s, size_t maxlen)
{
	 size_t len;
	for (len = 0; len < maxlen; len++, s++) {
		if (!*s) {
			break;
		}
	}
	return (len);
}

static char * _strndup (const char *s, size_t n)
{
	char *result;
	size_t len = _strnlen(s, n);
	result = (char *) malloc(len + 1);
	if (result == NULL) {
		return NULL;
	}
	result[len] = '\0';
	return (char *) memcpy(result, s, len);
}

static const char * client_connection_close_code_string (enum client_connection_close_code close_code)
{
        switch (close_code) {
                case client_connection_close_code_unknown:              return "unknown";
                case client_connection_close_code_close_comand:         return "close_command";
                case client_connection_close_code_ping_threshold:       return "ping_threshold";
                case client_connection_close_code_connection_closed:    return "connection_closed";
                case client_connection_close_code_internal_error:       return "internal_error";
        }
        return "unknown";
}

static struct listener * client_get_listener (struct client *client)
{
	if (client == NULL) {
		mbus_errorf("client is null");
		goto bail;
	}
	return client->listener;
bail:	return NULL;
}

static struct connection * client_get_connection (struct client *client)
{
        if (client == NULL) {
                mbus_errorf("client is null");
                return NULL;
        }
        return client->connection;
}

static enum client_connection_close_code client_get_connection_close_code (struct client *client)
{
        if (client == NULL) {
                mbus_errorf("client is null");
                return client_connection_close_code_unknown;
        }
        return client->connection_close_code;
}

static int client_set_connection (struct client *client, struct connection *connection, enum client_connection_close_code close_code)
{
	int rc;
	if (client == NULL) {
		mbus_errorf("client is null");
		goto bail;
	}
	if (connection == NULL) {
		if (client->connection != NULL) {
			rc = mbus_server_connection_close(client->connection);
			if (rc != 0) {
				mbus_errorf("can not close connection");
				goto bail;
			}
			client->connection = NULL;
		}
	} else {
		if (client->connection != NULL) {
			mbus_errorf("client connection is not null");
			return -1;
		}
		client->connection = connection;
	}
	client->connection_close_code = close_code;
	return 0;
bail:	return -1;
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

static int client_set_identifier (struct client *client, const char *identifier)
{
	if (client == NULL) {
		mbus_errorf("client is null");
		goto bail;
	}
	if (identifier == NULL) {
		mbus_errorf("identifier is null");
		goto bail;
	}
	if (client->identifier != NULL) {
		free(client->identifier);
	}
	client->identifier = strdup(identifier);
	if (client->identifier == NULL) {
		mbus_errorf("can not allocate memory");
		goto bail;
	}
	return 0;
bail:	return -1;
}

static const char * client_get_identifier (struct client *client)
{
	if (client == NULL) {
		mbus_errorf("client is null");
		return NULL;
	}
	return client->identifier;
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
		if ((strcmp(mbus_server_subscription_get_source(subscription), source) == 0) &&
		    (strcmp(mbus_server_subscription_get_event(subscription), event) == 0)) {
			break;
		}
	}
	if (subscription != NULL) {
		TAILQ_REMOVE(&client->subscriptions, subscription, subscriptions);
		mbus_server_subscription_destroy(subscription);
		mbus_infof("unsubscribed '%s' from '%s', '%s'", client_get_identifier(client), source, event);
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
		if ((strcmp(mbus_server_subscription_get_source(subscription), source) == 0) &&
		    (strcmp(mbus_server_subscription_get_event(subscription), event) == 0)) {
			goto out;
		}
	}
	subscription = mbus_server_subscription_create(source, event);
	if (subscription == NULL) {
		mbus_errorf("can not create subscription");
		goto bail;
	}
	TAILQ_INSERT_TAIL(&client->subscriptions, subscription, subscriptions);
	mbus_infof("subscribed '%s' to '%s', '%s'", client_get_identifier(client), source, event);
out:	return 0;
bail:	mbus_server_subscription_destroy(subscription);
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
		if (strcmp(mbus_server_command_get_identifier(command), identifier) == 0) {
			goto out;
		}
	}
	command = mbus_server_command_create(identifier);
	if (command == NULL) {
		mbus_errorf("can not create command");
		goto bail;
	}
	TAILQ_INSERT_TAIL(&client->commands, command, commands);
	mbus_infof("registered '%s' '%s'", client_get_identifier(client), identifier);
out:	return 0;
bail:	mbus_server_command_destroy(command);
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
		if (strcmp(mbus_server_command_get_identifier(command), identifier) == 0) {
			break;
		}
	}
	TAILQ_REMOVE(&client->commands, command, commands);
	mbus_server_command_destroy(command);
	mbus_infof("unregistered '%s' '%s'", client_get_identifier(client), identifier);
	return 0;
bail:	mbus_server_command_destroy(command);
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
	if (client->connection != NULL) {
		mbus_server_connection_close(client_get_connection(client));
	}
	if (client->identifier != NULL) {
		free(client->identifier);
	}
	while (client->commands.tqh_first != NULL) {
		command = client->commands.tqh_first;
		TAILQ_REMOVE(&client->commands, client->commands.tqh_first, commands);
		mbus_server_command_destroy(command);
	}
	while (client->subscriptions.tqh_first != NULL) {
		subscription = client->subscriptions.tqh_first;
		TAILQ_REMOVE(&client->subscriptions, client->subscriptions.tqh_first, subscriptions);
		mbus_server_subscription_destroy(subscription);
	}
	while (client->requests.tqh_first != NULL) {
		request = client->requests.tqh_first;
		TAILQ_REMOVE(&client->requests, client->requests.tqh_first, methods);
		mbus_server_method_destroy(request);
	}
	while (client->results.tqh_first != NULL) {
		result = client->results.tqh_first;
		TAILQ_REMOVE(&client->results, client->results.tqh_first, methods);
		mbus_server_method_destroy(result);
	}
	while (client->events.tqh_first != NULL) {
		event = client->events.tqh_first;
		TAILQ_REMOVE(&client->events, client->events.tqh_first, methods);
		mbus_server_method_destroy(event);
	}
	while (client->waits.tqh_first != NULL) {
		wait = client->waits.tqh_first;
		TAILQ_REMOVE(&client->waits, client->waits.tqh_first, methods);
		mbus_server_method_destroy(wait);
	}
	if (client->buffer_in != NULL) {
		mbus_buffer_destroy(client->buffer_in);
	}
	if (client->buffer_out != NULL) {
		mbus_buffer_destroy(client->buffer_out);
	}
	free(client);
}

static struct client * client_create (struct listener *listener, struct connection *connection)
{
	struct client *client;
	client = NULL;
	if (listener == NULL) {
		mbus_errorf("listener is invalid");
		goto bail;
	}
	if (connection == NULL) {
		mbus_errorf("connection is invalid");
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
	client->status = 0;
	client->listener = listener;
	client->connection = connection;
	client->ssequence = MBUS_METHOD_SEQUENCE_START;
	client->esequence = MBUS_METHOD_SEQUENCE_START;
	client->buffer_in = mbus_buffer_create();
	if (client->buffer_in == NULL) {
		mbus_errorf("can not create buffer");
		goto bail;
	}
	client->buffer_out = mbus_buffer_create();
	if (client->buffer_out == NULL) {
		mbus_errorf("can not create buffer");
		goto bail;
	}
	return client;
bail:	client_destroy(client);
	return NULL;
}

static struct client * server_find_client_by_identifier (struct mbus_server *server, const char *identifier)
{
	struct client *client;
	if (identifier == NULL) {
		mbus_errorf("identifier is null");
		return NULL;
	}
	TAILQ_FOREACH(client, &server->clients, clients) {
		if (client_get_identifier(client) == NULL) {
			continue;
		}
		if (strcmp(client_get_identifier(client), identifier) == 0) {
			return client;
		}
	}
	return NULL;
}

static struct client * server_find_client_by_fd (struct mbus_server *server, int fd)
{
	int cfd;
	struct client *client;
	if (fd < 0) {
		mbus_errorf("fd is invalid");
		return NULL;
	}
	TAILQ_FOREACH(client, &server->clients, clients) {
		if (client_get_listener(client) == NULL) {
			continue;
		}
		if (client_get_connection(client) == NULL) {
			continue;
		}
		cfd = mbus_server_connection_get_fd(client_get_connection(client));
		if (cfd < 0) {
			mbus_errorf("connection fd is invalid");
			continue;
		}
		if (cfd == fd) {
			return client;
		}
	}
	return NULL;
}

static __attribute__ ((__unused__)) struct client * server_find_client_by_connection (struct mbus_server *server, struct connection *connection)
{
	struct client *client;
	if (connection == NULL) {
		mbus_errorf("connection is null");
		return NULL;
	}
	TAILQ_FOREACH(client, &server->clients, clients) {
		if (client_get_connection(client) == connection) {
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
	if (strcmp(destination, MBUS_SERVER_IDENTIFIER) == 0) {
		if (strcmp(identifier, MBUS_SERVER_EVENT_PING) == 0) {
			client = server_find_client_by_identifier(server, source);
			if (client != NULL) {
				client->ping_recv_tsms = mbus_clock_monotonic();
			}
			rc = server_send_event_to(server, MBUS_SERVER_IDENTIFIER, source, MBUS_SERVER_EVENT_PONG, NULL);
			if (rc != 0) {
				mbus_errorf("can not send pong to: %s", source);
				goto bail;
			}
		}
	} else if (strcmp(destination, MBUS_METHOD_EVENT_DESTINATION_ALL) == 0) {
		TAILQ_FOREACH(client, &server->clients, clients) {
			if (client_get_identifier(client) == NULL) {
				continue;
			}
			if (strcmp(client_get_identifier(client), source) == 0) {
				continue;
			}
			method = mbus_server_method_create_response(MBUS_METHOD_TYPE_EVENT, source, identifier, client->esequence, payload);
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
				mbus_server_method_destroy(method);
				goto bail;
			}
		}
	} else if (strcmp(destination, MBUS_METHOD_EVENT_DESTINATION_SUBSCRIBERS) == 0) {
		TAILQ_FOREACH(client, &server->clients, clients) {
			TAILQ_FOREACH(subscription, &client->subscriptions, subscriptions) {
				if (strcmp(mbus_server_subscription_get_source(subscription), MBUS_METHOD_EVENT_SOURCE_ALL) != 0) {
					if (strcmp(mbus_server_subscription_get_source(subscription), source) != 0) {
						continue;
					}
				}
				if (strcmp(mbus_server_subscription_get_event(subscription), MBUS_METHOD_EVENT_IDENTIFIER_ALL) != 0) {
					if (strcmp(mbus_server_subscription_get_event(subscription), identifier) != 0) {
						continue;
					}
				}
				method = mbus_server_method_create_response(MBUS_METHOD_TYPE_EVENT, source, identifier, client->esequence, payload);
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
					mbus_server_method_destroy(method);
					goto bail;
				}
				break;
			}
		}
	} else {
		TAILQ_FOREACH(client, &server->clients, clients) {
			if (client_get_identifier(client) == NULL) {
				continue;
			}
			if (strcmp(client_get_identifier(client), destination) != 0) {
				continue;
			}
			method = mbus_server_method_create_response(MBUS_METHOD_TYPE_EVENT, source, identifier, client->esequence, payload);
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
				mbus_server_method_destroy(method);
				goto bail;
			}
			break;
		}
	}
	return 0;
bail:	return -1;
}

static int server_send_event_connected (struct mbus_server *server, struct client *client)
{
	int rc;
        char address[MAX(INET_ADDRSTRLEN, INET6_ADDRSTRLEN)];
	struct mbus_json *payload;
	payload = NULL;
	if (server == NULL) {
		mbus_errorf("server is null");
		goto bail;
	}
	payload = mbus_json_create_object();
	if (payload == NULL) {
		mbus_errorf("can not create payload");
		goto bail;
	}
	mbus_json_add_string_to_object_cs(payload, "source", client_get_identifier(client));
        mbus_json_add_string_to_object_cs(payload, "address", mbus_socket_fd_get_address(mbus_server_connection_get_fd(client_get_connection(client)), address, sizeof(address)));
	rc = server_send_event_to(server, MBUS_SERVER_IDENTIFIER, MBUS_METHOD_EVENT_DESTINATION_SUBSCRIBERS, MBUS_SERVER_EVENT_CONNECTED, payload);
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

static int server_send_event_disconnected (struct mbus_server *server, const char *source, enum client_connection_close_code close_code)
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
	mbus_json_add_string_to_object_cs(payload, "reason", client_connection_close_code_string(close_code));
	rc = server_send_event_to(server, MBUS_SERVER_IDENTIFIER, MBUS_METHOD_EVENT_DESTINATION_SUBSCRIBERS, MBUS_SERVER_EVENT_DISCONNECTED, payload);
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
	rc = server_send_event_to(server, MBUS_SERVER_IDENTIFIER, MBUS_METHOD_EVENT_DESTINATION_SUBSCRIBERS, MBUS_SERVER_EVENT_SUBSCRIBED, payload);
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
	rc = server_send_event_to(server, MBUS_SERVER_IDENTIFIER, MBUS_METHOD_EVENT_DESTINATION_SUBSCRIBERS, MBUS_SERVER_EVENT_UNSUBSCRIBED, payload);
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

static int server_send_event_registered (struct mbus_server *server, const char *source, const char *identifier)
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
	rc = server_send_event_to(server, MBUS_SERVER_IDENTIFIER, MBUS_METHOD_EVENT_DESTINATION_SUBSCRIBERS, MBUS_SERVER_EVENT_REGISTERED, payload);
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

static int server_send_event_unregistered (struct mbus_server *server, const char *source, const char *identifier)
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
	rc = server_send_event_to(server, MBUS_SERVER_IDENTIFIER, MBUS_METHOD_EVENT_DESTINATION_SUBSCRIBERS, MBUS_SERVER_EVENT_UNREGISTERED, payload);
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

static int server_client_connection_establish (struct mbus_server *server, struct listener *listener, struct connection *connection)
{
	struct client *client;
	client = NULL;
	if (server == NULL) {
		mbus_errorf("server is invalid");
		goto bail;
	}
	if (listener == NULL) {
		mbus_errorf("listener is invalid");
		goto bail;
	}
	if (connection == NULL) {
		mbus_errorf("connection is invalid");
		goto bail;
	}
	client = client_create(listener, connection);
	if (client == NULL) {
		mbus_errorf("can not create client");
		goto bail;
	}
	TAILQ_INSERT_TAIL(&server->clients, client, clients);
	return 0;
bail:	if (client != NULL) {
		client_destroy(client);
	}
	return -1;
}

#if defined(WS_ENABLE) && (WS_ENABLE == 1)

static int server_listener_ws_callback_connection_established (void *context, struct listener *listener, struct connection *connection)
{
	return server_client_connection_establish(context, listener, connection);
}

static int server_listener_ws_callback_connection_receive (void *context, struct listener *listener, struct connection *connection, void *in, int len)
{
	struct client *client;
	struct mbus_server *server = (struct mbus_server *) context;
	(void) listener;
	client = server_find_client_by_connection(server, connection);
	if (client == NULL) {
		return -1;
	}
	return mbus_buffer_push(client->buffer_in, in, len);
}

static struct mbus_buffer * server_listener_ws_callback_connection_writable (void *context, struct listener *listener, struct connection *connection)
{
	struct client *client;
	struct mbus_server *server = (struct mbus_server *) context;
	(void) listener;
	client = server_find_client_by_connection(server, connection);
	if (client == NULL) {
		return NULL;
	}
	return client->buffer_out;
}

static int server_listener_ws_callback_connection_closed (void *context, struct listener *listener, struct connection *connection)
{
	struct client *client;
	struct mbus_server *server = (struct mbus_server *) context;
	(void) listener;
	client = server_find_client_by_connection(server, connection);
	if (client == NULL) {
		return -1;
	}
	return client_set_connection(client, NULL, client_connection_close_code_connection_closed);
}

static int server_listener_ws_callback_poll_add (void *context, struct listener *listener, int fd, unsigned int events)
{
	struct mbus_server *server = (struct mbus_server *) context;
	{
		unsigned int i;
		for (i = 0; i < server->ws_pollfds.length; i++) {
			if (server->ws_pollfds.pollfds[i].fd == fd) {
				server->ws_pollfds.pollfds[i].events = events;
				break;
			}
		}
		if (i < server->ws_pollfds.length) {
			return 0;
		}
	}
	if (server->ws_pollfds.length + 1 > server->ws_pollfds.size) {
		struct pollfd *tmp;
		while (server->ws_pollfds.length + 1 > server->ws_pollfds.size) {
			server->ws_pollfds.size += 1024;
		}
		tmp = realloc(server->ws_pollfds.pollfds, sizeof(struct pollfd) * server->ws_pollfds.size);
		if (tmp == NULL) {
			tmp = malloc(sizeof(int) * server->ws_pollfds.size);
			if (tmp == NULL) {
				mbus_errorf("can not allocate memory");
				goto bail;
			}
			memcpy(tmp, server->ws_pollfds.pollfds, sizeof(struct pollfd) * server->ws_pollfds.length);
			free(server->ws_pollfds.pollfds);
		}
		server->ws_pollfds.pollfds = tmp;
	}
	server->ws_pollfds.pollfds[server->ws_pollfds.length].fd = fd;
	server->ws_pollfds.pollfds[server->ws_pollfds.length].events = events;
	server->ws_pollfds.pollfds[server->ws_pollfds.length].revents = 0;
	server->ws_pollfds.length += 1;
	return 0;
bail:	return -1;
}

static int server_listener_ws_callback_poll_mod (void *context, struct listener *listener, int fd, unsigned int events)
{
	struct mbus_server *server = (struct mbus_server *) context;
	{
		unsigned int i;
		for (i = 0; i < server->ws_pollfds.length; i++) {
			if (server->ws_pollfds.pollfds[i].fd == fd) {
				server->ws_pollfds.pollfds[i].events = events;
				server->ws_pollfds.pollfds[i].revents = 0;
				break;
			}
		}
	}
	return 0;
}

static int server_listener_ws_callback_poll_del (void *context, struct listener *listener, int fd)
{
	struct mbus_server *server = (struct mbus_server *) context;
	{
		unsigned int i;
		for (i = 0; i < server->ws_pollfds.length; i++) {
			if (server->ws_pollfds.pollfds[i].fd == fd) {
				memmove(&server->ws_pollfds.pollfds[i], &server->ws_pollfds.pollfds[i + 1], sizeof(struct pollfd) * (server->ws_pollfds.length - i - 1));
				server->ws_pollfds.length -= 1;
				break;
			}
		}
	}
	return 0;
}

#endif

static int server_handle_command_create (struct mbus_server *server, struct method *method)
{
	int rc;
	char ridentifier[64];
	const char *identifier;
	const char *password;
	struct client *client;
	if (server == NULL) {
		mbus_errorf("server is null");
		goto bail;
	}
	if (method == NULL) {
		mbus_errorf("method is null");
		goto bail;
	}
	identifier = client_get_identifier(mbus_server_method_get_source(method));
	if (identifier != NULL) {
		mbus_errorf("invalid client state");
		goto bail;
	}
	{
		struct mbus_json *payload;
		payload = mbus_server_method_get_request_payload(method);
		{
			identifier = mbus_json_get_string_value(payload, "identifier", NULL);
			if (identifier == NULL ||
			    strlen(identifier) == 0) {
				mbus_infof("empty identifier, creating a random identifier for client");
				while (1) {
					snprintf(ridentifier, sizeof(ridentifier), "%s%08x", MBUS_SERVER_CLIENT_IDENTIFIER_PREFIX, rand());
					client = server_find_client_by_identifier(server, ridentifier);
					if (client == NULL) {
						break;
					}
				}
				identifier = ridentifier;
			}
			client = server_find_client_by_identifier(server, identifier);
			if (client != NULL) {
				mbus_errorf("client with identifier: %s already exists", identifier);
				goto bail;
			}
			rc = client_set_identifier(mbus_server_method_get_source(method), identifier);
			if (rc != 0) {
				mbus_errorf("can not set client identifier");
				goto bail;
			}
			client = server_find_client_by_identifier(server, identifier);
			if (client == NULL) {
				mbus_errorf("client identifier logic error");
				goto bail;
			}
		}
                {
                        password = mbus_json_get_string_value(payload, "password", NULL);
                        if (server->password == NULL) {
                                mbus_infof("getting everyone in");
                        } else if (password == NULL) {
                                mbus_errorf("invalid password %s != %s", server->password, "(null)");
                                goto bail;
                        } else if (strcmp(server->password, password) != 0) {
                                mbus_errorf("invalid password %s != %s", server->password, password);
                                goto bail;
                        }
                }
		{
			struct mbus_json *ping;
			ping = mbus_json_get_object(payload, "ping");
			client->ping_interval = mbus_json_get_int_value(ping, "interval", -1);
			client->ping_timeout = mbus_json_get_int_value(ping, "timeout", -1);
			client->ping_threshold = mbus_json_get_int_value(ping, "threshold", -1);
			if (client->ping_interval <= 0) {
				client->ping_interval = 0;
				client->ping_timeout = 0;
				client->ping_threshold = 0;
				client->ping_enabled = 0;
				client->ping_recv_tsms = 0;
			} else {
				if (client->ping_timeout > client->ping_interval) {
					client->ping_timeout = client->ping_interval;
				}
				if (client->ping_threshold <= 0) {
					client->ping_threshold = 0;
				}
				client->ping_recv_tsms = mbus_clock_monotonic() - client->ping_interval;
				client->ping_enabled = 1;
			}
		}
		{
			int i;
			int j;
			struct mbus_json *compressions;
			compressions = mbus_json_get_object(payload, "compressions");
			for (i = 0; i < (int) (sizeof(compression_methods) / sizeof(compression_methods[0])); i++) {
				for (j = 0; j < mbus_json_get_array_size(compressions); j++) {
					if (mbus_json_get_value_string(mbus_json_get_array_item(compressions, j)) == NULL) {
						continue;
					}
					if (strcmp(compression_methods[i].name, mbus_json_get_value_string(mbus_json_get_array_item(compressions, j))) == 0) {
						break;
					}
				}
				if (j < mbus_json_get_array_size(compressions)) {
					break;
				}
			}
			if (i < (int) (sizeof(compression_methods) / sizeof(compression_methods[0]))) {
				mbus_debugf("compression: %s", compression_methods[i].name);
				client_set_compression(mbus_server_method_get_source(method), compression_methods[i].value);
			} else {
				mbus_debugf("compression: %s", "none");
				client_set_compression(mbus_server_method_get_source(method), mbus_compress_method_none);
			}
		}
	}
	mbus_infof("client created");
	mbus_infof("  identifier : %s", client_get_identifier(mbus_server_method_get_source(method)));
	mbus_infof("  compression: %s", mbus_compress_method_string(client_get_compression(mbus_server_method_get_source(method))));
	mbus_infof("  ping");
	mbus_infof("    enabled  : %d", client->ping_enabled);
	mbus_infof("    interval : %d", client->ping_interval);
	mbus_infof("    timeout  : %d", client->ping_timeout);
	mbus_infof("    threshold: %d", client->ping_threshold);
	{
		struct mbus_json *payload;
		struct mbus_json *ping;
		payload = mbus_json_create_object();
		mbus_json_add_string_to_object_cs(payload, "identifier", client_get_identifier(mbus_server_method_get_source(method)));
		mbus_json_add_string_to_object_cs(payload, "compression", mbus_compress_method_string(client_get_compression(mbus_server_method_get_source(method))));
		ping = mbus_json_create_object();
		mbus_json_add_number_to_object_cs(ping, "interval", client->ping_interval);
		mbus_json_add_number_to_object_cs(ping, "timeout", client->ping_timeout);
		mbus_json_add_number_to_object_cs(ping, "threshold", client->ping_threshold);
		mbus_json_add_item_to_object_cs(payload, "ping", ping);
		mbus_server_method_set_result_payload(method, payload);
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
	source = mbus_json_get_string_value(mbus_server_method_get_request_payload(method), "source", NULL);
	event = mbus_json_get_string_value(mbus_server_method_get_request_payload(method), "event", NULL);
	if ((source == NULL) ||
	    (event == NULL)) {
		mbus_errorf("invalid request");
		goto bail;
	}
	rc = client_add_subscription(mbus_server_method_get_source(method), source, event);
	if (rc != 0) {
		mbus_errorf("can not add subscription");
		goto bail;
	}
	rc = server_send_event_subscribed(server, client_get_identifier(mbus_server_method_get_source(method)), source, event);
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
	source = mbus_json_get_string_value(mbus_server_method_get_request_payload(method), "source", NULL);
	event = mbus_json_get_string_value(mbus_server_method_get_request_payload(method), "event", NULL);
	if ((source == NULL) ||
	    (event == NULL)) {
		mbus_errorf("invalid request");
		goto bail;
	}
	rc = client_del_subscription(mbus_server_method_get_source(method), source, event);
	if (rc != 0) {
		mbus_errorf("can not del subscription");
		goto bail;
	}
	rc = server_send_event_unsubscribed(server, client_get_identifier(mbus_server_method_get_source(method)), source, event);
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
	command = mbus_json_get_string_value(mbus_server_method_get_request_payload(method), "command", NULL);
	if (command == NULL) {
		mbus_errorf("invalid request");
		goto bail;
	}
	rc = client_add_command(mbus_server_method_get_source(method), command);
	if (rc != 0) {
		mbus_errorf("can not add subscription");
		goto bail;
	}
	rc = server_send_event_registered(server, client_get_identifier(mbus_server_method_get_source(method)), command);
	if (rc != 0) {
		mbus_errorf("can not send registered event");
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
	command = mbus_json_get_string_value(mbus_server_method_get_request_payload(method), "command", NULL);
	if (command == NULL) {
		mbus_errorf("invalid request");
		goto bail;
	}
	rc = client_del_command(mbus_server_method_get_source(method), command);
	if (rc != 0) {
		mbus_errorf("can not add subscription");
		goto bail;
	}
	rc = server_send_event_unregistered(server, client_get_identifier(mbus_server_method_get_source(method)), command);
	if (rc != 0) {
		mbus_errorf("can not send unregistered event");
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
	destination = mbus_json_get_string_value(mbus_server_method_get_request_payload(method), MBUS_METHOD_TAG_DESTINATION, NULL);
	identifier = mbus_json_get_string_value(mbus_server_method_get_request_payload(method), MBUS_METHOD_TAG_IDENTIFIER, NULL);
	payload = mbus_json_get_object(mbus_server_method_get_request_payload(method), MBUS_METHOD_TAG_PAYLOAD);
	if ((destination == NULL) ||
	    (identifier == NULL) ||
	    (payload == NULL)) {
		mbus_errorf("invalid request");
		goto bail;
	}
	rc = server_send_event_to(server, client_get_identifier(mbus_server_method_get_source(method)), destination, identifier, payload);
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
		mbus_json_add_string_to_object_cs(source, "source", client_get_identifier(client));
		mbus_json_add_string_to_object_cs(source, "address", mbus_socket_fd_get_address(mbus_server_connection_get_fd(client_get_connection(client)), address, sizeof(address)));
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
			mbus_json_add_string_to_object_cs(object, "source", mbus_server_subscription_get_source(subscription));
			mbus_json_add_string_to_object_cs(object, "identifier", mbus_server_subscription_get_event(subscription));
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
			mbus_json_add_string_to_object_cs(object, "identifier", mbus_server_command_get_identifier(command));
		}
	}
	mbus_server_method_set_result_payload(method, clients);
	return 0;
bail:	if (clients != NULL) {
		mbus_json_delete(clients);
	}
	return -1;
}

static int server_handle_command_client (struct mbus_server *server, struct method *method)
{
	char address[MAX(INET_ADDRSTRLEN, INET6_ADDRSTRLEN)];
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
	source = mbus_json_get_string_value(mbus_server_method_get_request_payload(method), "source", NULL);
	if (source == NULL) {
		mbus_errorf("method request source is null");
		goto bail;
	}
	client = server_find_client_by_identifier(server, source);
	if (client == NULL) {
		mbus_errorf("client: %s is not connected", source);
		goto bail;
	}
	result = mbus_json_create_object();
	if (result == NULL) {
		goto bail;
	}
	mbus_json_add_string_to_object_cs(result, "source", client_get_identifier(client));
	mbus_json_add_string_to_object_cs(result, "address", mbus_socket_fd_get_address(mbus_server_connection_get_fd(client_get_connection(client)), address, sizeof(address)));
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
		mbus_json_add_string_to_object_cs(object, "source", mbus_server_subscription_get_source(subscription));
		mbus_json_add_string_to_object_cs(object, "identifier", mbus_server_subscription_get_event(subscription));
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
		mbus_json_add_string_to_object_cs(object, "identifier", mbus_server_command_get_identifier(command));
	}
	mbus_server_method_set_result_payload(method, result);
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
		if (client_get_identifier(client) == NULL) {
			continue;
		}
		source = mbus_json_create_string(client_get_identifier(client));
		if (source == NULL) {
			goto bail;
		}
		mbus_json_add_item_to_array(clients, source);
	}
	mbus_server_method_set_result_payload(method, clients);
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
	source = mbus_json_get_string_value(mbus_server_method_get_request_payload(method), "source", NULL);
	if (source == NULL) {
		mbus_errorf("method request source is null");
		goto bail;
	}
	if (strcmp(source, MBUS_SERVER_IDENTIFIER) == 0) {
		server->running = 0;
	} else {
		TAILQ_FOREACH(client, &server->clients, clients) {
			if (client_get_identifier(client) == NULL) {
				continue;
			}
			if (strcmp(client_get_identifier(client), source) != 0) {
				continue;
			}
			client_set_connection(client, NULL, client_connection_close_code_close_comand);
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
		if (client_get_identifier(client) == NULL) {
			continue;
		}
		if (strcmp(mbus_server_method_get_request_destination(method), client_get_identifier(client)) != 0) {
			continue;
		}
		break;
	}
	if (client == NULL) {
		mbus_errorf("client %s does not exists", mbus_server_method_get_request_destination(method));
		goto bail;
	}
	TAILQ_FOREACH(command, &client->commands, commands) {
		if (strcmp(mbus_server_method_get_request_identifier(method), mbus_server_command_get_identifier(command)) == 0) {
			break;
		}
	}
	if (command == NULL) {
		mbus_errorf("client: %s does not have such command: %s", mbus_server_method_get_request_destination(method), mbus_server_method_get_request_identifier(method));
		goto bail;
	}
	request = mbus_server_method_create_response(MBUS_METHOD_TYPE_COMMAND, client_get_identifier(mbus_server_method_get_source(method)), mbus_server_method_get_request_identifier(method), mbus_server_method_get_request_sequence(method), mbus_server_method_get_request_payload(method));
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
	source = client_get_identifier(mbus_server_method_get_source(method));
	if (source == NULL) {
		mbus_errorf("source is invalid");
		goto bail;
	}
	destination = mbus_json_get_string_value(mbus_server_method_get_request_payload(method), MBUS_METHOD_TAG_DESTINATION, NULL);
	if (destination == NULL) {
		mbus_errorf("destination is invalid");
		goto bail;
	}
	identifier = mbus_json_get_string_value(mbus_server_method_get_request_payload(method), MBUS_METHOD_TAG_IDENTIFIER, NULL);
	if (identifier == NULL) {
		mbus_errorf("identifier is invalid");
		goto bail;
	}
	sequence = mbus_json_get_int_value(mbus_server_method_get_request_payload(method), MBUS_METHOD_TAG_SEQUENCE, -1);
	if (sequence < MBUS_METHOD_SEQUENCE_START ||
	    sequence >= MBUS_METHOD_SEQUENCE_END) {
		mbus_errorf("sequence is invalid");
		goto bail;
	}
	rc = mbus_json_get_int_value(mbus_server_method_get_request_payload(method), MBUS_METHOD_TAG_STATUS, -1);
	TAILQ_FOREACH(client, &server->clients, clients) {
		if (client_get_identifier(client) == NULL) {
			continue;
		}
		if (strcmp(client_get_identifier(client), destination) != 0) {
			continue;
		}
		TAILQ_FOREACH_SAFE(wait, &client->waits, methods, nwait) {
			if (sequence != mbus_server_method_get_request_sequence(wait)) {
				continue;
			}
			if (strcmp(mbus_server_method_get_request_destination(wait), source) != 0) {
				continue;
			}
			if (strcmp(mbus_server_method_get_request_identifier(wait), identifier) != 0) {
				continue;
			}
			TAILQ_REMOVE(&client->waits, wait, methods);
			mbus_server_method_set_result_code(wait, rc);
			mbus_server_method_set_result_payload(wait, mbus_json_duplicate(mbus_json_get_object(mbus_server_method_get_request_payload(method), MBUS_METHOD_TAG_PAYLOAD), 1));
			client_push_result(client, wait);
			break;
		}
		break;
	}
	return 0;
bail:	return -1;
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
		mbus_debugf("handle method: %s, %s, %s", mbus_server_method_get_request_type(method), mbus_server_method_get_request_identifier(method), mbus_server_method_get_request_destination(method));
		TAILQ_REMOVE(&server->methods, method, methods);
		if (strcmp(mbus_server_method_get_request_type(method), MBUS_METHOD_TYPE_COMMAND) == 0) {
			if (strcmp(mbus_server_method_get_request_destination(method), MBUS_SERVER_IDENTIFIER) == 0) {
				response = 1;
				if (strcmp(mbus_server_method_get_request_identifier(method), MBUS_SERVER_COMMAND_CREATE) == 0) {
					rc = server_handle_command_create(server, method);
				} else if (strcmp(mbus_server_method_get_request_identifier(method), MBUS_SERVER_COMMAND_SUBSCRIBE) == 0) {
					rc = server_handle_command_subscribe(server, method);
				} else if (strcmp(mbus_server_method_get_request_identifier(method), MBUS_SERVER_COMMAND_UNSUBSCRIBE) == 0) {
					rc = server_handle_command_unsubscribe(server, method);
				} else if (strcmp(mbus_server_method_get_request_identifier(method), MBUS_SERVER_COMMAND_REGISTER) == 0) {
					rc = server_handle_command_register(server, method);
				} else if (strcmp(mbus_server_method_get_request_identifier(method), MBUS_SERVER_COMMAND_UNREGISTER) == 0) {
					rc = server_handle_command_unregister(server, method);
				} else if (strcmp(mbus_server_method_get_request_identifier(method), MBUS_SERVER_COMMAND_RESULT) == 0) {
					rc = server_handle_command_result(server, method);
				} else if (strcmp(mbus_server_method_get_request_identifier(method), MBUS_SERVER_COMMAND_EVENT) == 0) {
					rc = server_handle_command_event(server, method);
				} else if (strcmp(mbus_server_method_get_request_identifier(method), MBUS_SERVER_COMMAND_STATUS) == 0) {
					rc = server_handle_command_status(server, method);
				} else if (strcmp(mbus_server_method_get_request_identifier(method), MBUS_SERVER_COMMAND_CLIENT) == 0) {
					rc = server_handle_command_client(server, method);
				} else if (strcmp(mbus_server_method_get_request_identifier(method), MBUS_SERVER_COMMAND_CLIENTS) == 0) {
					rc = server_handle_command_clients(server, method);
				} else if (strcmp(mbus_server_method_get_request_identifier(method), MBUS_SERVER_COMMAND_CLOSE) == 0) {
					rc = server_handle_command_close(server, method);
				} else {
					rc = -1;
				}
			} else {
				response = 0;
				rc = server_handle_command_call(server, method);
			}
			if (rc != 0) {
				mbus_errorf("can not execute method type: '%s', destination: '%s', identifier: '%s'",
						mbus_server_method_get_request_type(method),
						mbus_server_method_get_request_destination(method),
						mbus_server_method_get_request_identifier(method));
			}
			if (response == 1 || rc != 0) {
				mbus_debugf("  push to result");
				mbus_server_method_set_result_code(method, rc);
				client_push_result(mbus_server_method_get_source(method), method);
			} else {
				mbus_debugf("  push to wait");
				client_push_wait(mbus_server_method_get_source(method), method);
			}
		}
		if (strcmp(mbus_server_method_get_request_type(method), MBUS_METHOD_TYPE_EVENT) == 0) {
			mbus_debugf("  push to trash");
			rc = server_send_event_to(server, client_get_identifier(mbus_server_method_get_source(method)), mbus_server_method_get_request_destination(method), mbus_server_method_get_request_identifier(method), mbus_server_method_get_request_payload(method));
			if (rc != 0) {
				mbus_errorf("can not send event: %s", mbus_server_method_get_source(method));
			}
			mbus_server_method_destroy(method);
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
	method = mbus_server_method_create_request(client, string);
	if (method == NULL) {
		mbus_errorf("invalid method");
		goto bail;
	}
	if (strcmp(mbus_server_method_get_request_type(method), MBUS_METHOD_TYPE_COMMAND) == 0) {
		rc = server_handle_method_command(server, method);
	} else if (strcmp(mbus_server_method_get_request_type(method), MBUS_METHOD_TYPE_EVENT) == 0) {
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
		mbus_server_method_destroy(method);
	}
	return -1;
}

__attribute__ ((__visibility__("default"))) int mbus_server_run_timeout (struct mbus_server *server, int milliseconds)
{
	int rc;
	char *string;
	unsigned int c;
	unsigned int n;
	unsigned long long current;
	struct client *client;
	struct client *nclient;
	struct client *wclient;
	struct client *nwclient;
	struct method *method;
	struct method *nmethod;
	struct listener *listener;
	enum listener_type listener_type;
	struct connection *connection;
	current = mbus_clock_monotonic();
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
		if (client_get_connection(client) == NULL) {
			continue;
		}
		if (client->ping_interval <= 0) {
			continue;
		}
		if (client->ping_enabled == 0) {
			continue;
		}
		mbus_debugf("    client: %s, recv: %lu, interval: %d, timeout: %d, missed: %d, threshold: %d",
				client_get_identifier(client),
				client->ping_recv_tsms,
				client->ping_interval,
				client->ping_timeout,
				client->ping_missed_count,
				client->ping_threshold);
		if (mbus_clock_after(current, client->ping_recv_tsms + client->ping_interval + client->ping_timeout)) {
			mbus_infof("%s ping timeout: %ld, %ld, %d, %d", client_get_identifier(client), current, client->ping_recv_tsms, client->ping_interval, client->ping_timeout);
			client->ping_missed_count += 1;
			client->ping_recv_tsms = client->ping_recv_tsms + client->ping_interval;
		}
		if (client->ping_missed_count > client->ping_threshold) {
			mbus_errorf("%s missed too many pings, %d > %d. closing connection", client_get_identifier(client), client->ping_missed_count, client->ping_threshold);
			client_set_connection(client, NULL, client_connection_close_code_ping_threshold);
		}
	}
	mbus_debugf("  prepare out buffer");
	TAILQ_FOREACH_SAFE(client, &server->clients, clients, nclient) {
		enum mbus_compress_method compression;
		mbus_debugf("    client: %s", client_get_identifier(client));
		connection = client_get_connection(client);
		if (connection == NULL) {
			continue;
		}
		compression = client_get_compression(client);
		if (client_get_results_count(client) > 0) {
			method = client_pop_result(client);
			if (method == NULL) {
				mbus_errorf("could not pop result from client");
				continue;
			}
			if (strcmp(mbus_server_method_get_request_destination(method), MBUS_SERVER_IDENTIFIER) == 0) {
				if (strcmp(mbus_server_method_get_request_identifier(method), MBUS_SERVER_COMMAND_CREATE) == 0) {
					compression = mbus_compress_method_none;
				}
			}
			string = mbus_server_method_get_result_string(method);
		} else if (client_get_requests_count(client) > 0) {
			method = client_pop_request(client);
			if (method == NULL) {
				mbus_errorf("could not pop request from client");
				continue;
			}
			string = mbus_server_method_get_request_string(method);
		} else if (client_get_events_count(client) > 0) {
			method = client_pop_event(client);
			if (method == NULL) {
				mbus_errorf("could not pop event from client");
				continue;
			}
			string = mbus_server_method_get_request_string(method);
		} else {
			continue;
		}
		if (string == NULL) {
			mbus_errorf("can not build string from method event");
			mbus_server_method_destroy(method);
			goto bail;
		}
		mbus_debugf("      message: %s, %s", mbus_compress_method_string(compression), string);
		rc = mbus_buffer_push_string(client->buffer_out, compression, string);
		if (rc != 0) {
			mbus_errorf("can not push string");
			mbus_server_method_destroy(method);
			goto bail;
		}
		mbus_server_method_destroy(method);
	}
	mbus_debugf("  prepare pollfds (count)");
	n  = 0;
	n += server->listeners.count;
	n += server->clients.count;
	n += server->ws_pollfds.length;
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
	mbus_debugf("  prepare pollfds (fill listeners)");
	{
		int lfd;
		TAILQ_FOREACH(listener, &server->listeners, listeners) {
			lfd = mbus_server_listener_get_fd(listener);
			if (lfd < 0) {
				continue;
			}
			server->pollfds.pollfds[n].events = POLLIN;
			server->pollfds.pollfds[n].revents = 0;
			server->pollfds.pollfds[n].fd = lfd;
			mbus_debugf("    in : %s", mbus_server_listener_get_name(listener));
			n += 1;
		}
	}
	mbus_debugf("  prepare pollfds (fill clients)");
	{
		TAILQ_FOREACH(client, &server->clients, clients) {
			listener = client_get_listener(client);
			if (listener == NULL) {
				continue;
			}
			connection = client_get_connection(client);
			if (connection == NULL) {
				continue;
			}
			listener_type = mbus_server_listener_get_type(listener);
			if (listener_type == listener_type_tcp) {
				server->pollfds.pollfds[n].events = POLLIN;
				server->pollfds.pollfds[n].revents = 0;
				server->pollfds.pollfds[n].fd = mbus_server_connection_get_fd(connection);
				mbus_debugf("    in : %s, connection: %s, %d", client_get_identifier(client), mbus_server_listener_get_name(listener), mbus_server_connection_get_fd(connection));
				if (mbus_buffer_get_length(client->buffer_out) > 0 ||
				    mbus_server_connection_wants_write(connection) > 0) {
					mbus_debugf("    out: %s, connection: %s, %d", client_get_identifier(client), mbus_server_listener_get_name(listener), mbus_server_connection_get_fd(connection));
					server->pollfds.pollfds[n].events |= POLLOUT;
				}
				n += 1;
			} else if (listener_type == listener_type_uds) {
				server->pollfds.pollfds[n].events = POLLIN;
				server->pollfds.pollfds[n].revents = 0;
				server->pollfds.pollfds[n].fd = mbus_server_connection_get_fd(connection);
				mbus_debugf("    in : %s, connection: %s, %d", client_get_identifier(client), mbus_server_listener_get_name(listener), mbus_server_connection_get_fd(connection));
				if (mbus_buffer_get_length(client->buffer_out) > 0 ||
				    mbus_server_connection_wants_write(connection) > 0) {
					mbus_debugf("    out: %s, connection: %s, %d", client_get_identifier(client), mbus_server_listener_get_name(listener), mbus_server_connection_get_fd(connection));
					server->pollfds.pollfds[n].events |= POLLOUT;
				}
				n += 1;
			} else if (listener_type == listener_type_ws) {
				if (mbus_buffer_get_length(client->buffer_out) > 0) {
					mbus_debugf("    out: %s, connection: %s, %d", client_get_identifier(client), mbus_server_listener_get_name(listener), mbus_server_connection_get_fd(connection));
					mbus_server_connection_request_write(connection);
				}
			}
		}
	}
	mbus_debugf("  prepare pollfds (fill ws pollfds)");
	{
		memcpy(&server->pollfds.pollfds[n], server->ws_pollfds.pollfds, sizeof(struct pollfd) * server->ws_pollfds.length);
		n += server->ws_pollfds.length;
	}
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
		if (mbus_debug_level >= mbus_debug_level_debug) {
			TAILQ_FOREACH(listener, &server->listeners, listeners) {
				if (server->pollfds.pollfds[c].fd == mbus_server_listener_get_fd(listener)) {
					mbus_debugf("      listener: %s", mbus_server_listener_get_name(listener));
				}
			}
			TAILQ_FOREACH(client, &server->clients, clients) {
				listener = client_get_listener(client);
				if (listener == NULL) {
					continue;
				}
				connection = client_get_connection(client);
				if (connection == NULL) {
					continue;
				}
				mbus_debugf("      client: %s, connection: %s, %d", client_get_identifier(client), mbus_server_listener_get_name(listener), mbus_server_connection_get_fd(connection));
			}
		}
		{
			TAILQ_FOREACH(listener, &server->listeners, listeners) {
				if (server->pollfds.pollfds[c].fd != mbus_server_listener_get_fd(listener)) {
					continue;
				}
				connection = mbus_server_listener_accept(listener);
				if (connection == NULL) {
					mbus_errorf("can not accept new connection on listener: %s", mbus_server_listener_get_name(listener));
					continue;
				}
				rc = server_client_connection_establish(server, listener, connection);
				if (rc != 0) {
					mbus_errorf("can not establish new connection on listener: %s", mbus_server_listener_get_name(listener));
					mbus_server_connection_close(connection);
					goto bail;
				}
				mbus_infof("accepted new connection on listener: %s", mbus_server_listener_get_name(listener));
			}
		}
		client = server_find_client_by_fd(server, server->pollfds.pollfds[c].fd);
		if (client == NULL) {
			continue;
			mbus_errorf("can not find client by socket");
			goto bail;
		}
		listener = client_get_listener(client);
		if (listener == NULL) {
			mbus_errorf("client listener is invalid");
			goto bail;
		}
		connection = client_get_connection(client);
		if (connection == NULL) {
			mbus_errorf("client_connection is invalid");
			goto bail;
		}
		listener_type = mbus_server_listener_get_type(listener);
		if (listener_type == listener_type_ws) {
			continue;
		}
		if (server->pollfds.pollfds[c].revents & POLLIN) {
			rc = mbus_server_connection_read(connection, client->buffer_in);
			if ((rc <= 0) &&
			    ((errno != EINTR) && (errno != EAGAIN) && (errno != EWOULDBLOCK))) {
				mbus_debugf("can not read data from client");
				mbus_infof("client: '%s' connection reset by peer", client_get_identifier(client));
				client_set_connection(client, NULL, client_connection_close_code_connection_closed);
				continue;
			}
		}
		if (server->pollfds.pollfds[c].revents & POLLOUT) {
			if (mbus_buffer_get_length(client->buffer_out) <= 0) {
				mbus_errorf("logic error");
				goto bail;
			}
			rc = mbus_server_connection_write(connection, client->buffer_out);
			if ((rc <= 0) &&
			    ((errno != EINTR) && (errno != EAGAIN) && (errno != EWOULDBLOCK))) {
				mbus_debugf("can not write string to client");
				mbus_infof("client: '%s' connection reset by server", client_get_identifier(client));
				client_set_connection(client, NULL, client_connection_close_code_connection_closed);
				continue;
			}
		}
		if (server->pollfds.pollfds[c].revents & (POLLERR | POLLHUP | POLLNVAL)) {
			mbus_infof("client: '%s' connection reset by server", client_get_identifier(client));
			client_set_connection(client, NULL, client_connection_close_code_connection_closed);
			continue;
		}
	}
out:
	{
		TAILQ_FOREACH(listener, &server->listeners, listeners) {
			if (mbus_server_listener_get_type(listener) == listener_type_ws) {
				mbus_server_listener_service(listener);
			}
		}
	}
	TAILQ_FOREACH(client, &server->clients, clients) {
		uint8_t *ptr;
		uint8_t *end;
		uint8_t *data;
		uint32_t expected;
		uint32_t uncompressed;
		if (mbus_buffer_get_length(client->buffer_in) < sizeof(expected)) {
			continue;
		}
		mbus_debugf("%s buffer.in:", client_get_identifier(client));
		mbus_debugf("  length  : %d", mbus_buffer_get_length(client->buffer_in));
		mbus_debugf("  size    : %d", mbus_buffer_get_size(client->buffer_in));
		while (1) {
			char *string;
			ptr = mbus_buffer_get_base(client->buffer_in);
			end = ptr + mbus_buffer_get_length(client->buffer_in);
			if (end - ptr < (int32_t) sizeof(expected)) {
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
			mbus_debugf("        message: %s, e: %d, u: %d, '%.*s'", mbus_compress_method_string(client_get_compression(client)), expected, uncompressed, uncompressed, data);
			string = _strndup((char *) data, uncompressed);
			if (string == NULL) {
				mbus_errorf("can not allocate memory, closing client: '%s' connection", client_get_identifier(client));
				client_set_connection(client, NULL, client_connection_close_code_internal_error);
				goto bail;
			}
			rc = server_handle_method(server, client, string);
			if (rc != 0) {
				mbus_errorf("can not handle request, closing client: '%s' connection", client_get_identifier(client));
				client_set_connection(client, NULL, client_connection_close_code_internal_error);
				free(string);
				if (data != ptr) {
					free(data);
				}
				break;
			}
			rc = mbus_buffer_shift(client->buffer_in, sizeof(uint32_t) + expected);
			if (rc != 0) {
				mbus_errorf("can not shift in, closing client: '%s' connection", client_get_identifier(client));
				client_set_connection(client, NULL, client_connection_close_code_internal_error);
				free(string);
				if (data != ptr) {
					free(data);
				}
				goto bail;
			}
			free(string);
			if (data != ptr) {
				free(data);
			}
		}
	}
	rc = server_handle_methods(server);
	if (rc != 0) {
		mbus_errorf("can not handle methods");
		goto bail;
	}
	TAILQ_FOREACH(client, &server->clients, clients) {
		if (client_get_connection(client) == NULL) {
			continue;
		}
		if ((client_get_status(client) & client_status_accepted) != 0) {
			continue;
		}
		client_set_status(client, client_get_status(client) | client_status_accepted);
		mbus_infof("client: '%s' accepted to server", client_get_identifier(client));
	}
	TAILQ_FOREACH(client, &server->clients, clients) {
		if (client_get_connection(client) == NULL) {
			continue;
		}
		if (client_get_identifier(client) == NULL) {
			continue;
		}
		if ((client_get_status(client) & client_status_accepted) == 0) {
			continue;
		}
		if ((client_get_status(client) & client_status_connected) != 0) {
			continue;
		}
		client_set_status(client, client_get_status(client) | client_status_connected);
		mbus_infof("client: '%s' connected to server", client_get_identifier(client));
		rc = server_send_event_connected(server, client);
		if (rc != 0) {
			mbus_errorf("can not send connected event");
			goto bail;
		}
	}
	TAILQ_FOREACH_SAFE(client, &server->clients, clients, nclient) {
		if (client_get_connection(client) != NULL) {
			continue;
		}
		mbus_infof("client: '%s' disconnected from server", client_get_identifier(client));
		TAILQ_REMOVE(&server->clients, client, clients);
		TAILQ_FOREACH_SAFE(method, &server->methods, methods, nmethod) {
			if (mbus_server_method_get_source(method) != client) {
				continue;
			}
			if (strcmp(mbus_server_method_get_request_type(method), MBUS_METHOD_TYPE_EVENT) == 0) {
				continue;
			}
			TAILQ_REMOVE(&server->methods, method, methods);
			mbus_server_method_destroy(method);
		}
		TAILQ_FOREACH_SAFE(wclient, &server->clients, clients, nwclient) {
			TAILQ_FOREACH_SAFE(method, &wclient->waits, methods, nmethod) {
				if (client_get_identifier(client) != NULL &&
				    strcmp(mbus_server_method_get_request_destination(method), client_get_identifier(client)) == 0) {
					TAILQ_REMOVE(&wclient->waits, method, methods);
					mbus_server_method_set_result_code(method, -1);
					client_push_result(wclient, method);
				}
			}
		}
		if ((client_get_status(client) & client_status_connected) != 0) {
			rc = server_send_event_disconnected(server, client_get_identifier(client), client_get_connection_close_code(client));
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

__attribute__ ((__visibility__("default"))) int mbus_server_run (struct mbus_server *server)
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

__attribute__ ((__visibility__("default"))) void mbus_server_destroy (struct mbus_server *server)
{
	struct client *client;
	struct method *method;
	struct listener *listener;
	if (server == NULL) {
		return;
	}
	mbus_infof("destroying server");
	while (server->methods.tqh_first != NULL) {
		method = server->methods.tqh_first;
		TAILQ_REMOVE(&server->methods, server->methods.tqh_first, methods);
		mbus_server_method_destroy(method);
	}
	while (server->listeners.tqh_first != NULL) {
		listener = server->listeners.tqh_first;
		TAILQ_REMOVE(&server->listeners, server->listeners.tqh_first, listeners);
		mbus_server_listener_destroy(listener);
	}
	while (server->clients.tqh_first != NULL) {
		client = server->clients.tqh_first;
		TAILQ_REMOVE(&server->clients, server->clients.tqh_first, clients);
		client_destroy(client);
	}
	if (server->pollfds.pollfds != NULL) {
		free(server->pollfds.pollfds);
	}
	if (server->ws_pollfds.pollfds != NULL) {
		free(server->ws_pollfds.pollfds);
	}
	if (server->password != NULL) {
	        free(server->password);
	}
#if defined(SSL_ENABLE) && (SSL_ENABLE == 1)
	EVP_cleanup();
#endif
	free(server);
}

__attribute__ ((__visibility__("default"))) int mbus_server_options_default (struct mbus_server_options *options)
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
	options->udss.certificate = MBUS_SERVER_UDSS_CERTIFICATE;
	options->udss.privatekey = MBUS_SERVER_UDSS_PRIVATEKEY;

#if defined(WS_ENABLE) && (WS_ENABLE == 1)
	options->wss.enabled = MBUS_SERVER_WSS_ENABLE;
	options->wss.address = MBUS_SERVER_WSS_ADDRESS;
	options->wss.port = MBUS_SERVER_WSS_PORT;
	options->wss.certificate = MBUS_SERVER_WSS_CERTIFICATE;
	options->wss.privatekey = MBUS_SERVER_WSS_PRIVATEKEY;
#endif
#endif

	options->password = NULL;

	return 0;
bail:	return -1;
}

__attribute__ ((__visibility__("default"))) int mbus_server_options_from_argv (struct mbus_server_options *options, int argc, char *argv[])
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
				options->udss.certificate = optarg;
				break;
			case OPTION_SERVER_UDSS_PRIVATEKEY:
				options->udss.privatekey = optarg;
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
                        case OPTION_SERVER_PASSWORD:
                                options->password = optarg;
                                break;
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

__attribute__ ((__visibility__("default"))) struct mbus_server * mbus_server_create (int argc, char *argv[])
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

__attribute__ ((__visibility__("default"))) struct mbus_server * mbus_server_create_with_options (const struct mbus_server_options *_options)
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
	mbus_infof("using mbus version '%s, %s'", mbus_version_git_commit(), mbus_version_git_revision());
#if defined(SSL_ENABLE) && (SSL_ENABLE == 1)
	mbus_infof("using openssl version '%s'", SSLeay_version(SSLEAY_VERSION));
#endif

	if (server->options.password != NULL) {
	        server->password = strdup(server->options.password);
	        if (server->password == NULL) {
	                mbus_errorf("can not allocate memory");
	                goto bail;
	        }
	} else {
	        server->password = NULL;
	}

	if (server->options.tcp.enabled == 1) {
		struct listener *listener;
		struct listener_tcp_options listener_tcp_options;
		memset(&listener_tcp_options, 0, sizeof(struct listener_tcp_options));
		listener_tcp_options.name        = "tcp";
		listener_tcp_options.address     = server->options.tcp.address;
		listener_tcp_options.port        = server->options.tcp.port;
		listener_tcp_options.certificate = NULL;
		listener_tcp_options.privatekey  = NULL;
		listener = mbus_server_listener_tcp_create(&listener_tcp_options);
		if (listener == NULL) {
			mbus_errorf("can not create listener: tcp");
			goto bail;
		}
		TAILQ_INSERT_TAIL(&server->listeners, listener, listeners);
		mbus_infof("listening from: '%s:%s:%d'", "tcp", server->options.tcp.address, server->options.tcp.port);
	}
	if (server->options.uds.enabled == 1) {
		struct listener *listener;
		struct listener_uds_options listener_uds_options;
		memset(&listener_uds_options, 0, sizeof(struct listener_uds_options));
		listener_uds_options.name        = "uds";
		listener_uds_options.address     = server->options.uds.address;
		listener_uds_options.port        = server->options.uds.port;
		listener_uds_options.certificate = NULL;
		listener_uds_options.privatekey  = NULL;
		listener = mbus_server_listener_uds_create(&listener_uds_options);
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
		struct listener_ws_options listener_ws_options;
		memset(&listener_ws_options, 0, sizeof(struct listener_ws_options));
		listener_ws_options.name        = "ws";
		listener_ws_options.address     = server->options.ws.address;
		listener_ws_options.port        = server->options.ws.port;
		listener_ws_options.certificate = NULL;
		listener_ws_options.privatekey  = NULL;
		listener_ws_options.callbacks.connection_established = server_listener_ws_callback_connection_established;
		listener_ws_options.callbacks.connection_receive     = server_listener_ws_callback_connection_receive;
		listener_ws_options.callbacks.connection_writable    = server_listener_ws_callback_connection_writable;
		listener_ws_options.callbacks.connection_closed      = server_listener_ws_callback_connection_closed;
		listener_ws_options.callbacks.poll_add               = server_listener_ws_callback_poll_add;
		listener_ws_options.callbacks.poll_mod               = server_listener_ws_callback_poll_mod;
		listener_ws_options.callbacks.poll_del               = server_listener_ws_callback_poll_del;
		listener_ws_options.callbacks.context                = server;
		listener = mbus_server_listener_ws_create(&listener_ws_options);
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
		struct listener_tcp_options listener_tcp_options;
		memset(&listener_tcp_options, 0, sizeof(struct listener_tcp_options));
		listener_tcp_options.name        = "tcps";
		listener_tcp_options.address     = server->options.tcps.address;
		listener_tcp_options.port        = server->options.tcps.port;
		listener_tcp_options.certificate = server->options.tcps.certificate;
		listener_tcp_options.privatekey  = server->options.tcps.privatekey;
		listener = mbus_server_listener_tcp_create(&listener_tcp_options);
		if (listener == NULL) {
			mbus_errorf("can not create listener '%s:%s:%d'", "tcps", server->options.tcps.address, server->options.tcps.port);
		} else {
			TAILQ_INSERT_TAIL(&server->listeners, listener, listeners);
			mbus_infof("listening from: '%s:%s:%d'", "tcps", server->options.tcps.address, server->options.tcps.port);
		}
	}
	if (server->options.udss.enabled == 1) {
		struct listener *listener;
		struct listener_uds_options listener_uds_options;
		memset(&listener_uds_options, 0, sizeof(struct listener_uds_options));
		listener_uds_options.name        = "udss";
		listener_uds_options.address     = server->options.udss.address;
		listener_uds_options.port        = server->options.udss.port;
		listener_uds_options.certificate = server->options.udss.certificate;
		listener_uds_options.privatekey  = server->options.udss.privatekey;
		listener = mbus_server_listener_uds_create(&listener_uds_options);
		if (listener == NULL) {
			mbus_errorf("can not create listener '%s:%s:%d'", "udss", server->options.udss.address, server->options.udss.port);
		} else {
			TAILQ_INSERT_TAIL(&server->listeners, listener, listeners);
			mbus_infof("listening from: '%s:%s:%d'", "udss", server->options.udss.address, server->options.udss.port);
		}
	}
#if defined(WS_ENABLE) && (WS_ENABLE == 1)
#if defined(SSL_ENABLE) && (SSL_ENABLE == 1)
	if (server->options.wss.enabled == 1) {
		struct listener *listener;
		struct listener_ws_options listener_ws_options;
		memset(&listener_ws_options, 0, sizeof(struct listener_ws_options));
		listener_ws_options.name        = "wss";
		listener_ws_options.address     = server->options.wss.address;
		listener_ws_options.port        = server->options.wss.port;
		listener_ws_options.certificate = server->options.wss.certificate;
		listener_ws_options.privatekey  = server->options.wss.privatekey;
		listener_ws_options.callbacks.connection_established = server_listener_ws_callback_connection_established;
		listener_ws_options.callbacks.connection_receive     = server_listener_ws_callback_connection_receive;
		listener_ws_options.callbacks.connection_writable    = server_listener_ws_callback_connection_writable;
		listener_ws_options.callbacks.connection_closed      = server_listener_ws_callback_connection_closed;
		listener_ws_options.callbacks.poll_add               = server_listener_ws_callback_poll_add;
		listener_ws_options.callbacks.poll_mod               = server_listener_ws_callback_poll_mod;
		listener_ws_options.callbacks.poll_del               = server_listener_ws_callback_poll_del;
		listener_ws_options.callbacks.context                = server;
		listener = mbus_server_listener_ws_create(&listener_ws_options);
		if (listener == NULL) {
			mbus_errorf("can not create listener '%s:%s:%d'", "wss", server->options.wss.address, server->options.wss.port);
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

__attribute__ ((__visibility__("default"))) int mbus_server_tcp_enabled (struct mbus_server *server)
{
	if (server == NULL) {
		mbus_errorf("server is null");
		goto bail;
	}
	return server->options.tcp.enabled;
bail:	return -1;
}

__attribute__ ((__visibility__("default"))) const char * mbus_server_tcp_address (struct mbus_server *server)
{
	if (server == NULL) {
		mbus_errorf("server is null");
		goto bail;
	}
	return server->options.tcp.address;
bail:	return NULL;
}

__attribute__ ((__visibility__("default"))) int mbus_server_tcp_port (struct mbus_server *server)
{
	if (server == NULL) {
		mbus_errorf("server is null");
		goto bail;
	}
	return server->options.tcp.port;
bail:	return -1;
}

__attribute__ ((__visibility__("default"))) int mbus_server_uds_enabled (struct mbus_server *server)
{
	if (server == NULL) {
		mbus_errorf("server is null");
		goto bail;
	}
	return server->options.uds.enabled;
bail:	return -1;
}

__attribute__ ((__visibility__("default"))) const char * mbus_server_uds_address (struct mbus_server *server)
{
	if (server == NULL) {
		mbus_errorf("server is null");
		goto bail;
	}
	return server->options.uds.address;
bail:	return NULL;
}

__attribute__ ((__visibility__("default"))) int mbus_server_uds_port (struct mbus_server *server)
{
	if (server == NULL) {
		mbus_errorf("server is null");
		goto bail;
	}
	return server->options.uds.port;
bail:	return -1;
}

__attribute__ ((__visibility__("default"))) int mbus_server_ws_enabled (struct mbus_server *server)
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

__attribute__ ((__visibility__("default"))) const char * mbus_server_ws_address (struct mbus_server *server)
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

__attribute__ ((__visibility__("default"))) int mbus_server_ws_port (struct mbus_server *server)
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

__attribute__ ((__visibility__("default"))) int mbus_server_tcps_port (struct mbus_server *server)
{
	if (server == NULL) {
		mbus_errorf("server is null");
		goto bail;
	}
	return server->options.tcps.port;
bail:	return -1;
}

__attribute__ ((__visibility__("default"))) int mbus_server_udss_enabled (struct mbus_server *server)
{
	if (server == NULL) {
		mbus_errorf("server is null");
		goto bail;
	}
	return server->options.udss.enabled;
bail:	return -1;
}

__attribute__ ((__visibility__("default"))) const char * mbus_server_udss_address (struct mbus_server *server)
{
	if (server == NULL) {
		mbus_errorf("server is null");
		goto bail;
	}
	return server->options.udss.address;
bail:	return NULL;
}

__attribute__ ((__visibility__("default"))) int mbus_server_udss_port (struct mbus_server *server)
{
	if (server == NULL) {
		mbus_errorf("server is null");
		goto bail;
	}
	return server->options.udss.port;
bail:	return -1;
}

__attribute__ ((__visibility__("default"))) int mbus_server_wss_enabled (struct mbus_server *server)
{
	if (server == NULL) {
		mbus_errorf("server is null");
		goto bail;
	}
#if defined(WS_ENABLE) && (WS_ENABLE == 1)
	return server->options.wss.enabled;
#endif
bail:	return -1;
}

__attribute__ ((__visibility__("default"))) const char * mbus_server_wss_address (struct mbus_server *server)
{
	if (server == NULL) {
		mbus_errorf("server is null");
		goto bail;
	}
#if defined(WS_ENABLE) && (WS_ENABLE == 1)
	return server->options.wss.address;
#endif
bail:	return NULL;
}

__attribute__ ((__visibility__("default"))) int mbus_server_wss_port (struct mbus_server *server)
{
	if (server == NULL) {
		mbus_errorf("server is null");
		goto bail;
	}
#if defined(WS_ENABLE) && (WS_ENABLE == 1)
	return server->options.wss.port;
#endif
bail:	return -1;
}
