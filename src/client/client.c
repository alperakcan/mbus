
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

#define OPTION_HELP		0x100
#define OPTION_DEBUG_LEVEL	0x101
#define OPTION_SERVER_PROTOCOL	0x201
#define OPTION_SERVER_ADDRESS	0x202
#define OPTION_SERVER_PORT	0x203
#define OPTION_CLIENT_NAME	0x301
#define OPTION_PING_INTERVAL	0x401
#define OPTION_PING_TIMEOUT	0x402
#define OPTION_PING_THRESHOLD	0x403
static struct option longopts[] = {
	{ "mbus-help",			no_argument,		NULL,	OPTION_HELP },
	{ "mbus-debug-level",		required_argument,	NULL,	OPTION_DEBUG_LEVEL },
	{ "mbus-server-protocol",	required_argument,	NULL,	OPTION_SERVER_PROTOCOL },
	{ "mbus-server-address",	required_argument,	NULL,	OPTION_SERVER_ADDRESS },
	{ "mbus-server-port",		required_argument,	NULL,	OPTION_SERVER_PORT },
	{ "mbus-client-name",		required_argument,	NULL,	OPTION_CLIENT_NAME },
	{ "mbus-ping-interval",		required_argument,	NULL,	OPTION_PING_INTERVAL },
	{ "mbus-ping-timeout",		required_argument,	NULL,	OPTION_PING_TIMEOUT },
	{ "mbus-ping-threshold",	required_argument,	NULL,	OPTION_PING_THRESHOLD },
	{ NULL,				0,			NULL,	0 },
};

struct mbus_client {
};

void mbus_client_usage (void)
{
	fprintf(stdout, "mbus client arguments:\n");
	fprintf(stdout, "  --mbus-debug-level     : debug level (default: %s)\n", mbus_debug_level_to_string(mbus_debug_level));
	fprintf(stdout, "  --mbus-server-protocol : server protocol (default: %s)\n", MBUS_SERVER_PROTOCOL);
	fprintf(stdout, "  --mbus-server-address  : server address (default: %s)\n", MBUS_SERVER_ADDRESS);
	fprintf(stdout, "  --mbus-server-port     : server port (default: %d)\n", MBUS_SERVER_PORT);
	fprintf(stdout, "  --mbus-client-name     : client name (overrides api parameter)\n");
	fprintf(stdout, "  --mbus-ping-interval   : ping interval (overrides api parameter) (default: %d)\n", MBUS_CLIENT_DEFAULT_PING_INTERVAL);
	fprintf(stdout, "  --mbus-ping-timeout    : ping timeout (overrides api parameter) (default: %d)\n", MBUS_CLIENT_DEFAULT_PING_TIMEOUT);
	fprintf(stdout, "  --mbus-ping-threshold  : ping threshold (overrides api parameter) (default: %d)\n", MBUS_CLIENT_DEFAULT_PING_THRESHOLD);
	fprintf(stdout, "  --mbus-help            : this text\n");
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

void mbus_client_destroy (struct mbus_client *client)
{
	if (client == NULL) {
		return;
	}
	free(client);
}

struct mbus_client * mbus_client_create (const struct mbus_client_options *_options)
{
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
	return client;
bail:	if (client != NULL) {
		mbus_client_destroy(client);
	}
	return NULL;
}
