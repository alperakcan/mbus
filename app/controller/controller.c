
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
#include <getopt.h>
#include <signal.h>

#include "mbus/debug.h"
#include "mbus/server.h"

static volatile int g_running;

static void signal_handler (int signal)
{
	(void) signal;
	g_running = 0;
}

#define OPTION_HELP		0x100
#define OPTION_SUBSCRIBE	0x101
static struct option longopts[] = {
	{ "help",			no_argument,		NULL,	OPTION_HELP },
	{ NULL,				0,			NULL,	0 },
};

static void usage (void)
{
	fprintf(stdout, "mbus controller arguments:\n");
	fprintf(stdout, "  --help                   : this text\n");
	fprintf(stdout, "  --mbus-help              : mbus help text\n");
	mbus_server_usage();
}

int main (int argc, char *argv[])
{
	int c;
	int rc;

	int _argc;
	char **_argv;
	int _optind;

	struct mbus_server *server;

	g_running = 1;
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

	server = NULL;
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
			case OPTION_HELP:
				usage();
				goto bail;
		}
	}

	optind = _optind;
	free(_argv);

	server = mbus_server_create(argc, argv);
	if (server == NULL) {
		mbus_errorf("can not create server");
		goto bail;
	}

	while (g_running) {
		rc = mbus_server_run_timeout(server, MBUS_SERVER_DEFAULT_TIMEOUT);
		if (rc < 0) {
			mbus_errorf("can not run server");
			goto bail;
		}
		if (rc == 1) {
			break;
		}
	}

	mbus_server_destroy(server);
	return 0;
bail:	if (server != NULL) {
		mbus_server_destroy(server);
	}
	if (_argv != NULL) {
		free(_argv);
	}
	return -1;
}
