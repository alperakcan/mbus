
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

#define MBUS_SERVER_TCP_PROTOCOL		"tcp"
#define MBUS_SERVER_TCP_PORT			8000
#define MBUS_SERVER_TCP_ADDRESS			"127.0.0.1"

#define MBUS_SERVER_UDS_PROTOCOL		"uds"
#define MBUS_SERVER_UDS_PORT			-1
#define MBUS_SERVER_UDS_ADDRESS			"/tmp/mbus-server"

#define MBUS_SERVER_PROTOCOL			MBUS_SERVER_UDS_PROTOCOL
#define MBUS_SERVER_PORT			MBUS_SERVER_UDS_PORT
#define MBUS_SERVER_ADDRESS			MBUS_SERVER_UDS_ADDRESS

#define MBUS_SERVER_NAME			"org.mbus.server"

/* command event
 *
 * {
 *   "comment": "event specific json object goes here"
 * }
 */
#define MBUS_SERVER_COMMAND_EVENT		"command.event"

/* command call
 *
 * {
 *   "comment": "call specific json object goes here"
 * }
 */
#define MBUS_SERVER_COMMAND_CALL		"command.call"

/* command result
 *
 * {
 *   "comment": "call specific json object goes here"
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
 *       "subscription": [
 *         {
 *           "source": "application name",
 *           "identifier": "event name"
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

/* command subscribe
 *
 * {
 *     "source": "application name",
 *     "event" : "event name"
 * }
 */
#define MBUS_SERVER_COMMAND_SUBSCRIBE		"command.subscribe"

/* command register
 *
 * {
 *     "command" : "command name"
 * }
 */
#define MBUS_SERVER_COMMAND_REGISTER		"command.register"

/* command unsubscribe
 *
 * {
 *     "source": "application name",
 *     "event" : "event name"
 * }
 */
#define MBUS_SERVER_COMMAND_UNSUBSCRIBE		"command.unsubscribe"

/* command close
 *
 * input:
 *   {
 *     "source": "application name"
 *   }
 *
 * output:
 * {
 * }
 */
#define MBUS_SERVER_COMMAND_CLOSE		"command.close"

/* status connected
 *
 * {
 *   "source": "application name"
 * }
 */
#define MBUS_SERVER_STATUS_CONNECTED		"status.connected"

/* status disconnected
 *
 * {
 *   "source": "application name"
 * }
 */
#define MBUS_SERVER_STATUS_DISCONNECTED		"status.disconnected"

/* status subscribed
 *
 * {
 *   "destination" : "application name",
 *   "identifier"  : "status name"
 * }
 */
#define MBUS_SERVER_STATUS_SUBSCRIBED		"status.subscribed"

/* status subscriber
 *
 * {
 *   "source"      : "application name",
 *   "identifier"  : "status name"
 * }
 */
#define MBUS_SERVER_STATUS_SUBSCRIBER		"status.subscriber"

/* status unsubscribed
 *
 * {
 *   "source"      : "application name",
 *   "destination" : "application name",
 *   "identifier"  : "status name"
 * }
 */
#define MBUS_SERVER_STATUS_UNSUBSCRIBED		"status.unsubscribed"

/* event connected
 *
 * {
 *   "source": "application name"
 * }
 */
#define MBUS_SERVER_EVENT_CONNECTED		"event.connected"

/* event disconnected
 *
 * {
 *   "source": "application name"
 * }
 */
#define MBUS_SERVER_EVENT_DISCONNECTED		"event.disconnected"

/* event subscribed
 *
 * {
 *   "source"      : "application name",
 *   "destination" : "application name",
 *   "identifier"  : "event name"
 * }
 */
#define MBUS_SERVER_EVENT_SUBSCRIBED		"event.subscribed"

/* event unsubscribed
 *
 * {
 *   "source"      : "application name",
 *   "destination" : "application name",
 *   "identifier"  : "event name"
 * }
 */
#define MBUS_SERVER_EVENT_UNSUBSCRIBED		"event.unsubscribed"

struct mbus_server;

struct mbus_server * mbus_server_create (int argc, char *argv[]);
void mbus_server_destroy (struct mbus_server *server);

int mbus_server_run (struct mbus_server *server);
int mbus_server_run_timeout (struct mbus_server *server, int milliseconds);
