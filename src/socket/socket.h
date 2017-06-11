
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

enum mbus_socket_domain {
	mbus_socket_domain_af_inet,
	mbus_socket_domain_af_unix,
};

enum mbus_socket_type {
	mbus_socket_type_sock_stream,
};

enum mbus_socket_protocol {
	mbus_socket_protocol_any,
};

struct mbus_socket;

struct mbus_socket * mbus_socket_create (enum mbus_socket_domain domain, enum mbus_socket_type type, enum mbus_socket_protocol protocol);
void mbus_socket_destroy (struct mbus_socket *socket);

void mbus_socket_close (struct mbus_socket *socket);

int mbus_socket_get_fd (struct mbus_socket *socket);

int mbus_socket_set_reuseaddr (struct mbus_socket *socket, int on);
int mbus_socket_get_reuseaddr (struct mbus_socket *socket);

int mbus_socket_set_blocking (struct mbus_socket *socket, int on);
int mbus_socket_get_blocking (struct mbus_socket *socket);

int mbus_socket_set_keepalive (struct mbus_socket *socket, int value);
int mbus_socket_get_keepalive (struct mbus_socket *socket);

int mbus_socket_set_keepcnt (struct mbus_socket *socket, int value);
int mbus_socket_get_keepcnt (struct mbus_socket *socket);

int mbus_socket_set_keepidle (struct mbus_socket *socket, int value);
int mbus_socket_get_keepidle (struct mbus_socket *socket);

int mbus_socket_set_keepintvl (struct mbus_socket *socket, int value);
int mbus_socket_get_keepintvl (struct mbus_socket *socket);

int mbus_socket_bind (struct mbus_socket *socket, const char *address, unsigned short port);
int mbus_socket_listen (struct mbus_socket *socket, int backlog);
struct mbus_socket * mbus_socket_accept (struct mbus_socket *socket);

int mbus_socket_connect (struct mbus_socket *socket, const char *address, unsigned short port);

int mbus_socket_read (struct mbus_socket *socket, void *vptr, int n);
int mbus_socket_write (struct mbus_socket *socket, const void *vptr, int n);
