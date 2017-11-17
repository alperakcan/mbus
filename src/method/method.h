
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

#define MBUS_METHOD_TYPE_COMMAND				"org.mbus.method.type.command"
#define MBUS_METHOD_TYPE_EVENT					"org.mbus.method.type.event"
#define MBUS_METHOD_TYPE_RESULT					"org.mbus.method.type.result"

#define MBUS_METHOD_SEQUENCE_START				1
#define MBUS_METHOD_SEQUENCE_END				9999

#define MBUS_METHOD_EVENT_SOURCE_ALL				"org.mbus.method.event.source.all"

#define MBUS_METHOD_EVENT_DESTINATION_ALL			"org.mbus.method.event.destination.all"
#define MBUS_METHOD_EVENT_DESTINATION_SUBSCRIBERS		"org.mbus.method.event.destination.subscribers"

#define MBUS_METHOD_EVENT_IDENTIFIER_ALL			"org.mbus.method.event.identifier.all"

#define MBUS_METHOD_STATUS_IDENTIFIER_ALL			"org.mbus.method.event.status.all"

/* status json model
 *
 * server -- status --> client
 *
 * status: {
 *   "type"        : MBUS_METHOD_TYPE_STATUS,
 *   "source"      : MBUS_SERVER_NAME,
 *   "destination" : "unique identifier",
 *   "identifier"  : "unique identifier",
 *   "sequence"    : sequence number,
 *   "payload"     : {
 *     "comment": "event specific data object goes here"
 *   }
 * }
 */

/* event json model
 *
 * client  -- request --> server
 *
 * server:
 *   for each client
 *     for each subscribes
 *       if subscription matches with identifier
 *         push event to subscriber client queue
 *
 *   push result to source client queue
 *
 * server --  event   --> client(s)
 *
 * request: {
 *   "type"        : MBUS_METHOD_TYPE_EVENT,
 *   "destination" : "unique identifier",
 *   "identifier"  : "unique identifier",
 *   "sequence"    : sequence number,
 *   "payload"     : {
 *     "comment": "event specific data object goes here"
 *   }
 * }
 */

/* command json model
 *
 * client  -- request --> server
 *
 * server:
 *   if destination is MBUS_SERVER_NAME
 *     process command
 *   else
 *     for each client
 *       if client identifier matches with destination and
 *          client has registered command with identifier
 *         push call to client queue
 *
 * if destination is not MBUS_SERVER_NAME
 *   server  -- call    --> callee
 *   server <-- result  --  callee
 *
 * client <-- result  --  server
 *
 * request: {
 *   "type"        : MBUS_METHOD_TYPE_COMMAND,
 *   "destination" : "unique identifier",
 *   "identifier"  : "unique identifier",
 *   "sequence"    : sequence number,
 *   "payload"     : {
 *     "comment": "command specific data object goes here"
 *   }
 * }
 *
 * call: {
 *   "type"        : MBUS_METHOD_TYPE_COMMAND,
 *   "source"      : "unique identifier",
 *   "identifier"  : "unique identifier",
 *   "sequence"    : sequence number,
 *   "payload"        : {
 *     "comment": "call specific data object goes here"
 *   }
 * }
 *
 * result: {
 *   "type"        : MBUS_METHOD_TYPE_COMMAND,
 *   "destination" : MBUS_SERVER_NAME,
 *   "identifier"  : MBUS_SERVER_COMMAND_RESULT,
 *   "sequence"    : sequence number,
 *   "payload"     : {
 *     "destination" : "call source",
 *     "identifier"  : "call identifier",
 *     "sequence"    : call sequence number,
 *     "return"      : integer return code,
 *     "result"      : {
 *       "comment": "result specific data object goes here"
 *     }
 *   }
 * }
 */
