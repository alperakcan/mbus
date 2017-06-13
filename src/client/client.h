
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

#define MBUS_CLIENT_DEFAULT_TIMEOUT		250

#define MBUS_CLIENT_DEFAULT_PING_INTERVAL	10000
#define MBUS_CLIENT_DEFAULT_PING_TIMEOUT	5000
#define MBUS_CLIENT_DEFAULT_PING_THRESHOLD	2

struct mbus_client_options {
	struct {
		const char *protocol;
		const char *address;
		int port;
	} server;
	struct {
		const char *name;
	} client;
	struct {
		int interval;
		int timeout;
		int threshold;
	} ping;
};

struct mbus_json;
struct mbus_client;

void mbus_client_usage (void);

int mbus_client_options_default (struct mbus_client_options *options);
int mbus_client_options_from_argv (struct mbus_client_options *options, int argc, char *argv[]);

struct mbus_client * mbus_client_create (const char *name, int argc, char *argv[]);
struct mbus_client * mbus_client_create_with_options (const struct mbus_client_options *options);
void mbus_client_destroy (struct mbus_client *client);

const char * mbus_client_name (struct mbus_client *client);

int mbus_client_subscribe (struct mbus_client *client, const char *source, const char *event, void (*function) (struct mbus_client *client, const char *source, const char *event, struct mbus_json *payload, void *data), void *data);
int mbus_client_register (struct mbus_client *client, const char *command, int (*function) (struct mbus_client *client, const char *source, const char *command, struct mbus_json *payload, struct mbus_json *result, void *data), void *data);
int mbus_client_event (struct mbus_client *client, const char *identifier, const struct mbus_json *event);
int mbus_client_event_to (struct mbus_client *client, const char *to, const char *identifier, const struct mbus_json *event);
int mbus_client_event_async (struct mbus_client *client, const char *identifier, const struct mbus_json *event);
int mbus_client_event_async_to (struct mbus_client *client, const char *to, const char *identifier, const struct mbus_json *event);
int mbus_client_command (struct mbus_client *client, const char *destination, const char *command, struct mbus_json *call, struct mbus_json **result);
int mbus_client_command_timeout (struct mbus_client *client, const char *destination, const char *command, struct mbus_json *call, struct mbus_json **result, int timeout);

int mbus_client_run (struct mbus_client *client);
int mbus_client_run_timeout (struct mbus_client *client, int milliseconds);
int mbus_client_break (struct mbus_client *client);
