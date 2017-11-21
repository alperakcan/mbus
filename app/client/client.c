
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

#include <ctype.h>
#include <poll.h>
#include <readline/readline.h>
#include <readline/history.h>

#define MBUS_DEBUG_NAME	"app-client"

#include "mbus/debug.h"
#include "mbus/json.h"
#include "mbus/method.h"
#include "mbus/client.h"
#include "mbus/server.h"

#define OPTION_HELP		0x100
#define OPTION_DESTINATION	0x101
#define OPTION_COMMAND		0x102
#define OPTION_PAYLOAD		0x103
static struct option longopts[] = {
	{ "help",		no_argument,	NULL,	OPTION_HELP },
	{ NULL,			0,		NULL,	0 },
};

static int g_running;
static struct mbus_client *g_mbus_client;

static void usage (void)
{
	fprintf(stdout, "mbus client arguments:\n");
	fprintf(stdout, "  --help                   : this text\n");
	fprintf(stdout, "  --mbus-help              : mbus help text\n");
	mbus_client_usage();
}

static void mbus_client_callback_connect (struct mbus_client *client, void *context, enum mbus_client_connect_status status)
{
	(void) client;
	(void) context;
	fprintf(stdout, "\033[0G** connect: %d, %s\n", status, mbus_client_connect_status_string(status));
	rl_redraw_prompt_last_line();
}

static void mbus_client_callback_disconnect (struct mbus_client *client, void *context, enum mbus_client_disconnect_status status)
{
	(void) client;
	(void) context;
	fprintf(stdout, "\033[0G** disconnect: %d, %s\n", status, mbus_client_disconnect_status_string(status));
	rl_redraw_prompt_last_line();
}

static char * readline_strip (char *buf)
{
	char *start;
	if (buf == NULL) {
		return NULL;
	}
	while ((*buf != '\0') && (buf[strlen(buf) - 1] < 33)) {
		buf[strlen(buf) - 1] = '\0';
	}
	start = buf;
	while (*start && (*start < 33)) {
		start++;
	}
	return start;
}

struct command {
	char *name;
	int (*func)(int argc, char *argv[]);
	char *help;
};

static int command_quit (int argc, char *argv[])
{
	(void) argc;
	(void) argv;
	g_running = 0;
	mbus_client_break(g_mbus_client);
	return 1;
}

static int command_connect (int argc, char *argv[])
{
	int rc;
	(void) argv;
	if (argc != 0) {
		return -2;
	}
	rc = mbus_client_connect(g_mbus_client);
	if (rc != 0) {
		fprintf(stderr, "can not connect client\n");
		return -1;
	}
	return 0;
}

static int command_disconnect (int argc, char *argv[])
{
	int rc;
	(void) argv;
	if (argc != 0) {
		return -2;
	}
	rc = mbus_client_disconnect(g_mbus_client);
	if (rc != 0) {
		fprintf(stderr, "can not disconnect client\n");
		return -1;
	}
	return 0;
}

static struct command *commands[] = {
	&(struct command) {
		"quit",
		command_quit,
		"quit application"
	},
	&(struct command) {
		"connect",
		command_connect,
		"connect"
	},
	&(struct command) {
		"disconnect",
		command_disconnect,
		"disconnect"
	},
	NULL,
};

static int readline_process (char *command)
{
	int ret;
	char *b;
	char *p;
	int argc;
	char **argv;
	struct command **pc;

	if (command == NULL) {
		return 0;
	}
	if (strlen(command) == 0) {
		return 0;
	}

	ret = 0;
	argc = 0;
	argv = NULL;
	b = strdup(command);
	p = b;

	while (*p) {
		while (isspace(*p)) {
			p++;
		}

		if (*p == '"' || *p == '\'') {
			char const delim = *p;
			char *const begin = ++p;

			while (*p && *p != delim) {
				p++;
			}
			if (*p) {
				*p++ = '\0';
				argv = (char **) realloc(argv, sizeof(char *) * (argc + 1));
				argv[argc] = begin;
				argc++;
			} else {
				goto out;
			}
		} else {
			char *const begin = p;

			while (*p && !isspace(*p)) {
				p++;
			}
			if (*p) {
				*p++ = '\0';
				argv = (char **) realloc(argv, sizeof(char *) * (argc + 1));
				argv[argc] = begin;
				argc++;
			} else if (p != begin) {
				argv = (char **) realloc(argv, sizeof(char *) * (argc + 1));
				argv[argc] = begin;
				argc++;
			}
		}
	}

	if (strcmp(argv[0], "help") == 0) {
		for (pc = commands; *pc; pc++) {
			printf("%-15s - %s\n", (*pc)->name, (*pc)->help);
		}
	} else {
		for (pc = commands; *pc; pc++) {
			if (strcmp((*pc)->name, argv[0]) == 0) {
				ret = (*pc)->func(argc - 1, &argv[1]);
				if (ret < 0) {
					fprintf(stderr, "command: %s failed: %s\n", argv[0], (ret == -2) ? "invalid arguments" : "internal error");
				}
				break;
			}
		}
	}

out:
	free(argv);
	free(b);
	return ret;
}

static void process_line (char *line)
{
	if (line != NULL) {
		readline_strip(line);
		readline_process(line);
		add_history(line);
		free(line);
	}
}

int main (int argc, char *argv[])
{
	int rc;

	int c;
	int _argc;
	char **_argv;

	int npollfd;
	struct pollfd pollfd[2];

	struct mbus_client *client;
	struct mbus_client_options options;

	client = NULL;

	_argc = 0;
	_argv = NULL;

	_argv = malloc(sizeof(char *) * argc);
	if (_argv == NULL) {
		fprintf(stderr, "can not allocate memory\n");
		goto bail;
	}
	for (_argc = 0; _argc < argc; _argc++) {
		_argv[_argc] = argv[_argc];
	}

	while ((c = getopt_long(_argc, _argv, ":", longopts, NULL)) != -1) {
		switch (c) {
			case OPTION_HELP:
				usage();
				goto bail;
		}
	}

	rc = mbus_client_options_from_argv(&options, argc, argv);
	if (rc != 0) {
		fprintf(stderr, "can not parse options\n");
		goto bail;
	}
	options.callbacks.connect    = mbus_client_callback_connect;
	options.callbacks.disconnect = mbus_client_callback_disconnect;
	client = mbus_client_create(&options);
	if (client == NULL) {
		fprintf(stderr, "can not create client\n");
		goto bail;
	}

	g_running = 1;
	g_mbus_client = client;
	rl_callback_handler_install("client> ", process_line);

	while (g_running != 0) {
		npollfd = 0;
		memset(pollfd, 0, sizeof(pollfd));

		pollfd[npollfd].fd = 0;
		pollfd[npollfd].events = POLLIN;
		pollfd[npollfd].revents = 0;
		npollfd += 1;

		if (mbus_client_get_fd(client) >= 0) {
			pollfd[npollfd].fd = mbus_client_get_fd(client);
			pollfd[npollfd].events = mbus_client_get_fd_events(client);
			pollfd[npollfd].revents = 0;
			npollfd += 1;
		}

		rc = poll(pollfd, npollfd, MBUS_CLIENT_DEFAULT_RUN_TIMEOUT);
		if (rc < 0) {
			fprintf(stderr, "poll failed with: %d\n", rc);
			goto bail;
		}

		if (pollfd[0].revents & POLLIN) {
			rl_callback_read_char();
		}

		rc = mbus_client_run(client, 0);
		if (rc != 0) {
			fprintf(stderr, "client run failed\n");
			goto bail;
		}
	}

	rl_clear_history();
	rl_cleanup_after_signal();
	clear_history();

	mbus_client_destroy(client);
	free(_argv);
	return 0;
bail:	if (client != NULL) {
		mbus_client_destroy(client);
	}
	if (_argv != NULL) {
		free(_argv);
	}
	rl_clear_history();
	rl_cleanup_after_signal();
	clear_history();
	return -1;
}
