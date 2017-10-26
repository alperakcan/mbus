
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>

#include <poll.h>
#include <signal.h>

#include <mbus/debug.h>
#include <mbus/client.h>
#include <mbus/json.h>

#include "clock.h"
#include "exec.h"

static struct option options[] = {
	{"reader"		, no_argument		, 0, 'r' },
	{"writer"		, no_argument		, 0, 'w' },
	{"identifier"		, required_argument	, 0, 'i' },
	{"reconnect"		, required_argument	, 0, 'l' },
	{"command"		, required_argument	, 0, 'c' },
	{"timeout"		, required_argument	, 0, 't' },
	{"help"			, no_argument	   	, 0, 'h' },
	{0			, 0                	, 0, 0 }
};

static void print_help (const char *name)
{
	fprintf(stdout, "%s options command\n", name);
	fprintf(stdout, "  -r / --reader     : mode reader\n");
	fprintf(stdout, "  -w / --writer     : mode writer\n");
	fprintf(stdout, "  -i / --identifier : mbus identifier\n");
	fprintf(stdout, "  -l / --reconnect  : reconnect on error\n");
	fprintf(stdout, "  -c / --command    : request command\n");
	fprintf(stdout, "  -t / --timeout    : request timeout milliseconds\n");
	fprintf(stdout, "example:\n");
	fprintf(stdout, "  %s -r -i org.mbus.client.execute-command -c 1\n", name);
	fprintf(stdout, "  %s -w -i org.mbus.client.execute-command -t 60000 -c 'ls -al'\n", name);
	mbus_client_usage();
}

static int command_execute (struct mbus_client *client, const char *source, const char *command, struct mbus_json *payload, struct mbus_json *result, void *data)
{
	int rc;

	int size;
	int length;
	char *buffer;

	const char *shell;
	int timeout;

	struct pollfd pollfd[2];
	unsigned long long execute_time;
	unsigned long long stopped_time;
	unsigned long long current_time;

	int pid;
	int io[3];
	char *argv[4];
	int status;

	(void) client;
	(void) source;
	(void) command;
	(void) data;

	pid = -1;
	io[0] = -1;
	io[1] = -1;
	io[2] = -1;

	size = 0;
	length = 0;
	buffer = NULL;

	shell = mbus_json_get_string_value(payload, "command", NULL);
	if (shell == NULL) {
		mbus_errorf("invalid command");
		goto bail;
	}
	mbus_debugf("shell: %s", shell);
	timeout = mbus_json_get_int_value(payload, "timeout", 5000);
	mbus_debugf("timeout: %d", timeout);

	argv[0] = "/bin/sh";
	argv[1] = "-c";
	argv[2] = (char *) shell;
	argv[3] = NULL;

	pid = command_exec(argv, io);
	if (pid < 0) {
		goto bail;
	}

	execute_time = command_clock_monotonic();
	while (1) {
		current_time = command_clock_monotonic();
		if (timeout > 0 &&
		    command_clock_after(current_time, execute_time + timeout)) {
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

	timeout = 1000;
	stopped_time = command_clock_monotonic();
	while (1) {
		current_time = command_clock_monotonic();
		if (timeout > 0 &&
		    command_clock_after(current_time, stopped_time + timeout)) {
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
	return -1;
}

int main (int argc, char *argv[])
{
	int c;
	int i;
	int l;
	int rc;

	struct mbus_json *request;
	struct mbus_json *response;
	struct mbus_client *mbus_client;
	struct mbus_client_options mbus_client_options;

	static int o_reader;
	static int o_writer;
	static const char *o_identifier;
	static int o_reconnect;
	static const char *o_command;
	static int o_timeout;

	o_reader = 0;
	o_writer = 0;
	o_identifier = NULL;
	o_reconnect = 1;
	o_command = NULL;
	o_timeout = 10000;

	request = NULL;
	response = NULL;

	mbus_client = NULL;
	mbus_client_options_from_argv(&mbus_client_options, argc, argv);

	while ((c = getopt_long(argc, argv, ":rwi:l:c:t:", options, NULL)) != -1) {
		switch (c) {
			case 'h':
				print_help(argv[0]);
				return 0;
			case 'r':
				o_reader = 1;
				break;
			case 'w':
				o_writer = 1;
				break;
			case 'i':
				o_identifier = optarg;
				break;
			case 'l':
				o_reconnect = atoi(optarg);
				break;
			case 'c':
				o_command = optarg;
				break;
			case 't':
				o_timeout = atoi(optarg);
				break;
		}
	}
	if (o_reader == 0 && o_writer == 0) {
		fprintf(stderr, "invalid mode\n");
		goto bail;
	}
	if (o_reader == 1 && o_writer == 1) {
		fprintf(stderr, "invalid mode\n");
		goto bail;
	}
	if (o_identifier == NULL) {
		fprintf(stderr, "invalid identifier\n");
		goto bail;
	}
	if (o_writer == 1) {
		if (o_command == NULL) {
			fprintf(stderr, "invalid command\n");
			goto bail;
		}
	}

	if (o_reader == 1) {
		while (1) {
			if (mbus_client == NULL) {
				mbus_client_options.client.name = o_identifier;
				mbus_client = mbus_client_create_with_options(&mbus_client_options);
				if (mbus_client == NULL) {
					fprintf(stderr, "can not create mbus client\n");
					if (o_reconnect > 0) {
						usleep(o_reconnect * 1000);
						continue;
					} else {
						goto bail;
					}
				} else {
					rc = mbus_client_register(mbus_client, "command.execute", command_execute, NULL);
					if (rc != 0) {
						fprintf(stderr, "can not register command\n");
						if (o_reconnect  > 0) {
							mbus_client_destroy(mbus_client);
							mbus_client = NULL;
							usleep(o_reconnect * 1000);
							continue;
						} else {
							goto bail;
						}
					}
				}
			}
			rc = mbus_client_run_timeout(mbus_client, MBUS_CLIENT_DEFAULT_TIMEOUT);
			if (rc < 0) {
				fprintf(stderr, "client run failed: %d\n", rc);
				if (o_reconnect > 0) {
					mbus_client_destroy(mbus_client);
					mbus_client = NULL;
					usleep(o_reconnect * 1000);
					continue;
				} else {
					goto bail;
				}
			}
			if (rc > 1) {
				fprintf(stderr, "client run exited: %d\n", rc);
				if (o_reconnect > 0) {
					mbus_client_destroy(mbus_client);
					mbus_client = NULL;
					usleep(o_reconnect * 1000);
					continue;
				} else {
					goto bail;
				}
			}
		}
	}
	if (o_writer == 1) {
		l = 0;
		for (i = 0; i < argc; i++) {
			l += strlen(argv[i]);
			l += 1;
		}
		fprintf(stdout, "executing command: '%s'\n", o_command);
		mbus_client = mbus_client_create_with_options(&mbus_client_options);
		if (mbus_client == NULL) {
			fprintf(stderr, "can not create mbus client\n");
			goto bail;
		}
		request = mbus_json_create_object();
		mbus_json_add_string_to_object_cs(request, "command", o_command);
		mbus_json_add_number_to_object_cs(request, "timeout", o_timeout);
		rc = mbus_client_command_timeout(mbus_client, o_identifier, "command.execute", request, &response, o_timeout);
		if (rc != 0) {
			fprintf(stderr, "can not execute command\n");
			goto bail;
		}
		if (response != NULL) {
			int status;
			const char *output;
			status = mbus_json_get_int_value(response, "status", -1);
			if (status != 0) {
				fprintf(stdout, "can not execute command\n");
			} else {
				output = mbus_json_get_string_value(response, "output", "can not execute command");
				fprintf(stdout, "%s\n", output);
			}
		}
		mbus_client_destroy(mbus_client);
		mbus_client = NULL;
	}

	rc = 0;
out:	if (mbus_client != NULL) {
		mbus_client_destroy(mbus_client);
	}
	if (request != NULL) {
		mbus_json_delete(request);
	}
	if (response != NULL) {
		mbus_json_delete(response);
	}
	return rc;
bail:	rc = -1;
	goto out;
}
