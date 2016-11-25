
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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <poll.h>

#define MBUS_DEBUG_NAME	"mbus-launcher"

#include "mbus/debug.h"
#include "mbus/cJSON.h"
#include "mbus/tailq.h"
#include "mbus/exec.h"

struct arg {
	TAILQ_ENTRY(arg) args;
	char *key;
	char *value;
};
TAILQ_HEAD(args, arg);

enum app_state {
	app_state_running,
	app_state_stopped,
};
struct app {
	TAILQ_ENTRY(app) apps;
	int enable;
	int log;
	char *name;
	struct args args;
	enum app_state state;
	int io[2];
	int logfd;
	pid_t pid;
};
TAILQ_HEAD(apps, app);

struct config {
	struct apps apps;
};

#define OPTION_DEBUG_LEVEL	0x100
#define OPTION_HELP		0x101
#define OPTION_CONFIG		0x102
static struct option longopts[] = {
	{ "mbus-debug-level",	required_argument,	NULL,	OPTION_DEBUG_LEVEL },
	{ "help",			no_argument,		NULL,	OPTION_HELP },
	{ "config",			required_argument,	NULL,	OPTION_CONFIG },
	{ NULL,				0,			NULL,	0 },
};

static void usage (void)
{
	fprintf(stdout, "mbus launcher arguments:\n");
	fprintf(stdout, "  --mbus-debug-level     : debug level (default: %s)\n", mbus_debug_level_to_string(mbus_debug_level));
	fprintf(stdout, "  --config                    : configuration file\n");
}

static char * read_file (const char *filename)
{
	int rc;
	int fd;
	int length;
	char *buffer;
	struct stat stat;
	buffer = NULL;
	fd = open(filename, O_RDONLY);
	if (fd < 0) {
		mbus_errorf("can not open file: %s", filename);
		goto bail;
	}
	rc = fstat(fd, &stat);
	if (rc < 0) {
		mbus_errorf("can not get stats from file: %s", filename);
		goto bail;
	}
	length = stat.st_size;
	if (!S_ISREG(stat.st_mode)) {
		mbus_errorf("file is not regular");
		goto bail;
	}
	buffer = (char *) malloc(length + 1);
	if (buffer == NULL) {
		mbus_errorf("can not allocate memory");
		goto bail;
	}
	rc = read(fd, buffer, length);
	if (rc < 0) {
		mbus_errorf("can not read file");
		goto bail;
	}
	buffer[rc] = '\0';
	close(fd);
	return buffer;
bail:	if (fd >= 0) {
		close(fd);
	}
	if (buffer != NULL) {
		free(buffer);
	}
	return NULL;
}

static const char * arg_get_key (struct arg *arg)
{
	if (arg == NULL) {
		return NULL;
	}
	return arg->key;
}

static const char * arg_get_value (struct arg *arg)
{
	if (arg == NULL) {
		return NULL;
	}
	return arg->value;
}

static void arg_destroy (struct arg *arg)
{
	if (arg == NULL) {
		return;
	}
	if (arg->key != NULL) {
		free(arg->key);
	}
	if (arg->value != NULL) {
		free(arg->value);
	}
	free(arg);
}

static struct arg * arg_create (const char *key, const char *value)
{
	struct arg *arg;
	arg = NULL;
	if (key == NULL) {
		mbus_errorf("key is null");
		goto bail;
	}
	if (value == NULL) {
		mbus_errorf("value is null");
		goto bail;
	}
	arg = malloc(sizeof(struct arg));
	if (arg == NULL) {
		mbus_errorf("can not allocate memory");
		goto bail;
	}
	memset(arg, 0, sizeof(struct arg));
	arg->key = strdup(key);
	arg->value = strdup(value);
	if ((arg->key == NULL) ||
	    (arg->value == NULL)) {
		mbus_errorf("can not allocate memory");
		goto bail;
	}
	return arg;
bail:	if (arg != NULL) {
		arg_destroy(arg);
	}
	return NULL;
}

static void app_destroy (struct app *app)
{
	struct arg *arg;
	struct arg *narg;
	if (app == NULL) {
		return;
	}
	if (app->name != NULL) {
		free(app->name);
	}
	TAILQ_FOREACH_SAFE(arg, &app->args, args, narg) {
		TAILQ_REMOVE(&app->args, arg, args);
		arg_destroy(arg);
	}
	free(app);
}

static struct app * app_create (cJSON *application)
{
	int i;
	cJSON *item;
	cJSON *object;

	struct app *app;
	struct arg *arg;

	app = NULL;
	if (application == NULL) {
		mbus_errorf("application is null");
		goto bail;
	}

	app = malloc(sizeof(struct app));
	if (app == NULL) {
		mbus_errorf("can not allocate memory");
		goto bail;
	}
	memset(app, 0, sizeof(struct app));
	TAILQ_INIT(&app->args);
	app->state = app_state_stopped;
	app->pid = -1;
	app->io[0] = -1;
	app->io[1] = -1;
	app->logfd = -1;

	app->enable = ((object = cJSON_GetObjectItem(application, "enable")) != NULL) ? (object->type == cJSON_True) : 0;
	app->name = (cJSON_GetStringValue(application, "name") != NULL) ? strdup(cJSON_GetStringValue(application, "name")) : NULL;
	app->log = ((object = cJSON_GetObjectItem(application, "enable")) != NULL) ? (object->type == cJSON_True) : 0;

	object = cJSON_GetObjectItem(application, "arguments");
	if (object == NULL) {
		goto out;
	}
	object = object->child;
	while (object != NULL) {
		if (object->type == cJSON_String) {
			arg = arg_create(object->string, object->valuestring);
			if (arg == NULL) {
				mbus_errorf("can not create arg");
				goto bail;
			}
			TAILQ_INSERT_TAIL(&app->args, arg, args);
		} else if (object->type == cJSON_Array) {
			for (i = 0; i < cJSON_GetArraySize(object); i++) {
				item = cJSON_GetArrayItem(object, i);
				if (item == NULL) {
					mbus_errorf("can not create arg");
					goto bail;
				}
				if (item->type != cJSON_String) {
					mbus_errorf("invalid argument: %s", item->string);
					goto bail;
				}
				arg = arg_create(object->string, item->valuestring);
				if (arg == NULL) {
					mbus_errorf("can not create arg");
					goto bail;
				}
				TAILQ_INSERT_TAIL(&app->args, arg, args);
			}
		} else {
			mbus_errorf("invalid argument: %s", object->string);
			goto bail;
		}
		object = object->next;
	}

out:	if (app->name == NULL) {
		mbus_errorf("invalid app config");
		goto bail;
	}
	return app;

bail:	if (app != NULL) {
		app_destroy(app);
	}
	return NULL;
}

static int app_execute (struct app *app)
{
	int i;
	char **args;
	struct arg *arg;
	args = NULL;
	if (app == NULL) {
		mbus_errorf("app is null");
		goto bail;
	}
	if (app->enable == 0) {
		goto out;
	}
	mbus_infof("executing: %s", app->name);
	args = malloc(sizeof(char *) * (1 + 1 + app->args.count * 2 + 1));
	if (args == NULL) {
		mbus_errorf("can not allocate memory");
		goto bail;
	}
	i = 0;
#if 0
	args[i++] = "valgrind";
#endif
	args[i++] = app->name;
	TAILQ_FOREACH(arg, &app->args, args) {
		if (arg_get_key(arg) != NULL) {
			args[i++] = (char *) arg_get_key(arg);
		}
		if (arg_get_value(arg) != NULL) {
			args[i++] = (char *) arg_get_value(arg);
		}
	}
	args[i] = NULL;
	for (i = 0; args[i] != NULL; i++) {
		mbus_debugf("  %s", args[i]);
	}
	app->pid = mbus_exec(args, app->io);
	if (app->pid < 0) {
		mbus_errorf("can not execute app");
		goto bail;
	}
	if (app->log != 0) {
		char path[256];
		snprintf(path, sizeof(path) - 1, "/tmp/launcher.%s.log", app->name);
		app->logfd = open(path, O_TRUNC | O_CREAT | O_WRONLY, 0666);
	}
	app->state = app_state_running;
out:	if (args != NULL) {
		free(args);
	}
	return 0;
bail:	if (args != NULL) {
		free(args);
	}
	return -1;
}

static void config_destroy (struct config *config)
{
	struct app *app;
	struct app *napp;
	if (config == NULL) {
		return;
	}
	TAILQ_FOREACH_SAFE(app, &config->apps, apps, napp) {
		TAILQ_REMOVE(&config->apps, app, apps);
		app_destroy(app);
	}
	free(config);
}

static struct config * config_create (const char *file)
{
	int i;
	char *buffer;
	struct config *config;

	cJSON *root;
	cJSON *application;
	cJSON *applications;

	struct app *app;

	root = NULL;
	buffer = NULL;
	config = NULL;

	if (file == NULL) {
		mbus_errorf("file is null");
		goto bail;
	}

	buffer = read_file(file);
	if (buffer == NULL) {
		mbus_errorf("can not read file");
		goto bail;
	}

	root = cJSON_Parse(buffer);
	if (root == NULL) {
		mbus_errorf("can not parse file: %s", cJSON_GetErrorPtr());
		goto bail;
	}

	config = malloc(sizeof(struct config));
	if (config == NULL) {
		mbus_errorf("can not allocate memory");
		goto bail;
	}
	memset(config, 0, sizeof(struct config));
	TAILQ_INIT(&config->apps);

	applications = cJSON_GetObjectItem(root, "applications");
	if (applications == NULL || applications->type != cJSON_Array) {
		mbus_errorf("can not parse config");
		goto bail;
	}
	for (i = 0; i < cJSON_GetArraySize(applications); i++) {
		application = cJSON_GetArrayItem(applications, i);
		if (application == NULL) {
			mbus_errorf("can not parse config");
			goto bail;
		}
		app = app_create(application);
		if (app == NULL) {
			mbus_errorf("can not parse config");
			goto bail;
		}
		TAILQ_INSERT_TAIL(&config->apps, app, apps);
	}

	free(buffer);
	cJSON_Delete(root);
	return config;
bail:	if (buffer != NULL) {
		free(buffer);
	}
	if (root != NULL) {
		cJSON_Delete(root);
	}
	if (config != NULL) {
		config_destroy(config);
	}
	return NULL;
}

static int apps_execute (struct apps *apps)
{
	int rc;
	struct app *app;
	TAILQ_FOREACH(app, apps, apps) {
		rc = app_execute(app);
		if (rc != 0) {
			mbus_errorf("can not execute app");
			goto bail;
		}
	}
	return 0;
bail:	return -1;
}

static int apps_run (struct apps *apps)
{
	int rc;
	int wl;
	pid_t pid;
	struct app *app;
	char buffer[256];
	int p;
	int npollfd;
	struct pollfd *pollfd;
	pollfd = malloc(sizeof(struct pollfd) * apps->count);
	if (pollfd == NULL) {
		mbus_errorf("can not allocate memory");
		goto bail;
	}
	while (1) {
		TAILQ_FOREACH(app, apps, apps) {
			mbus_debugf("app name: %s, enable: %d, state: %d", app->name, app->enable, app->state);
			if (app->enable == 0) {
				continue;
			}
			if (app->state != app_state_running) {
				mbus_errorf("restarting app: %s", app->name);
				rc = app_execute(app);
				if (rc != 0) {
					mbus_errorf("can not execute app");
				}
				continue;
			} else {
				pid = mbus_waitpid(app->pid, NULL, mbus_waitpid_option_nohang);
				if (pid > 0) {
					mbus_errorf("app is closed: %s, io: %d, %d", app->name, app->io[0], app->io[1]);
					app->state = app_state_stopped;
					if (app->io[0] >= 0) {
						close(app->io[0]);
						app->io[0] = -1;
					}
					if (app->io[1] >= 0) {
						close(app->io[1]);
						app->io[1] = -1;
					}
					if (app->logfd >= 0) {
						close(app->logfd);
						app->logfd = -1;
					}
					app->pid = -1;
				}
			}
		}
		npollfd = 0;
		TAILQ_FOREACH(app, apps, apps) {
			if (app->enable == 0) {
				continue;
			}
			if (app->state != app_state_running) {
				continue;
			}
			if (app->io[0] < 0) {
				continue;
			}
			if (app->io[1] < 0) {
				continue;
			}
			pollfd[npollfd].fd = app->io[0];
			pollfd[npollfd].events = POLLIN;
			pollfd[npollfd].revents = 0;
			npollfd += 1;
		}
		rc = poll(pollfd, npollfd, 100);
		if (rc < 0) {
			mbus_errorf("poll failed");
			goto bail;
		}
		if (rc == 0) {
			continue;
		}
		for (p = 0; p < npollfd; p++) {
			if (pollfd[p].revents == 0) {
				continue;
			}
			TAILQ_FOREACH(app, apps, apps) {
				if (app->io[0] == pollfd[p].fd) {
					break;
				}
			}
			if (app == NULL) {
				mbus_errorf("invalid fd");
				goto bail;
			}
			rc = read(app->io[0], buffer, sizeof(buffer) - 1);
			if (rc > 0) {
				buffer[rc] = '\0';
				mbus_debugf("%s", buffer);
				if (app->logfd > 0) {
					wl = write(app->logfd, buffer, rc);
					if (wl != rc) {
						mbus_errorf("can not write log");
					}
				}
			}
		}
	}
	free(pollfd);
	return 0;
bail:	if (pollfd != NULL) {
		free(pollfd);
	}
	return -1;
}

int main (int argc, char *argv[])
{
	int rc;
	int ch;
	const char *file;
	struct config *config;
	file = NULL;
	config = NULL;
	while ((ch = getopt_long(argc, argv, ":", longopts, NULL)) != -1) {
		switch (ch) {
			case OPTION_DEBUG_LEVEL:
				mbus_debug_level = mbus_debug_level_from_string(optarg);
				break;
			case OPTION_CONFIG:
				file = optarg;
				break;
			case OPTION_HELP:
				usage();
				goto bail;
		}
	}
	if (file == NULL) {
		mbus_errorf("file is null");
		goto bail;
	}
	config = config_create(file);
	if (config == NULL) {
		mbus_errorf("can not parse config");
		goto bail;
	}
	rc = apps_execute(&config->apps);
	if (rc != 0) {
		mbus_errorf("can not execute apps");
		goto bail;
	}
	rc = apps_run(&config->apps);
	if (rc != 0) {
		mbus_errorf("can not run apps");
		goto bail;
	}
	//sleep(100);
	config_destroy(config);
	return 0;
bail:	return -1;
}
