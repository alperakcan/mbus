
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
#include <signal.h>
#include <poll.h>

#define MBUS_DEBUG_NAME	"app-benchmark"

#include "mbus/debug.h"
#include "mbus/json.h"
#include "mbus/method.h"
#include "mbus/tailq.h"
#include "mbus/client.h"
#include "mbus/server.h"

#if !defined(MIN)
#define MIN(a, b)	(((a) < (b)) ? (a) : (b))
#endif
#if !defined(MAX)
#define MAX(a, b)	(((a) > (b)) ? (a) : (b))
#endif

TAILQ_HEAD(subscriptions, subscription);
struct subscription {
	TAILQ_ENTRY(subscription) subscriptions;
	char *source;
	char *event;
};

TAILQ_HEAD(clients, client);
struct client {
	TAILQ_ENTRY(client) clients;
	struct mbus_client *client;
	struct subscriptions *subscriptions;
	int connected;
	int disconnected;
};

static void mbus_client_callback_connect (struct mbus_client *mbus_client, void *context, enum mbus_client_connect_status status)
{
	int rc;
	struct subscription *subscription;
	struct mbus_client_subscribe_options subscribe_options;

	struct client *client = context;

	(void) mbus_client;

	if (status == mbus_client_connect_status_success) {
		client->connected = 1;
		TAILQ_FOREACH(subscription, client->subscriptions, subscriptions) {
			rc = mbus_client_subscribe_options_default(&subscribe_options);
			if (rc != 0) {
				fprintf(stderr, "can not get default subscribe options\n");
				goto bail;
			}
			subscribe_options.source = subscription->source;
			subscribe_options.event = subscription->event;
			rc = mbus_client_subscribe_with_options(client->client, &subscribe_options);
			if (rc != 0) {
				fprintf(stderr, "can not subscribe to event\n");
				goto bail;
			}
		}
	} else {
		fprintf(stderr, "connect: %s\n", mbus_client_connect_status_string(status));
		if (mbus_client_get_options(client->client)->connect_interval <= 0) {
			client->connected = -1;
		}
	}

	return;
bail:	return;
}

static void mbus_client_callback_disconnect (struct mbus_client *mbus_client, void *context, enum mbus_client_disconnect_status status)
{
	struct client *client = context;
	(void) mbus_client;
	(void) status;
	if (mbus_client_get_options(client->client)->connect_interval <= 0) {
		client->disconnected = 1;
	}
}

static void mbus_client_callback_message (struct mbus_client *mbus_client, void *context, struct mbus_client_message_event *message)
{
	struct client *client = context;
	(void) mbus_client;
	(void) client;
	(void) message;
}

static void client_destroy (struct client *client)
{
	if (client == NULL) {
		return;
	}
	if (client->client != NULL) {
		mbus_client_destroy(client->client);
	}
	free(client);
}

static struct client * client_create (struct mbus_client_options *options, struct subscriptions *subscriptions)
{
	struct client *client;
	client = malloc(sizeof(struct client));
	if (client == NULL) {
		fprintf(stderr, "can not allocate memory\n");
		goto bail;
	}
	memset(client, 0, sizeof(struct client));
	options->callbacks.connect    = mbus_client_callback_connect;
	options->callbacks.disconnect = mbus_client_callback_disconnect;
	options->callbacks.message    = mbus_client_callback_message;
	options->callbacks.context    = client;
	client->client = mbus_client_create(options);
	if (client->client == NULL) {
		fprintf(stderr, "can not client mbus client\n");
		goto bail;
	}
	client->subscriptions = subscriptions;
	return client;
bail:	if (client != NULL) {
		client_destroy(client);
	}
	return NULL;
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

static struct subscription * subscription_create (const char *topic)
{
	struct subscription *subscription;
	subscription = NULL;
	if (topic == NULL) {
		fprintf(stderr, "topic is invalid\n");
		goto bail;
	}
	subscription = malloc(sizeof(struct subscription));
	if (subscription == NULL) {
		fprintf(stderr, "can not allocate memory\n");
		goto bail;
	}
	memset(subscription, 0, sizeof(struct subscription));
	subscription->source = strdup(MBUS_METHOD_EVENT_SOURCE_ALL);
	if (subscription->source == NULL) {
		fprintf(stderr, "can not allocate memory\n");
		goto bail;
	}
	subscription->event = strdup(topic);
	if (subscription->event == NULL) {
		fprintf(stderr, "can not allocate memory\n");
		goto bail;
	}
	return subscription;
bail:	if (subscription != NULL) {
		subscription_destroy(subscription);
	}
	return NULL;
}

#define OPTION_HELP		'h'
#define OPTION_CLIENTS		'c'
#define OPTION_SUBSCRIBE	's'
static struct option longopts[] = {
	{ "help",			no_argument,		NULL,	OPTION_HELP },
	{ "clients",			required_argument,	NULL,	OPTION_CLIENTS },
	{ "subscribe",			required_argument,	NULL,	OPTION_SUBSCRIBE },
	{ NULL,				0,			NULL,	0 },
};

static volatile int g_running;

static void signal_handler (int signal)
{
	(void) signal;
	g_running = 0;
}

static void usage (void)
{
	fprintf(stdout, "mbus benchmark arguments:\n");
	fprintf(stdout, "  -c, --clients   : number of clients (default: 1)\n");
	fprintf(stdout, "  -s, --subscribe : event identifier to subscribe (default: all)\n");
	fprintf(stdout, "  -h, --help      : this text\n");
	fprintf(stdout, "  --mbus-help     : mbus help text\n");
	mbus_client_usage();
}

int main (int argc, char *argv[])
{
	int c;
	int rc;

	int _argc;
	char **_argv;
	int _optind;

	struct subscription *subscription;
	struct subscription *nsubscription;
	struct subscriptions subscriptions;

	struct client *client;
	struct client *nclient;
	struct clients clients;
	struct mbus_client_options mbus_client_options;

	int i;
	int nclients;
	int timeout;
	struct pollfd *pollfds;

	int all_connected;

	pollfds = NULL;
	nclients = 1;
	TAILQ_INIT(&clients);
	TAILQ_INIT(&subscriptions);

	_argc = 0;
	_argv = NULL;
	_optind = optind;

	g_running = 1;
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

	optind = 1;
	_argv = malloc(sizeof(char *) * argc);
	if (_argv == NULL) {
		fprintf(stderr, "can not allocate memory\n");
		goto bail;
	}
	for (_argc = 0; _argc < argc; _argc++) {
		_argv[_argc] = argv[_argc];
	}

	while ((c = getopt_long(_argc, _argv, ":c:s:h", longopts, NULL)) != -1) {
		switch (c) {
			case OPTION_CLIENTS:
				nclients = atoi(optarg);
				break;
			case OPTION_SUBSCRIBE:
				subscription = subscription_create(optarg);
				if (subscription == NULL) {
					fprintf(stderr, "can not create subscription\n");
					goto bail;
				}
				TAILQ_INSERT_TAIL(&subscriptions, subscription, subscriptions);
				break;
			case OPTION_HELP:
				usage();
				goto bail;
		}
	}

	optind = _optind;

	rc = mbus_client_options_default(&mbus_client_options);
	if (rc != 0) {
		fprintf(stderr, "can not get default options\n");
		goto bail;
	}
	rc = mbus_client_options_from_argv(&mbus_client_options, argc, argv);
	if (rc != 0) {
		fprintf(stderr, "can not parse options\n");
		goto bail;
	}

	if (subscriptions.count == 0) {
		subscription = subscription_create(MBUS_METHOD_EVENT_IDENTIFIER_ALL);
		if (subscription == NULL) {
			fprintf(stderr, "can not create subscription\n");
			goto bail;
		}
		TAILQ_INSERT_TAIL(&subscriptions, subscription, subscriptions);
	}
	for (i = 0; i < nclients; i++) {
		client = client_create(&mbus_client_options, &subscriptions);
		if (client == NULL) {
			fprintf(stderr, "can not create client\n");
			goto bail;
		}
		TAILQ_INSERT_TAIL(&clients, client, clients);
	}

	pollfds = malloc(sizeof(struct pollfd) * (clients.count * 2));
	if (pollfds == NULL) {
		fprintf(stderr, "can not allocate memory\n");
		goto bail;
	}

	TAILQ_FOREACH_SAFE(client, &clients, clients, nclient) {
		rc = mbus_client_connect(client->client);
		if (rc != 0) {
			fprintf(stderr, "client connect failed\n");
			goto bail;
		}
	}

	all_connected = 0;
	while (1) {
		if (all_connected == 0) {
			TAILQ_FOREACH_SAFE(client, &clients, clients, nclient) {
				if (client->connected <= 0) {
					break;
				}
			}
			if (client == NULL) {
				fprintf(stderr, "clients: all connected\n");
				all_connected = 1;
			}
		}

		i = 0;
		timeout = 1000;
		TAILQ_FOREACH_SAFE(client, &clients, clients, nclient) {
			pollfds[i + 0].fd = mbus_client_get_wakeup_fd(client->client);
			pollfds[i + 0].events = mbus_client_get_wakeup_fd_events(client->client);
			pollfds[i + 0].revents = 0;

			pollfds[i + 1].fd = mbus_client_get_connection_fd(client->client);
			pollfds[i + 1].events = mbus_client_get_connection_fd_events(client->client);
			pollfds[i + 1].revents = 0;

			timeout = MIN(timeout, mbus_client_get_run_timeout(client->client));
			i += 2;
		}

		rc = poll(pollfds, i, timeout);
		if (rc < 0) {
			fprintf(stderr, "poll failed with: %d\n", rc);
			goto bail;
		}

		TAILQ_FOREACH_SAFE(client, &clients, clients, nclient) {
			rc = mbus_client_run(client->client, 0);
			if (rc != 0) {
				fprintf(stderr, "client run failed\n");
				goto bail;
			}
		}
	}

	TAILQ_FOREACH_SAFE(subscription, &subscriptions, subscriptions, nsubscription) {
		TAILQ_REMOVE(&subscriptions, subscription, subscriptions);
		subscription_destroy(subscription);
	}
	TAILQ_FOREACH_SAFE(client, &clients, clients, nclient) {
		TAILQ_REMOVE(&clients, client, clients);
		client_destroy(client);
	}
	free(pollfds);
	free(_argv);
	return 0;
bail:	TAILQ_FOREACH_SAFE(subscription, &subscriptions, subscriptions, nsubscription) {
		TAILQ_REMOVE(&subscriptions, subscription, subscriptions);
		subscription_destroy(subscription);
	}
	TAILQ_FOREACH_SAFE(client, &clients, clients, nclient) {
		TAILQ_REMOVE(&clients, client, clients);
		client_destroy(client);
	}
	if (_argv != NULL) {
		free(_argv);
	}
	if (pollfds != NULL) {
		free(pollfds);
	}
	return -1;
}
