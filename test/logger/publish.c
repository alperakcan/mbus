
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
#include <getopt.h>
#include <unistd.h>
#include <signal.h>

#include <poll.h>

#define MBUS_DEBUG_NAME	"test-logger"

#include <mbus/debug.h>
#include <mbus/client.h>
#include <mbus/json.h>

#define OPTION_HELP	'h'
#define OPTION_QOS	'q'
static struct option longopts[] = {
	{"qos"			, required_argument   	, 0, OPTION_QOS },
	{"help"			, no_argument	   	, 0, OPTION_HELP },
	{0			, 0                	, 0, 0 }
};

struct mbus_client_callback_param {
	int connected;
	int disconnected;
	int qos;
};

static void usage (const char *name)
{
	fprintf(stdout, "%s options:\n", name);
	fprintf(stdout, "  -q, --qos : publish qos (default: 0)\n");
	fprintf(stdout, "              0: at most once\n");
	fprintf(stdout, "              1: at least once\n");
	fprintf(stdout, "              2: exactly once\n");
	fprintf(stdout, "  -h, --help: this text\n");
	mbus_client_usage();
}

static void mbus_client_callback_connect (struct mbus_client *client, void *context, enum mbus_client_connect_status status)
{
	struct mbus_client_callback_param *param = context;
	fprintf(stdout, "connect: %d, %s\n", status, mbus_client_connect_status_string(status));
	if (status == mbus_client_connect_status_success) {
		param->connected = 1;
	} else {
		if (mbus_client_get_options(client)->connect_interval <= 0) {
			param->connected = -1;
		}
	}
}

static void mbus_client_callback_disconnect (struct mbus_client *client, void *context, enum mbus_client_disconnect_status status)
{
	struct mbus_client_callback_param *param = context;
	fprintf(stdout, "disconnect: %d, %s\n", status, mbus_client_disconnect_status_string(status));
	if (mbus_client_get_options(client)->connect_interval <= 0) {
		param->disconnected = 1;
	}
}

static void mbus_client_callback_publish (struct mbus_client *client, void *context, struct mbus_client_message_event *message, enum mbus_client_publish_status status)
{
	char *string;
	(void) client;
	(void) context;
	(void) message;
	string = mbus_json_print(mbus_client_message_event_payload(message));
	fprintf(stdout, "publish status: %d, %s message: %s.%s: %s\n", status, mbus_client_publish_status_string(status), mbus_client_message_event_destination(message), mbus_client_message_event_identifier(message), string);
	free(string);
}

int main (int argc, char *argv[])
{
	int rc;

	int c;
	int _argc;
	char **_argv;

	int lline;
	int sline;
	char *line;

	int timeout;
	int npollfd;
	struct pollfd pollfd[3];

	struct mbus_client *mbus_client;
	struct mbus_client_options mbus_client_options;
	struct mbus_client_callback_param mbus_client_callback_param;

	_argc = 0;
	_argv = NULL;

	line = NULL;
	sline = 0;
	lline = 0;

	mbus_client = NULL;
	memset(&mbus_client_callback_param, 0, sizeof(struct mbus_client_callback_param));

	_argv = malloc(sizeof(char *) * argc);
	if (_argv == NULL) {
		fprintf(stderr, "can not allocate memory\n");
		goto bail;
	}

	optind = 1;
	for (_argc = 0; _argc < argc; _argc++) {
		_argv[_argc] = argv[_argc];
	}
	while ((c = getopt_long(_argc, _argv, ":h", longopts, NULL)) != -1) {
		switch (c) {
			case OPTION_HELP:
				usage(argv[0]);
				goto out;
			case OPTION_QOS:
				mbus_client_callback_param.qos = atoi(optarg);
				break;
		}
	}

	rc = mbus_client_options_default(&mbus_client_options);
	if (rc != 0) {
		fprintf(stderr, "can not get default options\n");
		goto bail;
	}
	mbus_client_options.callbacks.connect = mbus_client_callback_connect;
	mbus_client_options.callbacks.disconnect = mbus_client_callback_disconnect;
	mbus_client_options.callbacks.publish = mbus_client_callback_publish;
	mbus_client_options.callbacks.context = &mbus_client_callback_param;
	rc = mbus_client_options_from_argv(&mbus_client_options, argc, argv);
	if (rc != 0) {
		fprintf(stderr, "can not parse options\n");
		goto bail;
	}

	mbus_client = mbus_client_create(&mbus_client_options);
	if (mbus_client == NULL) {
		fprintf(stderr, "can not create client\n");
		goto bail;
	}
	rc = mbus_client_connect(mbus_client);
	if (rc != 0) {
		fprintf(stderr, "can not connect client\n");
		goto bail;
	}

	while (1) {
		if (mbus_client_callback_param.connected < 0 ||
		    mbus_client_callback_param.disconnected != 0) {
			break;
		}

		timeout = -1;
		npollfd = 0;
		memset(pollfd, 0, sizeof(pollfd));

		pollfd[npollfd].fd = 0;
		pollfd[npollfd].events = POLLIN;
		pollfd[npollfd].revents = 0;
		npollfd += 1;

		if (mbus_client_get_wakeup_fd(mbus_client) >= 0) {
			pollfd[npollfd].fd = mbus_client_get_wakeup_fd(mbus_client);
			pollfd[npollfd].events = mbus_client_get_wakeup_fd_events(mbus_client);
			pollfd[npollfd].revents = 0;
			npollfd += 1;
		}

		if (mbus_client_get_connection_fd(mbus_client) >= 0) {
			pollfd[npollfd].fd = mbus_client_get_connection_fd(mbus_client);
			pollfd[npollfd].events = mbus_client_get_connection_fd_events(mbus_client);
			pollfd[npollfd].revents = 0;
			npollfd += 1;
		}
		timeout = mbus_client_get_run_timeout(mbus_client);

		rc = poll(pollfd, npollfd, timeout);
		if (rc < 0) {
			fprintf(stderr, "poll failed with: %d\n", rc);
			goto bail;
		}

		if (pollfd[0].revents & POLLIN) {
			char ch;
			rc = read(0, &ch, sizeof(ch));
			if (rc != sizeof(ch)) {
				fprintf(stderr, "can not read stdin\n");
				goto bail;
			}
			{
				char *tmp;
				if (lline + 1 >= sline) {
					sline += 1024;
					tmp = malloc(sline);
					if (tmp == NULL) {
						fprintf(stderr, "can not allocate memory");
						goto bail;
					}
					memcpy(tmp, line, lline);
					free(line);
					line = tmp;
				}
			}
			if (ch != '\n') {
				line[lline] = ch;
				lline += 1;
			} else {
				line[lline] = '\0';
				fprintf(stdout, "publishing: '%s'\n", line);
				{
					struct mbus_json *publish_payload;
					struct mbus_client_publish_options publish_options;
					publish_payload = mbus_json_create_object();
					if (publish_payload == NULL) {
						fprintf(stderr, "can not create payload\n");
						goto bail;
					}
					rc = mbus_json_add_string_to_object_cs(publish_payload, "line", line);
					if (rc != 0) {
						fprintf(stderr, "can not add string to payload\n");
						mbus_json_delete(publish_payload);
						goto bail;
					}
					mbus_client_publish_options_default(&publish_options);
					publish_options.event = "org.mbus.test.logger.event.line";
					publish_options.payload = publish_payload;
					publish_options.qos = mbus_client_callback_param.qos;
					rc = mbus_client_publish_with_options(mbus_client, &publish_options);
					if (rc != 0) {
						fprintf(stderr, "can not publish payload\n");
					}
					mbus_json_delete(publish_payload);
				}
				lline = 0;
			}
		}

		rc = mbus_client_run(mbus_client, 0);
		if (rc != 0) {
			fprintf(stderr, "client run failed\n");
			goto bail;
		}
	}

out:	if (_argv != NULL) {
		free(_argv);
	}
	if (line != NULL) {
		free(line);
	}
	if (mbus_client != NULL) {
		mbus_client_destroy(mbus_client);
	}
	return 0;
bail:	if (_argv != NULL) {
		free(_argv);
	}
	if (line != NULL) {
		free(line);
	}
	if (mbus_client != NULL) {
		mbus_client_destroy(mbus_client);
	}
	return -1;
}
