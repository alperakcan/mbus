
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
#include <unistd.h>

#define MBUS_DEBUG_NAME	"app-listener"

#include "mbus/debug.h"
#include "mbus/cJSON.h"
#include "mbus/method.h"
#include "mbus/client.h"
#include "mbus/server.h"
#include "listener.h"

static void listener_event_all_all (struct mbus_client *client, const char *source, const char *event, cJSON *payload, void *data)
{
	char *string;
	(void) client;
	(void) source;
	(void) event;
	(void) payload;
	(void) data;
	string = cJSON_Print(payload);
	if (string == NULL) {
		mbus_errorf("can not allocate memory");
	} else {
		fprintf(stdout, "%s.%s: %s\n", source, event, string);
		free(string);
	}
}

static void listener_status_server_connected (struct mbus_client *client, const char *source, const char *status, cJSON *payload, void *data)
{
	int rc;
	char *string;
	cJSON *result;
	(void) client;
	(void) source;
	(void) status;
	(void) payload;
	(void) data;
	rc = mbus_client_command(client, MBUS_SERVER_NAME, MBUS_SERVER_COMMAND_STATUS, NULL, &result);
	if (rc != 0) {
		mbus_errorf("can not call command");
	}
	if (result != NULL) {
		string = cJSON_Print(result);
		if (string == NULL) {
			return;
		}
		fprintf(stdout, "%s.%s: %s\n", MBUS_SERVER_NAME, MBUS_SERVER_COMMAND_STATUS, string);
		free(string);
		cJSON_Delete(result);
	}
}

static void listener_status_server_subscribed (struct mbus_client *client, const char *source, const char *status, cJSON *payload, void *data)
{
	(void) client;
	(void) source;
	(void) status;
	(void) payload;
	(void) data;
}

int main (int argc, char *argv[])
{
	int rc;
	struct mbus_client *client;
	client = mbus_client_create(MBUS_APP_LISTENER_NAME, argc, argv);
	if (client == NULL) {
		mbus_errorf("can not create client");
		goto bail;
	}
	rc = mbus_client_subscribe(client, MBUS_METHOD_EVENT_SOURCE_ALL, MBUS_METHOD_EVENT_IDENTIFIER_ALL, listener_event_all_all, NULL);
	if (rc != 0) {
		mbus_errorf("can not subscribe to events");
		goto bail;
	}
	rc = mbus_client_subscribe(client, MBUS_SERVER_NAME, MBUS_SERVER_STATUS_CONNECTED, listener_status_server_connected, NULL);
	if (rc != 0) {
		mbus_errorf("can not subscribe to events");
		goto bail;
	}
	rc = mbus_client_subscribe(client, MBUS_SERVER_NAME, MBUS_SERVER_STATUS_SUBSCRIBED, listener_status_server_subscribed, NULL);
	if (rc != 0) {
		mbus_errorf("can not subscribe to events");
		goto bail;
	}
	rc = mbus_client_run(client);
	if (rc != 0) {
		mbus_errorf("client run failed");
		goto bail;
	}
	mbus_client_destroy(client);
	return 0;
bail:	if (client != NULL) {
		mbus_client_destroy(client);
	}
	return -1;
}
