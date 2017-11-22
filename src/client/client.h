
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

#define MBUS_CLIENT_DEFAULT_RUN_TIMEOUT		250

#define MBUS_CLIENT_DEFAULT_CONNECT_TIMEOUT	30000
#define MBUS_CLIENT_DEFAULT_CONNECT_INTERVAL	0
#define MBUS_CLIENT_DEFAULT_SUBSCRIBE_TIMEOUT	180000
#define MBUS_CLIENT_DEFAULT_REGISTER_TIMEOUT	180000
#define MBUS_CLIENT_DEFAULT_COMMAND_TIMEOUT	180000
#define MBUS_CLIENT_DEFAULT_PUBLISH_TIMEOUT	180000

#define MBUS_CLIENT_DEFAULT_PING_INTERVAL	180000
#define MBUS_CLIENT_DEFAULT_PING_TIMEOUT	5000
#define MBUS_CLIENT_DEFAULT_PING_THRESHOLD	2

struct mbus_json;
struct mbus_client;
struct mbus_client_message;

enum mbus_client_state {
	mbus_client_state_unknown,
	mbus_client_state_connecting,
	mbus_client_state_connected,
	mbus_client_state_disconnecting,
	mbus_client_state_disconnected
};

enum mbus_client_connect_status {
	mbus_client_connect_status_success,
	mbus_client_connect_status_internal_error,
	mbus_client_connect_status_invalid_protocol,
	mbus_client_connect_status_connection_refused,
	mbus_client_connect_status_server_unavailable,
	mbus_client_connect_status_timeout,
	mbus_client_connect_status_invalid_protocol_version,
	mbus_client_connect_status_invalid_client_identfier,
	mbus_client_connect_status_server_error,
};

enum mbus_client_disconnect_status {
	mbus_client_disconnect_status_success,
	mbus_client_disconnect_status_internal_error,
	mbus_client_disconnect_status_connection_closed
};

enum mbus_client_publish_status {
	mbus_client_publish_status_success,
	mbus_client_publish_status_internal_error,
	mbus_client_publish_status_timeout
};

enum mbus_client_subscribe_status {
	mbus_client_subscribe_status_success,
	mbus_client_subscribe_status_internal_error,
	mbus_client_subscribe_status_timeout
};

enum mbus_client_unsubscribe_status {
	mbus_client_unsubscribe_status_success,
	mbus_client_unsubscribe_status_internal_error,
	mbus_client_unsubscribe_status_timeout
};

enum mbus_client_register_status {
	mbus_client_register_status_success,
	mbus_client_register_status_internal_error,
	mbus_client_register_status_timeout
};

enum mbus_client_unregister_status {
	mbus_client_unregister_status_success,
	mbus_client_unregister_status_internal_error,
	mbus_client_unregister_status_timeout
};

enum mbus_client_command_status {
	mbus_client_command_status_success,
	mbus_client_command_status_internal_error,
	mbus_client_command_status_timeout
};

struct mbus_client_options {
	char *name;
	char *server_protocol;
	char *server_address;
	int server_port;
	int connect_timeout;
	int connect_interval;
	int subscribe_timeout;
	int register_timeout;
	int command_timeout;
	int publish_timeout;
	int ping_interval;
	int ping_timeout;
	int ping_threshold;
	struct {
		void (*connect) (struct mbus_client *client, void *context, enum mbus_client_connect_status status);
		void (*disconnect) (struct mbus_client *client, void *context, enum mbus_client_disconnect_status status);
		void (*message) (struct mbus_client *client, void *context, struct mbus_client_message *message);
		int (*routine) (struct mbus_client *client, void *context, struct mbus_client_message *message);
		void (*publish) (struct mbus_client *client, void *context, struct mbus_client_message *message, enum mbus_client_publish_status status);
		void (*subscribe) (struct mbus_client *client, void *context, const char *source, const char *event, enum mbus_client_subscribe_status status);
		void (*unsubscribe) (struct mbus_client *client, void *context, const char *source, const char *event, enum mbus_client_unsubscribe_status status);
		void (*registered) (struct mbus_client *client, void *context, const char *command, enum mbus_client_register_status status);
		void (*unregistered) (struct mbus_client *client, void *context, const char *command, enum mbus_client_unregister_status status);
		void *context;
	} callbacks;
};

void mbus_client_usage (void);

int mbus_client_options_default (struct mbus_client_options *options);
int mbus_client_options_from_argv (struct mbus_client_options *options, int argc, char *argv[]);

struct mbus_client * mbus_client_create (const struct mbus_client_options *options);
void mbus_client_destroy (struct mbus_client *client);

int mbus_client_lock (struct mbus_client *client);
int mbus_client_unlock (struct mbus_client *client);

enum mbus_client_state mbus_client_get_state (struct mbus_client *client);
const char * mbus_client_get_name (struct mbus_client *client);
int mbus_client_get_wakeup_fd (struct mbus_client *client);
int mbus_client_get_wakeup_fd_events (struct mbus_client *client);
int mbus_client_get_connection_fd (struct mbus_client *client);
int mbus_client_get_connection_fd_events (struct mbus_client *client);
int mbus_client_has_pending (struct mbus_client *client);

int mbus_client_connect (struct mbus_client *client);
int mbus_client_disconnect (struct mbus_client *client);

int mbus_client_subscribe (struct mbus_client *client, const char *source, const char *event);
int mbus_client_subscribe_timeout (struct mbus_client *client, const char *source, const char *event, int timeout);
int mbus_client_subscribe_callback (struct mbus_client *client, const char *source, const char *event, void (*callback) (struct mbus_client *client, void *context, struct mbus_client_message *message), void *context);
int mbus_client_subscribe_callback_timeout (struct mbus_client *client, const char *source, const char *event, void (*callback) (struct mbus_client *client, void *context, struct mbus_client_message *message), void *context, int timeout);

int mbus_client_unsubscribe (struct mbus_client *client, const char *source, const char *event);
int mbus_client_unsubscribe_timeout (struct mbus_client *client, const char *source, const char *event, int timeout);

int mbus_client_publish (struct mbus_client *client, const char *event, const struct mbus_json *payload);
int mbus_client_publish_unlocked (struct mbus_client *client, const char *event, const struct mbus_json *payload);
int mbus_client_publish_timeout (struct mbus_client *client, const char *event, const struct mbus_json *payload, int timeout);
int mbus_client_publish_timeout_unlocked (struct mbus_client *client, const char *event, const struct mbus_json *payload, int timeout);
int mbus_client_publish_to (struct mbus_client *client, const char *destination, const char *event, const struct mbus_json *payload);
int mbus_client_publish_to_unlocked (struct mbus_client *client, const char *destination, const char *event, const struct mbus_json *payload);
int mbus_client_publish_to_timeout (struct mbus_client *client, const char *destination, const char *event, const struct mbus_json *payload, int timeout);
int mbus_client_publish_to_timeout_unlocked (struct mbus_client *client, const char *destination, const char *event, const struct mbus_json *payload, int timeout);

int mbus_client_publish_sync (struct mbus_client *client, const char *event, const struct mbus_json *payload);
int mbus_client_publish_sync_unlocked (struct mbus_client *client, const char *event, const struct mbus_json *payload);
int mbus_client_publish_sync_timeout (struct mbus_client *client, const char *event, const struct mbus_json *payload, int timeout);
int mbus_client_publish_sync_timeout_unlocked (struct mbus_client *client, const char *event, const struct mbus_json *payload, int timeout);
int mbus_client_publish_sync_to (struct mbus_client *client, const char *destination, const char *event, const struct mbus_json *payload);
int mbus_client_publish_sync_to_unlocked (struct mbus_client *client, const char *destination, const char *event, const struct mbus_json *payload);
int mbus_client_publish_sync_to_timeout (struct mbus_client *client, const char *destination, const char *event, const struct mbus_json *payload, int timeout);
int mbus_client_publish_sync_to_timeout_unlocked (struct mbus_client *client, const char *destination, const char *event, const struct mbus_json *payload, int timeout);

int mbus_client_command (struct mbus_client *client, const char *destination, const char *command, const struct mbus_json *payload, void (*callback) (struct mbus_client *client, void *context, struct mbus_client_message *message, enum mbus_client_command_status status), void *context);
int mbus_client_command_unlocked (struct mbus_client *client, const char *destination, const char *command, const struct mbus_json *payload, void (*callback) (struct mbus_client *client, void *context, struct mbus_client_message *message, enum mbus_client_command_status status), void *context);
int mbus_client_command_timeout (struct mbus_client *client, const char *destination, const char *command, const struct mbus_json *payload, void (*callback) (struct mbus_client *client, void *context, struct mbus_client_message *message, enum mbus_client_command_status status), void *context, int timeout);
int mbus_client_command_timeout_unlocked (struct mbus_client *client, const char *destination, const char *command, const struct mbus_json *payload, void (*callback) (struct mbus_client *client, void *context, struct mbus_client_message *message, enum mbus_client_command_status status), void *context, int timeout);

int mbus_client_register (struct mbus_client *client, const char *identifier);
int mbus_client_register_timeout (struct mbus_client *client, const char *identifier, int timeout);
int mbus_client_register_callback (struct mbus_client *client, const char *identifier, int (*callback) (struct mbus_client *client, void *context, struct mbus_client_message *message), void *context);
int mbus_client_register_callback_timeout (struct mbus_client *client, const char *identifier, int (*callback) (struct mbus_client *client, void *context, struct mbus_client_message *message), void *context, int timeout);

int mbus_client_unregister (struct mbus_client *client, const char *identifier);
int mbus_client_unregister_timeout (struct mbus_client *client, const char *identifier, int timeout);

int mbus_client_break (struct mbus_client *client);
int mbus_client_get_run_timeout (struct mbus_client *client);
int mbus_client_get_run_timeout_unlocked (struct mbus_client *client);
int mbus_client_run (struct mbus_client *client, int timeout);

const char * mbus_client_message_event_source (struct mbus_client_message *message);
const char * mbus_client_message_event_destination (struct mbus_client_message *message);
const char * mbus_client_message_event_identifier (struct mbus_client_message *message);
const struct mbus_json * mbus_client_message_event_payload (struct mbus_client_message *message);

const char * mbus_client_message_command_request_destination (struct mbus_client_message *message);
const char * mbus_client_message_command_request_identifier (struct mbus_client_message *message);
const struct mbus_json * mbus_client_message_command_request_payload (struct mbus_client_message *message);
const struct mbus_json * mbus_client_message_command_response_payload (struct mbus_client_message *message);
int mbus_client_message_command_response_result (struct mbus_client_message *message);

const struct mbus_json * mbus_client_message_routine_request_payload (struct mbus_client_message *message);
int mbus_client_message_routine_set_response_payload (struct mbus_client_message *message, const struct mbus_json *payload);

const char * mbus_client_state_string (enum mbus_client_state state);
const char * mbus_client_connect_status_string (enum mbus_client_connect_status status);
const char * mbus_client_disconnect_status_string (enum mbus_client_disconnect_status status);
const char * mbus_client_publish_status_string (enum mbus_client_publish_status status);
const char * mbus_client_subscribe_status_string (enum mbus_client_subscribe_status status);
const char * mbus_client_unsubscribe_status_string (enum mbus_client_unsubscribe_status status);
const char * mbus_client_command_status_string (enum mbus_client_command_status status);
