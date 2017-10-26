
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <mbus/client.h>
#include <mbus/json.h>

#include "base64.h"

static int o_r;
static int o_l;
static const char *o_p;
static int o_w;
static const char *o_s;
static const char *o_d;
static const char *o_n;
static int o_t;

static struct option options[] = {
	{"reader"		, no_argument		, 0, 'r' },
	{"writer"		, no_argument		, 0, 'w' },
	{"prefix"		, required_argument	, 0, 'p' },
	{"reconnect"		, required_argument	, 0, 'l' },
	{"source"		, required_argument	, 0, 's' },
	{"destination"		, required_argument	, 0, 'd' },
	{"identifier"		, required_argument	, 0, 'n' },
	{"timeout"		, required_argument	, 0, 't' },
	{"help"			, no_argument	   	, 0, 'h' },
	{0			, 0                	, 0, 0 }
};

static void print_help (const char *name)
{
	fprintf(stdout, "%s options:\n", name);
	fprintf(stdout, "  -r / --reader     : mode reader\n");
	fprintf(stdout, "  -p / --prefix     : file prefix\n");
	fprintf(stdout, "  -l / --reconnect  : reconnect on error\n");
	fprintf(stdout, "  -w / --writer     : mode writer\n");
	fprintf(stdout, "  -s / --source     : file source\n");
	fprintf(stdout, "  -d / --destination: file destination\n");
	fprintf(stdout, "  -n / --identifier : mbus identifier\n");
	fprintf(stdout, "  -t / --timeout    : request timeout milliseconds\n");
	fprintf(stdout, "example:\n");
	fprintf(stdout, "  %s -r -n org.mbus.client.file-transfer [-l 1] -p /tmp\n", name);
	fprintf(stdout, "  %s -w -n org.mbus.client.file-transfer -s from -d to -t 30000\n", name);
	mbus_client_usage();
}

static int command_put (struct mbus_client *client, const char *source, const char *command, struct mbus_json *payload, struct mbus_json *result, void *data)
{
	int rc;
	int fd;
	const char *s;
	const char *d;
	const char *e;
	char *fname;
	char *decoded;
	size_t decoded_length;
	(void) client;
	(void) source;
	(void) command;
	(void) payload;
	(void) result;
	(void) data;
	fd = -1;
	fname = NULL;
	decoded = NULL;
	s = mbus_json_get_string_value(payload, "source", NULL);
	if (s == NULL) {
		fprintf(stderr, "source is invalid\n");
		goto bail;
	}
	d = mbus_json_get_string_value(payload, "destination", NULL);
	if (d == NULL) {
		fprintf(stderr, "destination is invalid\n");
		goto bail;
	}
	e = mbus_json_get_string_value(payload, "encoded", NULL);
	if (d == NULL) {
		fprintf(stderr, "encoded is invalid\n");
		goto bail;
	}
	decoded = (char *) base64_decode((unsigned char *) e, strlen(e), &decoded_length);
	if (decoded == NULL) {
		fprintf(stderr, "can not decode source: %s\n", s);
		goto bail;
	}
	fname = malloc(strlen(o_p) + strlen(d) + 1);
	if (fname == NULL) {
		fprintf(stderr, "can not allocate memory\n");
		goto bail;
	}
	sprintf(fname, "%s%s", o_p, d);
	fd = open(fname, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd < 0) {
		fprintf(stderr, "can not open file: %s\n", fname);
		goto bail;
	}
	rc = write(fd, decoded, decoded_length);
	if (rc != (int) decoded_length) {
		fprintf(stderr, "can not write destination: %s\n", d);
		goto bail;
	}
	close(fd);
	free(fname);
	free(decoded);
	return 0;
bail:	if (decoded != NULL) {
		free(decoded);
	}
	if (fd >= 0) {
		close(fd);
	}
	if (fname != NULL) {
		unlink(fname);
		free(fname);
	}
	return -1;
}

int main (int argc, char *argv[])
{
	int c;
	int rc;
	int fd;
	char *buffer;
	size_t buffer_length;
	char *encoded;
	size_t encoded_length;
	struct mbus_json *req;
	struct mbus_client *mbus_client;
	struct mbus_client_options mbus_client_options;

	o_r = 0;
	o_p = NULL;
	o_l = 0;
	o_w = 0;
	o_s = NULL;
	o_d = NULL;
	o_n = "org.pipeline.mbus-file-transfer";
	o_t = 30000;

	fd = -1;
	req = NULL;
	buffer = NULL;
	encoded = NULL;
	mbus_client = NULL;
	mbus_client_options_from_argv(&mbus_client_options, argc, argv);

	while (1) {
		c = getopt_long(argc, argv, ":rlp:ws:d:n:t:h", options, NULL);
		if (c == -1) {
			break;
		}
		switch (c) {
			case 'h':
				print_help(argv[0]);
				return 0;
			case 'r':
				o_r = 1;
				break;
			case 'l':
				o_l = 1;
				break;
			case 'p':
				o_p = optarg;
				break;
			case 'w':
				o_w = 1;
				break;
			case 's':
				o_s = optarg;
				break;
			case 'd':
				o_d = optarg;
				break;
			case 'n':
				o_n = optarg;
				break;
			case 't':
				o_t = atoi(optarg);
				break;
		}
	}

	if (o_r == 0 && o_w == 0) {
		fprintf(stderr, "invalid mode\n");
		goto bail;
	}
	if (o_r == 1 && o_w == 1) {
		fprintf(stderr, "invalid mode\n");
		goto bail;
	}
	if (o_r == 1) {
		if (o_p == NULL) {
			fprintf(stderr, "invalid prefix\n");
			goto bail;
		}
	}
	if (o_w == 1) {
		if (o_s == NULL) {
			fprintf(stderr, "invalid source\n");
			goto bail;
		}
		if (o_d == NULL) {
			fprintf(stderr, "invalid destination\n");
			goto bail;
		}
	}
	if (o_n == NULL) {
		fprintf(stderr, "invalid identifier\n");
		goto bail;
	}

	if (o_r == 1) {
		while (1) {
			if (mbus_client == NULL) {
				mbus_client_options.client.name = o_n;
				mbus_client = mbus_client_create_with_options(&mbus_client_options);
				if (mbus_client == NULL) {
					fprintf(stderr, "can not create mbus client\n");
					if (o_l) {
						usleep(1000000);
						continue;
					} else {
						goto bail;
					}
				} else {
					rc = mbus_client_register(mbus_client, "command.put", command_put, NULL);
					if (rc != 0) {
						fprintf(stderr, "can not register command\n");
						if (o_l) {
							mbus_client_destroy(mbus_client);
							mbus_client = NULL;
							usleep(1000000);
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
				if (o_l) {
					mbus_client_destroy(mbus_client);
					mbus_client = NULL;
					usleep(1000000);
					continue;
				} else {
					goto bail;
				}
			}
			if (rc > 1) {
				fprintf(stderr, "client run exited: %d\n", rc);
				if (o_l) {
					mbus_client_destroy(mbus_client);
					mbus_client = NULL;
					usleep(1000000);
					continue;
				} else {
					goto bail;
				}
			}
		}
	}
	if (o_w == 1) {
		int fd;
		struct stat st;
		rc = stat(o_s, &st);
		if (rc < 0) {
			fprintf(stderr, "can not open source: %s\n", o_s);
			goto bail;
		}
		buffer_length = st.st_size;
		buffer = malloc(buffer_length + 1);
		if (buffer == NULL) {
			fprintf(stderr, "can not allocate memory\n");
			goto bail;
		}
		fd = open(o_s, O_RDONLY);
		if (fd < 0) {
			fprintf(stderr, "can not open source: %s\n", o_s);
			goto bail;
		}
		rc = read(fd, buffer, buffer_length);
		if (rc != (int) buffer_length) {
			fprintf(stderr, "can not read source: %s\n", o_s);
			goto bail;
		}
		buffer[rc] = '\0';
		encoded = (char *) base64_encode((unsigned char *) buffer, buffer_length, &encoded_length);
		if (encoded == NULL) {
			fprintf(stderr, "can not encode source: %s\n", o_s);
			goto bail;
		}
		mbus_client = mbus_client_create_with_options(&mbus_client_options);
		if (mbus_client == NULL) {
			fprintf(stderr, "can not create mbus client\n");
			goto bail;
		}
		req = mbus_json_create_object();
		mbus_json_add_string_to_object_cs(req, "source", o_s);
		mbus_json_add_string_to_object_cs(req, "destination", o_d);
		mbus_json_add_string_to_object_cs(req, "encoded", encoded);
		rc = mbus_client_command_timeout(mbus_client, o_n, "command.put", req, NULL, o_t);
		if (rc != 0) {
			fprintf(stderr, "can not execute command\n");
			goto bail;
		}
		mbus_client_destroy(mbus_client);
		mbus_client = NULL;
	}

	rc = 0;
out:	if (mbus_client != NULL) {
		mbus_client_destroy(mbus_client);
	}
	if (req != NULL) {
		mbus_json_delete(req);
	}
	if (buffer != NULL) {
		free(buffer);
	}
	if (encoded != NULL) {
		free(encoded);
	}
	if (fd >= 0) {
		close(fd);
	}
	return rc;
bail:	rc = -1;
	goto out;
}
