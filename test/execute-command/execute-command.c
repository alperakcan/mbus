
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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <mbus/debug.h>
#include <mbus/clock.h>
#include <mbus/client.h>
#include <mbus/json.h>

#include "exec.h"

#define DEFAULT_MODE		mode_receiver
#define DEFAULT_IDENTIFIER	"org.mbus.client.execute-command"
#define DEFAULT_PREFIX		"./"
#define DEFAULT_TIMEOUT		30000

enum mode {
	mode_unknown,
	mode_receiver,
	mode_sender,
};

struct receiver_param {
	const char *prefix;
	int result;
	int finished;
};

struct sender_param {
	const char *identifier;
	const char *command;
	int timeout;
	int status;
	int finished;
};

static enum mode mode_value (const char *mode)
{
	if (strcasecmp(mode, "receiver") == 0) {
		return mode_receiver;
	}
	if (strcasecmp(mode, "r") == 0) {
		return mode_receiver;
	}
	if (strcasecmp(mode, "sender") == 0) {
		return mode_sender;
	}
	if (strcasecmp(mode, "s") == 0) {
		return mode_sender;
	}
	return mode_unknown;
}

static const char * mode_string (enum mode mode)
{
	if (mode == mode_receiver) {
		return "receiver";
	}
	if (mode == mode_sender) {
		return "sender";
	}
	return "unknown";
}

static struct option longopts[] = {
	{"mode"			, required_argument	, 0, 'm' },
	{"identifier"		, required_argument	, 0, 'i' },
	{"command"		, required_argument	, 0, 'c' },
	{"timeout"		, required_argument	, 0, 't' },
	{"help"			, no_argument	   	, 0, 'h' },
	{0			, 0                	, 0, 0 }
};

static void usage (const char *name)
{
	fprintf(stdout, "%s options:\n", name);
	fprintf(stdout, "\n");
	fprintf(stdout, "%s --mode receiver --identifier identifier\n", name);
	fprintf(stdout, "%s --mode sender --identifier identifier --command command [--timeout timeout]\n", name);
	fprintf(stdout, "\n");
	fprintf(stdout, "  -m, --mode: running mode (default: %s)\n", mode_string(DEFAULT_MODE));
	fprintf(stdout, "  modes:\n");
	fprintf(stdout, "    receiver\n");
	fprintf(stdout, "      -i, --identifier : application identifier (default: %s)\n", DEFAULT_IDENTIFIER);
	fprintf(stdout, "    sender\n");
	fprintf(stdout, "      -i, --identifier : receiver application identifier (default: %s)\n", DEFAULT_IDENTIFIER);
	fprintf(stdout, "      -c, --command    : command to execute\n");
	fprintf(stdout, "      -t, --timeout    : request timeout milliseconds (default: %d)\n", DEFAULT_TIMEOUT);
	fprintf(stdout, "example:\n");
	fprintf(stdout, "  %s --mode receiver --identifier org.mbus.client.execute-command\n", name);
	fprintf(stdout, "  %s --mode sender --identifier org.mbus.client.execute-command --command 'ls -al' --timeout 30000\n", name);
	mbus_client_usage();
}

static int mbus_client_receiver_callback_command_execute (struct mbus_client *client, void *context, struct mbus_client_message_routine *message)
{
	int rc;

	int size;
	int length;
	char *buffer;

	const char *command;
	int timeout;

	struct pollfd pollfd[2];
	unsigned long long execute_time;
	unsigned long long stopped_time;
	unsigned long long current_time;

	int pid;
	int io[3];
	char *argv[4];
	int status;

	struct mbus_json *result;

	(void) client;
	(void) context;
	(void) message;

	pid = -1;
	io[0] = -1;
	io[1] = -1;
	io[2] = -1;

	size = 0;
	length = 0;
	buffer = NULL;
	result = NULL;

	command = mbus_json_get_string_value(mbus_client_message_routine_request_payload(message), "command", NULL);
	if (command == NULL) {
		mbus_errorf("invalid command");
		goto bail;
	}
	mbus_debugf("command: %s", command);
	timeout = mbus_json_get_int_value(mbus_client_message_routine_request_payload(message), "timeout", 5000);
	mbus_debugf("timeout: %d", timeout);

	argv[0] = "/bin/sh";
	argv[1] = "-c";
	argv[2] = (char *) command;
	argv[3] = NULL;

	pid = command_exec(argv, io);
	if (pid < 0) {
		goto bail;
	}

	execute_time = mbus_clock_monotonic();
	while (1) {
		current_time = mbus_clock_monotonic();
		if (timeout > 0 &&
		    mbus_clock_after(current_time, execute_time + timeout)) {
			command_kill(pid, SIGKILL);
			break;
		}

		pollfd[0].fd = io[1];
		pollfd[0].events = POLLIN;
		pollfd[0].revents = 0;
		pollfd[1].fd = io[2];
		pollfd[1].events = POLLIN;
		pollfd[1].revents = 0;
		rc = poll(pollfd, 2, 100);
		if (rc == 0) {
			continue;
		}
		if (rc < 0) {
			break;
		}

		if (length + 1 >= size) {
			size += 4096;
			buffer = realloc(buffer, size);
			buffer[length] = '\0';
		}

		if (pollfd[0].revents) {
			rc = read(pollfd[0].fd, buffer + length, size - length - 1);
		} else if (pollfd[1].revents) {
			rc = read(pollfd[1].fd, buffer + length, size - length - 1);
		} else {
			continue;
		}
		if (rc == 0) {
			break;
		}
		if (rc < 0) {
			mbus_errorf("can not read pipe");
			goto bail;
		}
		length += rc;
		buffer[length] = '\0';
	}

	result = mbus_json_create_object();
	if (result == NULL) {
		mbus_errorf("can not create json object");
		goto bail;
	}

	timeout = 1000;
	stopped_time = mbus_clock_monotonic();
	while (1) {
		current_time = mbus_clock_monotonic();
		if (timeout > 0 &&
		    mbus_clock_after(current_time, stopped_time + timeout)) {
			command_kill(pid, SIGKILL);
			break;
		}
		rc = command_waitpid(pid, &status, command_waitpid_option_nohang);
		if (rc > 0) {
			mbus_json_add_number_to_object_cs(result, "status", status);
			mbus_json_add_number_to_object_cs(result, "WIFEXITED", WIFEXITED(status));
			mbus_json_add_number_to_object_cs(result, "WEXITSTATUS", WEXITSTATUS(status));
			mbus_json_add_number_to_object_cs(result, "WIFSIGNALED", WIFSIGNALED(status));
			mbus_json_add_number_to_object_cs(result, "WTERMSIG", WTERMSIG(status));
			mbus_json_add_number_to_object_cs(result, "WIFSTOPPED", WIFSTOPPED(status));
			mbus_json_add_number_to_object_cs(result, "WSTOPSIG", WSTOPSIG(status));
			mbus_json_add_number_to_object_cs(result, "WIFCONTINUED", WIFCONTINUED(status));
			pid = -1;
			break;
		}
	}

	if (length > 0) {
		buffer[length] = '\0';
		mbus_json_add_string_to_object_cs(result, "output", buffer);
	} else {
		mbus_json_add_string_to_object_cs(result, "output", "");
	}

	if (pid >= 0) {
		command_kill(pid, SIGKILL);
	}
	if (io[0] >= 0) {
		close(io[0]);
	}
	if (io[1] >= 0) {
		close(io[1]);
	}
	if (io[2] >= 0) {
		close(io[2]);
	}
	if (buffer != NULL) {
		free(buffer);
	}
	mbus_client_message_routine_set_response_payload(message, result);
	mbus_json_delete(result);
	return 0;
bail:	if (pid >= 0) {
		command_kill(pid, SIGKILL);
	}
	if (io[0] >= 0) {
		close(io[0]);
	}
	if (io[1] >= 0) {
		close(io[1]);
	}
	if (io[2] >= 0) {
		close(io[2]);
	}
	if (buffer != NULL) {
		free(buffer);
	}
	if (result != NULL) {
		mbus_json_delete(result);
	}
	return -1;
}

static void mbus_client_receiver_callback_connect (struct mbus_client *client, void *context, enum mbus_client_connect_status status)
{
	int rc;
	struct receiver_param *param = context;
	if (status == mbus_client_connect_status_success) {
		struct mbus_client_register_options register_options;
		rc = mbus_client_register_options_default(&register_options);
		if (rc != 0) {
			param->result = -1;
			param->finished = 1;
			return;
		}
		register_options.command = "command.execute";
		register_options.callback = mbus_client_receiver_callback_command_execute;
		register_options.context = param;
		rc = mbus_client_register_with_options(client, &register_options);
		if (rc != 0) {
			param->result = -1;
			param->finished = 1;
		}
	} else {
		param->result = -1;
		param->finished = 1;
	}
}

static void mbus_client_sender_callback_command_execute_result (struct mbus_client *client, void *context, struct mbus_client_message_command *message, enum mbus_client_command_status status)
{
	struct sender_param *param = context;
	(void) client;
	(void) status;
	param->status = mbus_client_message_command_response_status(message);
	if (param->status == 0) {
		fprintf(stdout, "%s\n", mbus_json_get_string_value(mbus_client_message_command_response_payload(message), "output", ""));
	}
	param->finished = 1;
}

static void mbus_client_sender_callback_connect (struct mbus_client *client, void *context, enum mbus_client_connect_status status)
{
	int rc;
	struct mbus_json *request;
	struct sender_param *param = context;
	struct mbus_client_command_options command_options;
	request = NULL;
	if (status != mbus_client_connect_status_success) {
		goto bail;
	}
	request = mbus_json_create_object();
	mbus_json_add_string_to_object_cs(request, "command", param->command);
	mbus_json_add_number_to_object_cs(request, "timeout", param->timeout);
	mbus_client_command_options_default(&command_options);
	command_options.destination = param->identifier;
	command_options.command = "command.execute";
	command_options.payload = request;
	command_options.callback = mbus_client_sender_callback_command_execute_result;
	command_options.context = param;
	command_options.timeout = param->timeout;
	rc = mbus_client_command_with_options(client, &command_options);
	if (rc != 0) {
		fprintf(stderr, "can not execute command\n");
		goto bail;
	}
	mbus_json_delete(request);
	return;
bail:	if (request != NULL) {
		mbus_json_delete(request);
	}
	param->status = -1;
	param->finished = 1;
	return;
}

int main (int argc, char *argv[])
{
	int rc;

	int c;
	int _argc;
	char **_argv;

	enum mode o_mode;
	const char *o_prefix;
	const char *o_identifier;
	const char *o_command;
	int o_timeout;

	struct mbus_client *mbus_client;
	struct mbus_client_options mbus_options;

	struct receiver_param receiver_param;
	struct sender_param sender_param;

	o_mode = DEFAULT_MODE;
	o_prefix = DEFAULT_PREFIX;
	o_identifier = DEFAULT_IDENTIFIER;
	o_command = NULL;
	o_timeout = DEFAULT_TIMEOUT;

	_argc = 0;
	_argv = NULL;

	memset(&receiver_param, 0, sizeof(struct receiver_param));
	memset(&sender_param, 0, sizeof(struct sender_param));
	mbus_client = NULL;

	_argv = malloc(sizeof(char *) * argc);
	if (_argv == NULL) {
		fprintf(stderr, "can not allocate memory\n");
		goto bail;
	}
	for (_argc = 0; _argc < argc; _argc++) {
		_argv[_argc] = argv[_argc];
	}

	while ((c = getopt_long(_argc, _argv, ":m:p:i:c:t:h", longopts, NULL)) != -1) {
		switch (c) {
			case 'm':
				o_mode = mode_value(optarg);
				if (o_mode == mode_unknown) {
					fprintf(stderr, "invalid mode: %s\n", optarg);
					goto bail;
				}
				break;
			case 'p':
				o_prefix = optarg;
				break;
			case 'i':
				o_identifier = optarg;
				break;
			case 'c':
				o_command = optarg;
				break;
			case 't':
				o_timeout = atoi(optarg);
				break;
			case 'h':
				usage(argv[0]);
				goto bail;
		}
	}

	rc = mbus_client_options_default(&mbus_options);
	if (rc != 0) {
		fprintf(stderr, "can not get default options\n");
		goto bail;
	}
	rc = mbus_client_options_from_argv(&mbus_options, argc, argv);
	if (rc != 0) {
		fprintf(stderr, "can not parse options\n");
		goto bail;
	}
	if (o_mode == mode_receiver) {
		memset(&receiver_param, 0, sizeof(struct receiver_param));
		receiver_param.prefix = o_prefix;
		mbus_options.identifier = (char *) o_identifier;
		mbus_options.callbacks.connect = mbus_client_receiver_callback_connect;
		mbus_options.callbacks.context = &receiver_param;
	} else if (o_mode == mode_sender) {
		if (o_command == NULL) {
			fprintf(stderr, "command is invalid\n");
			goto bail;
		}
		memset(&sender_param, 0, sizeof(struct sender_param));
		sender_param.identifier = o_identifier;
		sender_param.command = o_command;
		sender_param.timeout = o_timeout;
		mbus_options.callbacks.connect = mbus_client_sender_callback_connect;
		mbus_options.callbacks.context = &sender_param;
	} else {
		fprintf(stderr, "invalid mode: %d\n", o_mode);
		goto bail;
	}

	mbus_client = mbus_client_create(&mbus_options);
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
		rc = mbus_client_run(mbus_client, MBUS_CLIENT_DEFAULT_RUN_TIMEOUT);
		if (rc != 0) {
			fprintf(stderr, "client run failed\n");
			goto bail;
		}
		if (o_mode == mode_receiver) {
			if (receiver_param.finished == 1 &&
			    mbus_client_has_pending(mbus_client) == 0) {
				break;
			}
		} else if (o_mode == mode_sender) {
			if (sender_param.finished == 1 &&
			    mbus_client_has_pending(mbus_client) == 0) {
				break;
			}
		}
		if (mbus_client_get_state(mbus_client) == mbus_client_state_disconnected) {
			break;
		}
	}

	mbus_client_destroy(mbus_client);
	free(_argv);
	if (o_mode == mode_receiver) {
		return receiver_param.result;
	}
	if (o_mode == mode_sender) {
		return sender_param.status;
	}
	return 0;
bail:	if (_argv != NULL) {
		free(_argv);
	}
	if (mbus_client != NULL) {
		mbus_client_destroy(mbus_client);
	}
	return -1;
}
