
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

#define MBUS_SERVER_TCP_ENABLE			1
#define MBUS_SERVER_TCP_PROTOCOL		"tcp"
#define MBUS_SERVER_TCP_PORT			8000
#define MBUS_SERVER_TCP_ADDRESS			"127.0.0.1"

#define MBUS_SERVER_UDS_ENABLE			1
#define MBUS_SERVER_UDS_PROTOCOL		"uds"
#define MBUS_SERVER_UDS_PORT			0
#define MBUS_SERVER_UDS_ADDRESS			"/tmp/mbus-server-uds"

#define MBUS_SERVER_WS_ENABLE			1
#define MBUS_SERVER_WS_PROTOCOL			"ws"
#define MBUS_SERVER_WS_PORT			9000
#define MBUS_SERVER_WS_ADDRESS			"127.0.0.1"

#define MBUS_SERVER_TCPS_ENABLE			1
#define MBUS_SERVER_TCPS_PROTOCOL		"tcps"
#define MBUS_SERVER_TCPS_PORT			8001
#define MBUS_SERVER_TCPS_ADDRESS		"127.0.0.1"
#define MBUS_SERVER_TCPS_CERTIFICATE		"server.crt"
#define MBUS_SERVER_TCPS_PRIVATEKEY		"server.key"

#define MBUS_SERVER_UDSS_ENABLE			1
#define MBUS_SERVER_UDSS_PROTOCOL		"udss"
#define MBUS_SERVER_UDSS_PORT			0
#define MBUS_SERVER_UDSS_ADDRESS		"/tmp/mbus-server-udss"
#define MBUS_SERVER_UDSS_CERTIFICATE		"server.crt"
#define MBUS_SERVER_UDSS_PRIVATEKEY		"server.key"

#define MBUS_SERVER_WSS_ENABLE			1
#define MBUS_SERVER_WSS_PROTOCOL		"wss"
#define MBUS_SERVER_WSS_PORT			9001
#define MBUS_SERVER_WSS_ADDRESS			"127.0.0.1"
#define MBUS_SERVER_WSS_CERTIFICATE		"server.crt"
#define MBUS_SERVER_WSS_PRIVATEKEY		"server.key"

#define MBUS_SERVER_PROTOCOL			"uds"
#define MBUS_SERVER_ADDRESS			MBUS_SERVER_UDS_ADDRESS
#define MBUS_SERVER_PORT			MBUS_SERVER_UDS_PORT

#define MBUS_SERVER_DEFAULT_TIMEOUT		10000

#define MBUS_SERVER_IDENTIFIER			"org.mbus.server"
#define MBUS_SERVER_CLIENT_IDENTIFIER_PREFIX	"org.mbus.client."

/* command create
 *
 * input:
 * {
 *   "identifier": "identifier",
 *   "ping": {
 *     "interval": interval
 *     "timeout": timeout
 *     "threshold": threshold
 *   },
 *   "compressions": {
 *     "none",
 *     "zlib"
 *   }
 * }
 *
 * output:
 * {
 *   "identifier": "identifier",
 *   "ping": {
 *     "interval": interval
 *     "timeout": timeout
 *     "threshold": threshold
 *   },
 *   "compression": compression
 * }
 */
#define MBUS_SERVER_COMMAND_CREATE		"command.create"

/* command event
 *
 * input:
 * {
 *   "destination": "destination",
 *   "identifier": "identifier",
 *   "payload"     : {
 *     "comment": "event specific data object goes here"
 *   }
 * }
 *
 * output:
 * {
 * }
 */
#define MBUS_SERVER_COMMAND_EVENT		"command.event"

/* command result
 *
 * input:
 * {
 *   "destination": "destination",
 *   "identifier": "identifier",
 *   "sequence": sequence,
 *   "status": status
 *   "payload"     : {
 *     "comment": "command specific data object goes here"
 *   }
 * }
 *
 * output:
 * {
 * }
 */
#define MBUS_SERVER_COMMAND_RESULT		"command.result"

/* command status
 *
 * input:
 * {
 * }
 *
 * output:
 * {
 *   "clients": [
 *     {
 *       "source": "application name",
 *       "subscriptions": [
 *         {
 *           "source": "application name",
 *           "identifier": "event name"
 *         }
 *         ..
 *         .
 *       ],
 *       "commands": [
 *         {
 *           "identifier": "command name"
 *         }
 *         ..
 *         .
 *       ]
 *     },
 *     ..
 *     .
 *   ]
 * }
 */
#define MBUS_SERVER_COMMAND_STATUS		"command.status"

/* command status
 *
 * input:
 * {
 * }
 *
 * output:
 * {
 *   "clients": [
 *     "identifier",
 *     ..
 *     .
 *   ]
 * }
 */
#define MBUS_SERVER_COMMAND_CLIENTS		"command.clients"

/* command status
 *
 * input:
 * {
 *   "source": "application name",
 * }
 *
 * output:
 * {
 *   "source": "application name",
 *   "subscriptions": [
 *     {
 *       "source": "application name",
 *       "identifier": "event name"
 *     }
 *     ..
 *     .
 *   ],
 *   "commands": [
 *     {
 *       "identifier": "command name"
 *     }
 *     ..
 *     .
 *   ]
 * }
 */
#define MBUS_SERVER_COMMAND_CLIENT		"command.client"

/* command subscribe
 *
 * input:
 * {
 *     "source": "application name",
 *     "event" : "event name"
 * }
 *
 * output:
 * {
 * }
 */
#define MBUS_SERVER_COMMAND_SUBSCRIBE		"command.subscribe"

/* command unsubscribe
 *
 * input:
 * {
 *     "source": "application name",
 *     "event" : "event name"
 * }
 *
 * output:
 * {
 * }
 */
#define MBUS_SERVER_COMMAND_UNSUBSCRIBE		"command.unsubscribe"

/* command register
 *
 * input:
 * {
 *     "command" : "command name"
 * }
 *
 * output:
 * {
 * }
 */
#define MBUS_SERVER_COMMAND_REGISTER		"command.register"

/* command unregister
 *
 * input:
 * {
 *     "command" : "command name"
 * }
 *
 * output:
 * {
 * }
 */
#define MBUS_SERVER_COMMAND_UNREGISTER		"command.unregister"

/* command close
 *
 * input:
 * {
 *   "source": "application name"
 * }
 *
 * output:
 * {
 * }
 */
#define MBUS_SERVER_COMMAND_CLOSE		"command.close"

/* event ping
 *
 * {
 *   "source"      : "application name",
 * }
 */
#define MBUS_SERVER_EVENT_PING			"org.mbus.server.event.ping"

/* event pong
 *
 * {
 *   "source"      : "application name",
 * }
 */
#define MBUS_SERVER_EVENT_PONG			"org.mbus.server.event.pong"

/* event connected
 *
 * {
 *   "source": "application name"
 * }
 */
#define MBUS_SERVER_EVENT_CONNECTED		"org.mbus.server.event.connected"

/* event disconnected
 *
 * {
 *   "source": "application name"
 * }
 */
#define MBUS_SERVER_EVENT_DISCONNECTED		"org.mbus.server.event.disconnected"

/* event subscribed
 *
 * {
 *   "source"      : "application name",
 *   "destination" : "application name",
 *   "identifier"  : "event name"
 * }
 */
#define MBUS_SERVER_EVENT_SUBSCRIBED		"org.mbus.server.event.subscribed"

/* event unsubscribed
 *
 * {
 *   "source"      : "application name",
 *   "destination" : "application name",
 *   "identifier"  : "event name"
 * }
 */
#define MBUS_SERVER_EVENT_UNSUBSCRIBED		"org.mbus.server.event.unsubscribed"

struct mbus_server;

struct mbus_server_options {
	struct {
		int enabled;
		const char *address;
		unsigned short port;
	} tcp;
	struct {
		int enabled;
		const char *address;
		unsigned short port;
	} uds;
	struct {
		int enabled;
		const char *address;
		unsigned short port;
	} ws;
	struct {
		int enabled;
		const char *address;
		unsigned short port;
		const char *certificate;
		const char *privatekey;
	} tcps;
	struct {
		int enabled;
		const char *address;
		unsigned short port;
		const char *certificate;
		const char *privatekey;
	} udss;
	struct {
		int enabled;
		const char *address;
		unsigned short port;
		const char *certificate;
		const char *privatekey;
	} wss;
};

void mbus_server_usage (void);

int mbus_server_options_default (struct mbus_server_options *options);
int mbus_server_options_from_argv (struct mbus_server_options *options, int argc, char *argv[]);

struct mbus_server * mbus_server_create (int argc, char *argv[]);
struct mbus_server * mbus_server_create_with_options (const struct mbus_server_options *options);
void mbus_server_destroy (struct mbus_server *server);

int mbus_server_run (struct mbus_server *server);
int mbus_server_run_timeout (struct mbus_server *server, int milliseconds);

int mbus_server_tcp_enabled (struct mbus_server *server);
const char * mbus_server_tcp_address (struct mbus_server *server);
int mbus_server_tcp_port (struct mbus_server *server);

int mbus_server_uds_enabled (struct mbus_server *server);
const char * mbus_server_uds_address (struct mbus_server *server);
int mbus_server_uds_port (struct mbus_server *server);

int mbus_server_ws_enabled (struct mbus_server *server);
const char * mbus_server_ws_address (struct mbus_server *server);
int mbus_server_ws_port (struct mbus_server *server);

int mbus_server_tcps_enabled (struct mbus_server *server);
const char * mbus_server_tcps_address (struct mbus_server *server);
int mbus_server_tcps_port (struct mbus_server *server);

int mbus_server_udss_enabled (struct mbus_server *server);
const char * mbus_server_udss_address (struct mbus_server *server);
int mbus_server_udss_port (struct mbus_server *server);

int mbus_server_wss_enabled (struct mbus_server *server);
const char * mbus_server_wss_address (struct mbus_server *server);
int mbus_server_wss_port (struct mbus_server *server);
