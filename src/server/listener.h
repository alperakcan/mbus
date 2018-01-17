
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

struct connection {
	TAILQ_ENTRY(listener) connections;
};
TAILQ_HEAD(connections, connection);

enum listener_type {
	listener_type_unknown,
	listener_type_tcp,
	listener_type_uds,
	listener_type_ws,
};

struct listener {
	TAILQ_ENTRY(listener) listeners;
};
TAILQ_HEAD(listeners, listener);

struct listener_tcp_options {
	const char *name;
	const char *address;
	unsigned short port;
	const char *certificate;
	const char *privatekey;
};

struct listener_uds_options {
	const char *name;
	const char *address;
	unsigned short port;
	const char *certificate;
	const char *privatekey;
};

struct listener_ws_callbacks {
	int (*connection_established) (void *context, struct listener *listener, struct connection *connection);
	int (*connection_receive) (void *context, struct listener *listener, struct connection *connection, void *in, int len);
	struct mbus_buffer * (*connection_writable) (void *context, struct listener *listener, struct connection *connection);
	int (*connection_closed) (void *context, struct listener *listener, struct connection *connection);
	int (*poll_add) (void *context, struct listener *listener, int fd, unsigned int events);
	int (*poll_mod) (void *context, struct listener *listener, int fd, unsigned int events);
	int (*poll_del) (void *context, struct listener *listener, int fd);
	void *context;
};

struct listener_ws_options {
	const char *name;
	const char *address;
	unsigned short port;
	const char *certificate;
	const char *privatekey;
	struct listener_ws_callbacks callbacks;
};

struct listener * listener_tcp_create (const struct listener_tcp_options *options);
struct listener * listener_uds_create (const struct listener_uds_options *options);
struct listener * listener_ws_create (const struct listener_ws_options *options);
void listener_destroy (struct listener *listener);

const char * listener_get_name (struct listener *listener);
enum listener_type listener_get_type (struct listener *listener);
int listener_get_fd (struct listener *listener);

struct connection * listener_connection_accept (struct listener *listener);
int connection_close (struct connection *connection);
int connection_get_fd (struct connection *connection);
int connection_get_want_read (struct connection *connection);
int connection_get_want_write (struct connection *connection);
