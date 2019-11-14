
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
#include <time.h>
#include <poll.h>

#define MBUS_DEBUG_NAME	"app-benchmark"

#include "mbus/debug.h"
#include "mbus/json.h"
#include "mbus/clock.h"
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
        TAILQ_ENTRY(subscription) list;
        char *source;
        char *event;
};

TAILQ_HEAD(publishs, publish);
struct publish {
        TAILQ_ENTRY(publish) list;
        int interval;
        char *event;
        char *payload;
        struct mbus_json *payload_json;
};

TAILQ_HEAD(commands, command);
struct command {
        TAILQ_ENTRY(command) list;
        char *command;
};

TAILQ_HEAD(client_publishs, client_publish);
struct client_publish {
        TAILQ_ENTRY(client_publish) list;
        struct publish *publish;
        int timeout;
};

TAILQ_HEAD(clients, client);
struct client {
	TAILQ_ENTRY(client) list;
	struct mbus_client *client;
	struct subscriptions *subscriptions;
        struct commands *commands;
        struct client_publishs publishs;
        int keepalive;
	int connected;
	int disconnected;
	unsigned long long processed_at;
};

#define OPTION_HELP                     'h'
#define OPTION_CLIENTS                  'c'
#define OPTION_SUBSCRIBE                's'
#define OPTION_PUBLISH                  'p'
#define OPTION_REGISTER                 'r'
#define OPTION_KEEPALIVE                'k'
static struct option longopts[] = {
        { "help",       no_argument,            NULL,   OPTION_HELP },
        { "clients",    required_argument,      NULL,   OPTION_CLIENTS },
        { "subscribe",  required_argument,      NULL,   OPTION_SUBSCRIBE },
        { "publish",    required_argument,      NULL,   OPTION_PUBLISH },
        { "register",   required_argument,      NULL,   OPTION_REGISTER },
        { "keepalive",  required_argument,      NULL,   OPTION_KEEPALIVE },
        { NULL,         0,                      NULL,   0 },
};

static volatile int g_running;

static void mbus_client_callback_connect (struct mbus_client *mbus_client, void *context, enum mbus_client_connect_status status)
{
	int rc;
	struct subscription *subscription;
	struct mbus_client_subscribe_options subscribe_options;
        struct command *command;
        struct mbus_client_register_options register_options;

	struct client *client = context;

	(void) mbus_client;

	if (status == mbus_client_connect_status_success) {
		client->connected = 1;
                TAILQ_FOREACH(subscription, client->subscriptions, list) {
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
                TAILQ_FOREACH(command, client->commands, list) {
                        rc = mbus_client_register_options_default(&register_options);
                        if (rc != 0) {
                                fprintf(stderr, "can not get default register options\n");
                                goto bail;
                        }
                        register_options.command = command->command;
                        rc = mbus_client_register_with_options(client->client, &register_options);
                        if (rc != 0) {
                                fprintf(stderr, "can not register command\n");
                                goto bail;
                        }
                }
	} else {
		fprintf(stderr, "connect: %s\n", mbus_client_connect_status_string(status));
		if (mbus_client_get_options(client->client)->connect_interval <= 0) {
			client->connected = 0;
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

static void mbus_client_callback_publish (struct mbus_client *mbus_client, void *context, struct mbus_client_message_event *message, enum mbus_client_publish_status status)
{
        struct client *client = context;
        (void) mbus_client;
        (void) client;
        (void) message;
        (void) status;
}

static int mbus_client_callback_routine (struct mbus_client *mbus_client, void *context, struct mbus_client_message_routine *message)
{
        struct client *client = context;
        (void) mbus_client;
        (void) client;
        (void) message;
        return 0;
}

static int client_process (struct client *client)
{
        int rc;
        unsigned long long diff_time;
        unsigned long long current_time;

        struct client_publish *client_publish;

        if (client == NULL) {
                fprintf(stderr, "client is invalid\n");
                goto bail;
        }

        current_time = mbus_clock_monotonic();
        if (current_time > client->processed_at) {
                diff_time = current_time - client->processed_at;
        } else {
                diff_time = 0;
        }
        client->processed_at = current_time;

        if (client->keepalive) {
                if (client->connected == 0 &&
                    client->disconnected == 0) {
                        rc = mbus_client_connect(client->client);
                        if (rc != 0) {
                                fprintf(stderr, "client connect failed\n");
                                goto bail;
                        }
                } else if (client->connected == 1 &&
                           client->disconnected == 0) {
                        TAILQ_FOREACH(client_publish, &client->publishs, list) {
                                client_publish->timeout -= diff_time;
                                client_publish->timeout -= (rand() % client_publish->publish->interval) / 100;

                                if (client_publish->timeout <= 0) {
                                        struct mbus_client_publish_options publish_options;

                                        rc = mbus_client_publish_options_default(&publish_options);
                                        if (rc != 0) {
                                                fprintf(stderr, "can not get default publish options\n");
                                                goto bail;
                                        }
                                        publish_options.event    = client_publish->publish->event;
                                        publish_options.payload  = client_publish->publish->payload_json;
                                        rc = mbus_client_publish_with_options(client->client, &publish_options);
                                        if (rc != 0) {
                                                fprintf(stderr, "can not publish event\n");
                                                goto bail;
                                        }
                                        client_publish->timeout = client_publish->publish->interval;
                                }
                        }
                }
        } else {
                if (client->connected == 0 &&
                    client->disconnected == 0) {
                        TAILQ_FOREACH(client_publish, &client->publishs, list) {
                                client_publish->timeout -= diff_time;
                                client_publish->timeout -= (rand() % client_publish->publish->interval) / 100;

                                if (client_publish->timeout <= 0) {
                                        rc = mbus_client_connect(client->client);
                                        if (rc != 0) {
                                                fprintf(stderr, "client connect failed\n");
                                                goto bail;
                                        }
                                }
                        }
                } else if (client->connected == 1 &&
                           client->disconnected == 0) {
                        if (mbus_client_get_state(client->client) == mbus_client_state_connected) {
                                TAILQ_FOREACH(client_publish, &client->publishs, list) {
                                        client_publish->timeout -= diff_time;
                                        client_publish->timeout -= (rand() % client_publish->publish->interval) / 100;

                                        if (client_publish->timeout <= 0) {
                                                struct mbus_client_publish_options publish_options;

                                                rc = mbus_client_publish_options_default(&publish_options);
                                                if (rc != 0) {
                                                        fprintf(stderr, "can not get default publish options\n");
                                                        goto bail;
                                                }
                                                publish_options.event    = client_publish->publish->event;
                                                publish_options.payload  = client_publish->publish->payload_json;
                                                rc = mbus_client_publish_with_options(client->client, &publish_options);
                                                if (rc != 0) {
                                                        fprintf(stderr, "can not publish event\n");
                                                        goto bail;
                                                }
                                                client_publish->timeout = client_publish->publish->interval;
                                        }
                                }
                                if (mbus_client_has_pending(client->client) == 0)  {
                                        mbus_client_disconnect(client->client);
                                }
                        }
                } else if (client->connected == 1 &&
                           client->disconnected == 1) {
                        client->connected    = 0;
                        client->disconnected = 0;

                }
        }
        return 0;
bail:   return -1;
}

static void client_destroy (struct client *client)
{
        struct client_publish *client_publish;
        struct client_publish *nclient_publish;
	if (client == NULL) {
		return;
	}
	if (client->client != NULL) {
		mbus_client_destroy(client->client);
	}
	TAILQ_FOREACH_SAFE(client_publish, &client->publishs, list, nclient_publish) {
	        TAILQ_REMOVE(&client->publishs, client_publish, list);
	        free(client_publish);
	}
	free(client);
}

static struct client * client_create (struct mbus_client_options *options, int keepalive, struct subscriptions *subscriptions, struct publishs *publishs, struct commands *commands)
{
	struct client *client;
	struct client_publish *client_publish;
	struct publish *publish;
	client = malloc(sizeof(struct client));
	if (client == NULL) {
		fprintf(stderr, "can not allocate memory\n");
		goto bail;
	}
	memset(client, 0, sizeof(struct client));
	TAILQ_INIT(&client->publishs);
	options->callbacks.connect    = mbus_client_callback_connect;
	options->callbacks.disconnect = mbus_client_callback_disconnect;
	options->callbacks.message    = mbus_client_callback_message;
        options->callbacks.publish    = mbus_client_callback_publish;
        options->callbacks.routine    = mbus_client_callback_routine;
	options->callbacks.context    = client;
	client->client = mbus_client_create(options);
	if (client->client == NULL) {
		fprintf(stderr, "can not client mbus client\n");
		goto bail;
	}
        client->subscriptions = subscriptions;
        client->commands = commands;
        TAILQ_FOREACH(publish, publishs, list) {
                client_publish = malloc(sizeof(struct client_publish));
                if (client_publish == NULL) {
                        fprintf(stderr, "can not allocate memory\n");
                        goto bail;
                }
                memset(client_publish, 0, sizeof(struct client_publish));
                client_publish->publish = publish;
                client_publish->timeout = publish->interval;
                TAILQ_INSERT_TAIL(&client->publishs, client_publish, list);
        }
        client->keepalive = keepalive;
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
        const char *mark;
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
        mark = strchr(topic, ':');
        if (mark == NULL) {
                fprintf(stderr, "topic is invalid\n");
                goto bail;
        }
        if (topic == mark) {
                subscription->source = strdup(MBUS_METHOD_EVENT_SOURCE_ALL);
        } else {
                subscription->source = strndup(topic, mark - topic);
        }
        if (subscription->source == NULL) {
                fprintf(stderr, "can not allocate memory\n");
                goto bail;
        }
        if (strlen(mark) == 1) {
                subscription->event = strdup(MBUS_METHOD_EVENT_IDENTIFIER_ALL);
        } else {
                subscription->event = strdup(mark + 1);
        }
        if (subscription->event == NULL) {
                fprintf(stderr, "can not allocate memory\n");
                goto bail;
        }
        return subscription;
bail:   if (subscription != NULL) {
                subscription_destroy(subscription);
        }
        return NULL;
}

static void publish_destroy (struct publish *publish)
{
        if (publish == NULL) {
                return;
        }
        if (publish->event != NULL) {
                free(publish->event);
        }
        if (publish->payload != NULL) {
                free(publish->payload);
        }
        if (publish->payload_json != NULL) {
                mbus_json_delete(publish->payload_json);
        }
        free(publish);
}

static struct publish * publish_create (const char *topic)
{
        char *tmp;
        char *mark;
        char *interval;
        char *event;
        char *payload;
        struct publish *publish;
        tmp = NULL;
        publish = NULL;
        if (topic == NULL) {
                fprintf(stderr, "topic is invalid\n");
                goto bail;
        }
        publish = malloc(sizeof(struct publish));
        if (publish == NULL) {
                fprintf(stderr, "can not allocate memory\n");
                goto bail;
        }
        memset(publish, 0, sizeof(struct publish));
        tmp = strdup(topic);
        if (tmp == NULL) {
                fprintf(stderr, "can not allocate memory\n");
                goto bail;
        }
        interval = tmp;
        mark = strchr(interval, ':');
        if (mark == NULL) {
                fprintf(stderr, "topic is invalid\n");
                goto bail;
        }
        *mark = '\0';
        event = mark + 1;
        mark = strchr(event, ':');
        if (mark == NULL) {
                fprintf(stderr, "topic is invalid\n");
                goto bail;
        }
        *mark = '\0';
        payload = mark + 1;
        if (strlen(interval) == 0) {
                fprintf(stderr, "interval is invalid\n");
                goto bail;
        }
        publish->interval = atoi(interval);
        if (publish->interval == 0) {
                fprintf(stderr, "interval is invalid\n");
                goto bail;
        }
        if (strlen(event) == 0) {
                fprintf(stderr, "event is invalid\n");
                goto bail;
        }
        publish->event = strdup(event);
        if (publish->event == NULL) {
                fprintf(stderr, "can not allocate memory\n");
                goto bail;
        }
        if (strlen(payload) == 0) {
                fprintf(stderr, "payload is invalid\n");
                goto bail;
        }
        publish->payload = strdup(payload);
        if (publish->payload == NULL) {
                fprintf(stderr, "can not allocate memory\n");
                goto bail;
        }
        publish->payload_json = mbus_json_parse(payload);
        if (publish->payload == NULL) {
                fprintf(stderr, "can not parse payload\n");
                goto bail;
        }
        free(tmp);
        return publish;
bail:   if (publish != NULL) {
                publish_destroy(publish);
        }
        if (tmp != NULL) {
                free(tmp);
        }
        return NULL;
}

static void command_destroy (struct command *command)
{
        if (command == NULL) {
                return;
        }
        if (command->command != NULL) {
                free(command->command);
        }
        free(command);
}

static struct command * command_create (const char *topic)
{
        struct command *command;
        command = NULL;
        if (topic == NULL) {
                fprintf(stderr, "topic is invalid\n");
                goto bail;
        }
        command = malloc(sizeof(struct command));
        if (command == NULL) {
                fprintf(stderr, "can not allocate memory\n");
                goto bail;
        }
        memset(command, 0, sizeof(struct command));
        command->command = strdup(topic);
        if (command->command == NULL) {
                fprintf(stderr, "can not allocate memory\n");
                goto bail;
        }
        return command;
bail:   if (command != NULL) {
                command_destroy(command);
        }
        return NULL;
}

static void signal_handler (int signal)
{
	(void) signal;
	g_running = 0;
}

static void usage (void)
{
	fprintf(stdout, "mbus benchmark arguments:\n");
	fprintf(stdout, "  -c, --clients   : number of clients (default: 1)\n");
        fprintf(stdout, "  -s, --subscribe : event identifier to subscribe (default: none)\n");
        fprintf(stdout, "                    source:event = %s:%s\n", "source", "event");
        fprintf(stdout, "                    source:      = %s:%s\n", "source", MBUS_METHOD_EVENT_IDENTIFIER_ALL);
        fprintf(stdout, "                    :event       = %s:%s\n", MBUS_METHOD_EVENT_SOURCE_ALL, "event");
        fprintf(stdout, "                    :            = %s:%s\n", MBUS_METHOD_EVENT_SOURCE_ALL, MBUS_METHOD_EVENT_IDENTIFIER_ALL);
        fprintf(stdout, "  -p, --publish   : event identifier to publish (default: none)\n");
        fprintf(stdout, "                    interval:event:payload = %s:%s:%s\n", "interval", "event", "payload");
        fprintf(stdout, "  -r, --register  : command identifier to register (default: none)\n");
        fprintf(stdout, "  -k, --keepalive : keep connection (defualt: -1)\n");
        fprintf(stdout, "                    -1 : all of clients\n");
        fprintf(stdout, "                    0  : none of clients\n");
        fprintf(stdout, "                    > 0: n of clients\n");
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

        struct command *command;
        struct command *ncommand;
        struct commands commands;

        struct publish *publish;
        struct publish *npublish;
        struct publishs publishs;

        struct subscription *subscription;
        struct subscription *nsubscription;
        struct subscriptions subscriptions;

	struct client *client;
	struct client *nclient;
	struct clients clients;
	struct mbus_client_options mbus_client_options;

	int i;
	int nclients;
	int keepalive;
	int timeout;
	struct pollfd *pollfds;

	pollfds   = NULL;
	nclients  = 1;
	keepalive = -1;
	TAILQ_INIT(&clients);
        TAILQ_INIT(&commands);
        TAILQ_INIT(&publishs);
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

	while ((c = getopt_long(_argc, _argv, ":c:s:p:r:k:h", longopts, NULL)) != -1) {
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
                                TAILQ_INSERT_TAIL(&subscriptions, subscription, list);
                                break;
                        case OPTION_PUBLISH:
                                publish = publish_create(optarg);
                                if (publish == NULL) {
                                        fprintf(stderr, "can not create publish\n");
                                        goto bail;
                                }
                                TAILQ_INSERT_TAIL(&publishs, publish, list);
                                break;
                        case OPTION_REGISTER:
                                command = command_create(optarg);
                                if (command == NULL) {
                                        fprintf(stderr, "can not create command\n");
                                        goto bail;
                                }
                                TAILQ_INSERT_TAIL(&commands, command, list);
                                break;
                        case OPTION_KEEPALIVE:
                                keepalive = atoi(optarg);
                                break;
			case OPTION_HELP:
				usage();
				goto bail;
		}
	}

	optind = _optind;

	srand(time(NULL));

        if (keepalive < 0) {
                keepalive = nclients;
        } else if (keepalive > nclients) {
                keepalive = nclients;
        }
        fprintf(stdout, "clients      : %d\n", nclients);
        fprintf(stdout, "keepalive    : %d\n", keepalive);
        fprintf(stdout, "publishs     :\n");
        TAILQ_FOREACH(publish, &publishs, list) {
                fprintf(stderr, "  interval: %d, event: '%s', payload: '%s'\n", publish->interval, publish->event, publish->payload);
        }
        fprintf(stdout, "subscriptions:\n");
        TAILQ_FOREACH(subscription, &subscriptions, list) {
                fprintf(stderr, "  source: '%s', event: '%s'\n", subscription->source, subscription->event);
        }
        fprintf(stdout, "registers    :\n");
        TAILQ_FOREACH(command, &commands, list) {
                fprintf(stderr, "  command: '%s'\n", command->command);
        }

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
	for (i = 0; i < nclients; i++) {
		client = client_create(&mbus_client_options, (i < keepalive), &subscriptions, &publishs, &commands);
		if (client == NULL) {
			fprintf(stderr, "can not create client\n");
			goto bail;
		}
		TAILQ_INSERT_TAIL(&clients, client, list);
	}

	pollfds = malloc(sizeof(struct pollfd) * (clients.count * 2));
	if (pollfds == NULL) {
		fprintf(stderr, "can not allocate memory\n");
		goto bail;
	}

	while (1) {
		i = 0;
		timeout = 1000;
                TAILQ_FOREACH(client, &clients, list) {
                        rc = client_process(client);
                        if (rc != 0) {
                                fprintf(stderr, "client process failed\n");
                                goto bail;
                        }

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

		TAILQ_FOREACH_SAFE(client, &clients, list, nclient) {
		        if (mbus_client_get_state(client->client) != mbus_client_state_disconnected) {
                                rc = mbus_client_run(client->client, 0);
                                if (rc != 0) {
                                        fprintf(stderr, "client run failed\n");
                                        goto bail;
                                }
		        }
		}
	}

        TAILQ_FOREACH_SAFE(command, &commands, list, ncommand) {
                TAILQ_REMOVE(&commands, command, list);
                command_destroy(command);
        }
        TAILQ_FOREACH_SAFE(publish, &publishs, list, npublish) {
                TAILQ_REMOVE(&publishs, publish, list);
                publish_destroy(publish);
        }
        TAILQ_FOREACH_SAFE(subscription, &subscriptions, list, nsubscription) {
                TAILQ_REMOVE(&subscriptions, subscription, list);
                subscription_destroy(subscription);
        }
	TAILQ_FOREACH_SAFE(client, &clients, list, nclient) {
		TAILQ_REMOVE(&clients, client, list);
		client_destroy(client);
	}
	free(pollfds);
	free(_argv);
	return 0;
bail:	TAILQ_FOREACH_SAFE(command, &commands, list, ncommand) {
                TAILQ_REMOVE(&commands, command, list);
                command_destroy(command);
        }
        TAILQ_FOREACH_SAFE(publish, &publishs, list, npublish) {
                TAILQ_REMOVE(&publishs, publish, list);
                publish_destroy(publish);
        }
        TAILQ_FOREACH_SAFE(subscription, &subscriptions, list, nsubscription) {
                TAILQ_REMOVE(&subscriptions, subscription, list);
                subscription_destroy(subscription);
        }
	TAILQ_FOREACH_SAFE(client, &clients, list, nclient) {
		TAILQ_REMOVE(&clients, client, list);
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
