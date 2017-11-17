
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
#include <unistd.h>
#include <getopt.h>

#define MBUS_DEBUG_NAME	"app-listener"

#include "mbus/debug.h"
#include "mbus/json.h"
#include "mbus/method.h"
#include "mbus/tailq.h"
#include "mbus/client.h"
#include "mbus/server.h"

TAILQ_HEAD(subscriptions, subscription);
struct subscription {
	TAILQ_ENTRY(subscription) subscriptions;
	char *source;
	char *event;
};

struct arg {
	struct subscriptions *subscriptions;
};

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

static void mbus_client_callback_connect (struct mbus_client *client, void *context, enum mbus_client_connect_status status)
{
	(void) client;
	(void) context;
	switch (status) {
		case mbus_client_connect_status_success:
			fprintf(stdout, "connected\n");
			break;
		case mbus_client_connect_status_generic_error:
			fprintf(stdout, "can not connect\n");
			break;
		case mbus_client_connect_status_invalid_protocol:
			fprintf(stdout, "can not connect: invalid protocol\n");
			break;
		case mbus_client_connect_status_internal_error:
			fprintf(stdout, "can not connect: internal error\n");
			break;
		case mbus_client_connect_status_connection_refused:
			fprintf(stdout, "can not connect: connection refused\n");
			break;
		case mbus_client_connect_status_server_unavailable:
			fprintf(stdout, "can not connect: server unavailable\n");
			break;
	}
}

static void mbus_client_callback_create (struct mbus_client *client, void *context, enum mbus_client_create_status status)
{
	int rc;
	struct arg *arg = context;
	if (status == mbus_client_create_status_success) {
		if (arg->subscriptions->count > 0) {
			struct subscription *subscription;
			TAILQ_FOREACH(subscription, arg->subscriptions, subscriptions) {
				rc = mbus_client_subscribe(client, subscription->source, subscription->event);
				if (rc != 0) {
					fprintf(stderr, "can not subscribe to event\n");
					goto bail;
				}
			}
		} else {
			rc = mbus_client_subscribe(client, MBUS_METHOD_EVENT_SOURCE_ALL, MBUS_METHOD_EVENT_IDENTIFIER_ALL);
			if (rc != 0) {
				fprintf(stderr, "can not subscribe to events\n");
				goto bail;
			}
		}
	}
	return;
bail:	return;
}

static void mbus_client_callback_message (struct mbus_client *client, void *context, struct mbus_client_message *message)
{
	char *string;
	(void) client;
	(void) context;
	string = mbus_json_print(mbus_client_message_event_payload(message));
	if (string == NULL) {
		fprintf(stderr, "can not allocate memory\n");
	} else {
		fprintf(stdout, "%s.%s: %s\n", mbus_client_message_event_source(message), mbus_client_message_event_identifier(message), string);
		free(string);
	}
}

#define OPTION_HELP		0x100
#define OPTION_SUBSCRIBE		0x101
static struct option longopts[] = {
	{ "help",			no_argument,		NULL,	OPTION_HELP },
	{ "subscribe",			required_argument,	NULL,	OPTION_SUBSCRIBE },
	{ NULL,				0,			NULL,	0 },
};

static void usage (void)
{
	fprintf(stdout, "mbus listener arguments:\n");
	fprintf(stdout, "  --subscribe: subscribe to identifier\n");
	fprintf(stdout, "  --help     : this text\n");
	fprintf(stdout, "  --mbus-help: mbus help text\n");
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

	struct arg arg;
	struct mbus_client *client;
	struct mbus_client_options options;

	client = NULL;
	TAILQ_INIT(&subscriptions);
	memset(&arg, 0, sizeof(struct arg));

	_argc = 0;
	_argv = NULL;
	_optind = optind;

	optind = 1;
	_argv = malloc(sizeof(char *) * argc);
	for (_argc = 0; _argc < argc; _argc++) {
		_argv[_argc] = argv[_argc];
	}

	while ((c = getopt_long(_argc, _argv, ":", longopts, NULL)) != -1) {
		switch (c) {
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

	rc = mbus_client_options_from_argv(&options, argc, argv);
	if (rc != 0) {
		fprintf(stderr, "can not parse options\n");
		goto bail;
	}
	options.callbacks.connect = mbus_client_callback_connect;
	options.callbacks.create = mbus_client_callback_create;
	options.callbacks.message = mbus_client_callback_message;
	arg.subscriptions = &subscriptions;
	options.callbacks.context = &arg;
	client = mbus_client_create(&options);
	if (client == NULL) {
		fprintf(stderr, "can not create client\n");
		goto bail;
	}
	rc = mbus_client_connect(client);
	if (rc != 0) {
		fprintf(stderr, "can not connect client\n");
		goto bail;
	}

	while (1) {
		rc = mbus_client_run(client, MBUS_CLIENT_DEFAULT_RUN_TIMEOUT);
		if (rc != 0) {
			fprintf(stderr, "client run failed\n");
			goto bail;
		}
	}

	TAILQ_FOREACH_SAFE(subscription, &subscriptions, subscriptions, nsubscription) {
		TAILQ_REMOVE(&subscriptions, subscription, subscriptions);
		subscription_destroy(subscription);
	}

	mbus_client_destroy(client);
	free(_argv);
	return 0;
bail:	if (client != NULL) {
		mbus_client_destroy(client);
	}
	TAILQ_FOREACH_SAFE(subscription, &subscriptions, subscriptions, nsubscription) {
		TAILQ_REMOVE(&subscriptions, subscription, subscriptions);
		subscription_destroy(subscription);
	}
	if (_argv != NULL) {
		free(_argv);
	}
	return -1;
}
