
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(SSL_ENABLE) && (SSL_ENABLE == 1)
#include <openssl/ssl.h>
#include <openssl/err.h>
#endif

#include <libwebsockets.h>

#include "mbus/debug.h"
#include "mbus/tailq.h"
#include "mbus/socket.h"
#include "mbus/compress.h"
#include "mbus/buffer.h"

#include "listener.h"

#define BUFFER_OUT_CHUNK_SIZE (64 * 1024)
static __attribute__((__unused__)) char __sizeof_check_buffer_out[BUFFER_OUT_CHUNK_SIZE < LWS_PRE ? -1 : 0];

struct connection_private {
	struct connection connection;
	int (*close) (struct connection *connection);
	int (*get_fd) (struct connection *connection);
	int (*get_want_read) (struct connection *connection);
	int (*get_want_write) (struct connection *connection);
};

struct listener_private {
	struct listener listener;
	const char * (*get_name) (struct listener *listener);
	enum listener_type (*get_type) (struct listener *listener);
	int (*get_fd) (struct listener *listener);
	struct connection * (*connection_accept) (struct listener *listener);
	void (*destroy) (struct listener *listener);
};

struct connection_tcp {
	struct connection_private private;
	struct mbus_socket *socket;
#if defined(SSL_ENABLE) && (SSL_ENABLE == 1)
	SSL *ssl;
	int want_read;
	int want_write;
#endif
};

struct listener_tcp {
	struct listener_private private;
	char *name;
	struct mbus_socket *socket;
#if defined(SSL_ENABLE) && (SSL_ENABLE == 1)
	SSL_CTX *ssl;
#endif
};

static const char * listener_tcp_get_name (struct listener *listener)
{
	struct listener_tcp *listener_tcp;
	if (listener == NULL) {
		mbus_errorf("listener is invalid");
		goto bail;
	}
	listener_tcp = (struct listener_tcp *) listener;
	return listener_tcp->name;
bail:	return NULL;
}

static enum listener_type listener_tcp_get_type (struct listener *listener)
{
	if (listener == NULL) {
		mbus_errorf("listener is invalid");
		goto bail;
	}
	return listener_type_tcp;
bail:	return listener_type_unknown;
}

static int listener_tcp_get_fd (struct listener *listener)
{
	struct listener_tcp *listener_tcp;
	if (listener == NULL) {
		mbus_errorf("listener is invalid");
		goto bail;
	}
	listener_tcp = (struct listener_tcp *) listener;
	return mbus_socket_get_fd(listener_tcp->socket);
bail:	return -1;
}

static int connection_tcp_close (struct connection *connection)
{
	struct connection_tcp *connection_tcp;
	if (connection == NULL) {
		mbus_errorf("connection is invalid");
		goto bail;
	}
	connection_tcp = (struct connection_tcp *) connection;
	mbus_socket_shutdown(connection_tcp->socket, mbus_socket_shutdown_rdwr);
	mbus_socket_destroy(connection_tcp->socket);
#if defined(SSL_ENABLE) && (SSL_ENABLE == 1)
	if (connection_tcp->ssl != NULL) {
		SSL_free(connection_tcp->ssl);
	}
#endif
	free(connection_tcp);
	return 0;
bail:	return -1;
}

static int connection_tcp_get_fd (struct connection *connection)
{
	struct connection_tcp *connection_tcp;
	if (connection == NULL) {
		mbus_errorf("connection is invalid");
		goto bail;
	}
	connection_tcp = (struct connection_tcp *) connection;
	return mbus_socket_get_fd(connection_tcp->socket);
bail:	return -1;
}

static int connection_tcp_get_want_read (struct connection *connection)
{
	struct connection_tcp *connection_tcp;
	if (connection == NULL) {
		mbus_errorf("connection is invalid");
		goto bail;
	}
	connection_tcp = (struct connection_tcp *) connection;
	(void) connection_tcp;
	return 0;
bail:	return -1;
}

static int connection_tcp_get_want_write (struct connection *connection)
{
	struct connection_tcp *connection_tcp;
	if (connection == NULL) {
		mbus_errorf("connection is invalid");
		goto bail;
	}
	connection_tcp = (struct connection_tcp *) connection;
	(void) connection_tcp;
	return 0;
bail:	return -1;
}

static struct connection * listener_tcp_connection_accept (struct listener *listener)
{
	int rc;
	struct listener_tcp *listener_tcp;
	struct connection_tcp *connection_tcp;
	connection_tcp = NULL;
	if (listener == NULL) {
		mbus_errorf("listener is invalid");
		goto bail;
	}
	listener_tcp = (struct listener_tcp *) listener;
	connection_tcp = malloc(sizeof(struct connection_tcp));
	if (connection_tcp == NULL) {
		mbus_errorf("can not allocate memory");
		goto bail;
	}
	memset(connection_tcp, 0, sizeof(struct connection_tcp));
	connection_tcp->socket = mbus_socket_accept(listener_tcp->socket);
	if (connection_tcp->socket == NULL) {
		mbus_errorf("can not accept new socket connection");
		goto bail;
	}
	rc = mbus_socket_set_blocking(connection_tcp->socket, 0);
	if (rc != 0) {
		mbus_errorf("can not set socket to nonblocking");
		goto bail;
	}
#if defined(SSL_ENABLE) && (SSL_ENABLE == 1)
	if (listener_tcp->ssl != NULL) {
		connection_tcp->ssl = SSL_new(listener_tcp->ssl);
		if (connection_tcp->ssl == NULL) {
			mbus_errorf("can not create ssl");
			goto bail;
		}
		SSL_set_fd(connection_tcp->ssl, mbus_socket_get_fd(connection_tcp->socket));
		rc = SSL_accept(connection_tcp->ssl);
		if (rc <= 0) {
			int error;
			error = SSL_get_error(connection_tcp->ssl, rc);
			if (error == SSL_ERROR_WANT_READ) {
				connection_tcp->want_read = 1;
			} else if (error == SSL_ERROR_WANT_WRITE) {
				connection_tcp->want_write = 1;
			} else if (error == SSL_ERROR_SYSCALL) {
				connection_tcp->want_read = 1;
			} else {
				char ebuf[256];
				mbus_errorf("can not accept ssl: %d", error);
				error = ERR_get_error();
				while (error) {
					mbus_errorf("  error: %d, %s", error, ERR_error_string(error, ebuf));
					error = ERR_get_error();
				}
				goto bail;
			}
		}
	}
#endif
	connection_tcp->private.get_fd         = connection_tcp_get_fd;
	connection_tcp->private.get_want_read  = connection_tcp_get_want_read;
	connection_tcp->private.get_want_write = connection_tcp_get_want_write;
	return &connection_tcp->private.connection;
bail:	if (connection_tcp != NULL) {
		connection_tcp_close(&connection_tcp->private.connection);
	}
	return NULL;
}


static void listener_tcp_destroy (struct listener *listener)
{
	struct listener_tcp *listener_tcp;
	if (listener == NULL) {
		return;
	}
	listener_tcp = (struct listener_tcp *) listener;
	if (listener_tcp->name != NULL) {
		free(listener_tcp->name);
	}
	if (listener_tcp->socket != NULL) {
		mbus_socket_destroy(listener_tcp->socket);
	}
#if defined(SSL_ENABLE) && (SSL_ENABLE == 1)
	if (listener_tcp->ssl != NULL) {
		SSL_CTX_free(listener_tcp->ssl);
	}
#endif
}

struct listener * listener_tcp_create (const struct listener_tcp_options *options)
{
	int rc;
	struct listener_tcp *listener_tcp;
	listener_tcp = NULL;
	if (options == NULL) {
		mbus_errorf("options is invalid");
		goto bail;
	}
	if (options->name == NULL) {
		mbus_errorf("name is invalid");
		goto bail;
	}
	if (options->address == NULL) {
		mbus_errorf("address is invalid");
		goto bail;
	}
	if (options->port <= 0) {
		mbus_errorf("port is invalid");
		goto bail;
	}
	if (options->certificate != NULL ||
	    options->privatekey != NULL) {
		if (options->certificate == NULL) {
			mbus_errorf("ssl certificate is invalid");
			goto bail;
		}
		if (options->privatekey == NULL) {
			mbus_errorf("ssl privatekey is invalid");
			goto bail;
		}
	}
	listener_tcp = malloc(sizeof(struct listener_tcp));
	if (listener_tcp == NULL) {
		mbus_errorf("can not allocate memory");
		goto bail;
	}
	memset(listener_tcp, 0, sizeof(struct listener_tcp));
	listener_tcp->name = strdup(options->name);
	if (listener_tcp->name == NULL) {
		mbus_errorf("can not allocate memory");
		goto bail;
	}
	listener_tcp->socket = mbus_socket_create(mbus_socket_domain_af_inet, mbus_socket_type_sock_stream, mbus_socket_protocol_any);
	if (listener_tcp->socket == NULL) {
		mbus_errorf("can not create socket: '%s:%s:%d'", "tcp", options->address, options->port);
		goto bail;
	}
	rc = mbus_socket_set_reuseaddr(listener_tcp->socket, 1);
	if (rc != 0) {
		mbus_errorf("can not reuse socket: '%s:%s:%d'", "tcp", options->address, options->port);
		goto bail;
	}
	mbus_socket_set_keepalive(listener_tcp->socket, 1);
#if 0
	mbus_socket_set_keepcnt(listener_tcp->socket, 5);
	mbus_socket_set_keepidle(listener_tcp->socket, 180);
	mbus_socket_set_keepintvl(listener_tcp->socket, 60);
#endif
	rc = mbus_socket_bind(listener_tcp->socket, options->address, options->port);
	if (rc != 0) {
		mbus_errorf("can not bind socket: '%s:%s:%d'", "tcp", options->address, options->port);
		goto bail;
	}
	rc = mbus_socket_listen(listener_tcp->socket, 1024);
	if (rc != 0) {
		mbus_errorf("can not listen socket: '%s:%s:%d'", "tcp", options->address, options->port);
		goto bail;
	}
#if defined(SSL_ENABLE) && (SSL_ENABLE == 1)
	if (options->certificate != NULL ||
	    options->privatekey != NULL) {
		const SSL_METHOD *method;
		method = SSLv23_server_method();
		if (method == NULL) {
			mbus_errorf("can not get server method");
			goto bail;
		}
		listener_tcp->ssl = SSL_CTX_new(method);
		if (listener_tcp->ssl == NULL) {
			mbus_errorf("can not create ssl context");
			goto bail;
		}
#if (OPENSSL_VERSION_NUMBER >= 0x10100000L)
		SSL_CTX_set_ecdh_auto(listener_tcp->ssl, 1);
#endif
		rc = SSL_CTX_use_certificate_file(listener_tcp->ssl, options->certificate, SSL_FILETYPE_PEM);
		if (rc <= 0) {
			mbus_errorf("can not use ssl certificate: %s", options->certificate);
			goto bail;
		}
		rc = SSL_CTX_use_PrivateKey_file(listener_tcp->ssl, options->privatekey, SSL_FILETYPE_PEM);
		if (rc <= 0) {
			mbus_errorf("can not use ssl privatekey: %s", options->privatekey);
			goto bail;
		}
	}
#endif
	listener_tcp->private.get_name          = listener_tcp_get_name;
	listener_tcp->private.get_type          = listener_tcp_get_type;
	listener_tcp->private.get_fd            = listener_tcp_get_fd;
	listener_tcp->private.connection_accept = listener_tcp_connection_accept;
	listener_tcp->private.destroy           = listener_tcp_destroy;
	return &listener_tcp->private.listener;
bail:	if (listener_tcp != NULL) {
		listener_tcp_destroy(&listener_tcp->private.listener);
	}
	return NULL;
}

struct connection_uds {
	struct connection_private private;
	struct mbus_socket *socket;
#if defined(SSL_ENABLE) && (SSL_ENABLE == 1)
	SSL *ssl;
	int want_read;
	int want_write;
#endif
};

struct listener_uds {
	struct listener_private private;
	char *name;
	struct mbus_socket *socket;
#if defined(SSL_ENABLE) && (SSL_ENABLE == 1)
	SSL_CTX *ssl;
#endif
};

static const char * listener_uds_get_name (struct listener *listener)
{
	struct listener_uds *listener_uds;
	if (listener == NULL) {
		mbus_errorf("listener is invalid");
		goto bail;
	}
	listener_uds = (struct listener_uds *) listener;
	return listener_uds->name;
bail:	return NULL;
}

static enum listener_type listener_uds_get_type (struct listener *listener)
{
	if (listener == NULL) {
		mbus_errorf("listener is invalid");
		goto bail;
	}
	return listener_type_uds;
bail:	return listener_type_unknown;
}

static int listener_uds_get_fd (struct listener *listener)
{
	struct listener_uds *listener_uds;
	if (listener == NULL) {
		mbus_errorf("listener is invalid");
		goto bail;
	}
	listener_uds = (struct listener_uds *) listener;
	return mbus_socket_get_fd(listener_uds->socket);
bail:	return -1;
}

static int connection_uds_close (struct connection *connection)
{
	struct connection_uds *connection_uds;
	if (connection == NULL) {
		mbus_errorf("connection is invalid");
		goto bail;
	}
	connection_uds = (struct connection_uds *) connection;
	mbus_socket_shutdown(connection_uds->socket, mbus_socket_shutdown_rdwr);
	mbus_socket_destroy(connection_uds->socket);
#if defined(SSL_ENABLE) && (SSL_ENABLE == 1)
	if (connection_uds->ssl != NULL) {
		SSL_free(connection_uds->ssl);
	}
#endif
	free(connection_uds);
	return 0;
bail:	return -1;
}

static int connection_uds_get_fd (struct connection *connection)
{
	struct connection_uds *connection_uds;
	if (connection == NULL) {
		mbus_errorf("connection is invalid");
		goto bail;
	}
	connection_uds = (struct connection_uds *) connection;
	return mbus_socket_get_fd(connection_uds->socket);
bail:	return -1;
}

static int connection_uds_get_want_read (struct connection *connection)
{
	struct connection_uds *connection_uds;
	if (connection == NULL) {
		mbus_errorf("connection is invalid");
		goto bail;
	}
	connection_uds = (struct connection_uds *) connection;
	(void) connection_uds;
	return 0;
bail:	return -1;
}

static int connection_uds_get_want_write (struct connection *connection)
{
	struct connection_uds *connection_uds;
	if (connection == NULL) {
		mbus_errorf("connection is invalid");
		goto bail;
	}
	connection_uds = (struct connection_uds *) connection;
	(void) connection_uds;
	return 0;
bail:	return -1;
}

static struct connection * connection_uds_accept (struct listener *listener)
{
	int rc;
	struct listener_uds *listener_uds;
	struct connection_uds *connection_uds;
	connection_uds = NULL;
	if (listener == NULL) {
		mbus_errorf("listener is invalid");
		goto bail;
	}
	listener_uds = (struct listener_uds *) listener;
	connection_uds = malloc(sizeof(struct connection_uds));
	if (connection_uds == NULL) {
		mbus_errorf("can not allocate memory");
		goto bail;
	}
	memset(connection_uds, 0, sizeof(struct connection_uds));
	connection_uds->socket = mbus_socket_accept(listener_uds->socket);
	if (connection_uds->socket == NULL) {
		mbus_errorf("can not accept new socket connection");
		goto bail;
	}
	rc = mbus_socket_set_blocking(connection_uds->socket, 0);
	if (rc != 0) {
		mbus_errorf("can not set socket to nonblocking");
		goto bail;
	}
#if defined(SSL_ENABLE) && (SSL_ENABLE == 1)
	if (listener_uds->ssl != NULL) {
		connection_uds->ssl = SSL_new(listener_uds->ssl);
		if (connection_uds->ssl == NULL) {
			mbus_errorf("can not create ssl");
			goto bail;
		}
		SSL_set_fd(connection_uds->ssl, mbus_socket_get_fd(connection_uds->socket));
		rc = SSL_accept(connection_uds->ssl);
		if (rc <= 0) {
			int error;
			error = SSL_get_error(connection_uds->ssl, rc);
			if (error == SSL_ERROR_WANT_READ) {
				connection_uds->want_read = 1;
			} else if (error == SSL_ERROR_WANT_WRITE) {
				connection_uds->want_write = 1;
			} else if (error == SSL_ERROR_SYSCALL) {
				connection_uds->want_read = 1;
			} else {
				char ebuf[256];
				mbus_errorf("can not accept ssl: %d", error);
				error = ERR_get_error();
				while (error) {
					mbus_errorf("  error: %d, %s", error, ERR_error_string(error, ebuf));
					error = ERR_get_error();
				}
				goto bail;
			}
		}
	}
#endif
	connection_uds->private.get_fd         = connection_uds_get_fd;
	connection_uds->private.get_want_read  = connection_uds_get_want_read;
	connection_uds->private.get_want_write = connection_uds_get_want_write;
	return &connection_uds->private.connection;
bail:	if (connection_uds != NULL) {
		connection_uds_close(&connection_uds->private.connection);
	}
	return NULL;
}

static void listener_uds_destroy (struct listener *listener)
{
	struct listener_uds *listener_uds;
	if (listener == NULL) {
		return;
	}
	listener_uds = (struct listener_uds *) listener;
	if (listener_uds->name != NULL) {
		free(listener_uds->name);
	}
	if (listener_uds->socket != NULL) {
		mbus_socket_destroy(listener_uds->socket);
	}
#if defined(SSL_ENABLE) && (SSL_ENABLE == 1)
	if (listener_uds->ssl != NULL) {
		SSL_CTX_free(listener_uds->ssl);
	}
#endif
}

struct listener * listener_uds_create (const struct listener_uds_options *options)
{
	int rc;
	struct listener_uds *listener_uds;
	listener_uds = NULL;
	if (options == NULL) {
		mbus_errorf("options is invalid");
		goto bail;
	}
	if (options->name == NULL) {
		mbus_errorf("name is invalid");
		goto bail;
	}
	if (options->address == NULL) {
		mbus_errorf("address is invalid");
		goto bail;
	}
	if (options->port <= 0) {
		mbus_errorf("port is invalid");
		goto bail;
	}
	if (options->certificate != NULL ||
	    options->privatekey != NULL) {
		if (options->certificate == NULL) {
			mbus_errorf("ssl certificate is invalid");
			goto bail;
		}
		if (options->privatekey == NULL) {
			mbus_errorf("ssl privatekey is invalid");
			goto bail;
		}
	}
	listener_uds = malloc(sizeof(struct listener_uds));
	if (listener_uds == NULL) {
		mbus_errorf("can not allocate memory");
		goto bail;
	}
	memset(listener_uds, 0, sizeof(struct listener_uds));
	listener_uds->name = strdup(options->name);
	if (listener_uds->name == NULL) {
		mbus_errorf("can not allocate memory");
		goto bail;
	}
	listener_uds->socket = mbus_socket_create(mbus_socket_domain_af_unix, mbus_socket_type_sock_stream, mbus_socket_protocol_any);
	if (listener_uds->socket == NULL) {
		mbus_errorf("can not create socket: '%s:%s:%d'", "uds", options->address, options->port);
		goto bail;
	}
	rc = mbus_socket_set_reuseaddr(listener_uds->socket, 1);
	if (rc != 0) {
		mbus_errorf("can not reuse socket: '%s:%s:%d'", "uds", options->address, options->port);
		goto bail;
	}
	mbus_socket_set_keepalive(listener_uds->socket, 1);
#if 0
	mbus_socket_set_keepcnt(listener_uds->socket, 5);
	mbus_socket_set_keepidle(listener_uds->socket, 180);
	mbus_socket_set_keepintvl(listener_uds->socket, 60);
#endif
	rc = mbus_socket_bind(listener_uds->socket, options->address, options->port);
	if (rc != 0) {
		mbus_errorf("can not bind socket: '%s:%s:%d'", "uds", options->address, options->port);
		goto bail;
	}
	rc = mbus_socket_listen(listener_uds->socket, 1024);
	if (rc != 0) {
		mbus_errorf("can not listen socket: '%s:%s:%d'", "uds", options->address, options->port);
		goto bail;
	}
#if defined(SSL_ENABLE) && (SSL_ENABLE == 1)
	if (options->certificate != NULL ||
	    options->privatekey != NULL) {
		const SSL_METHOD *method;
		method = SSLv23_server_method();
		if (method == NULL) {
			mbus_errorf("can not get server method");
			goto bail;
		}
		listener_uds->ssl = SSL_CTX_new(method);
		if (listener_uds->ssl == NULL) {
			mbus_errorf("can not create ssl context");
			goto bail;
		}
#if (OPENSSL_VERSION_NUMBER >= 0x10100000L)
		SSL_CTX_set_ecdh_auto(listener_uds->ssl, 1);
#endif
		rc = SSL_CTX_use_certificate_file(listener_uds->ssl, options->certificate, SSL_FILETYPE_PEM);
		if (rc <= 0) {
			mbus_errorf("can not use ssl certificate: %s", options->certificate);
			goto bail;
		}
		rc = SSL_CTX_use_PrivateKey_file(listener_uds->ssl, options->privatekey, SSL_FILETYPE_PEM);
		if (rc <= 0) {
			mbus_errorf("can not use ssl privatekey: %s", options->privatekey);
			goto bail;
		}
	}
#endif
	listener_uds->private.get_name          = listener_uds_get_name;
	listener_uds->private.get_type          = listener_uds_get_type;
	listener_uds->private.get_fd            = listener_uds_get_fd;
	listener_uds->private.connection_accept = connection_uds_accept;
	listener_uds->private.destroy           = listener_uds_destroy;
	return &listener_uds->private.listener;
bail:	if (listener_uds != NULL) {
		listener_uds_destroy(&listener_uds->private.listener);
	}
	return NULL;
}

struct connection_ws {
	struct connection_private private;
	struct lws *wsi;
};

struct listener_ws {
	struct listener_private private;
	char *name;
	struct lws_context *lws;
	struct listener_ws_callbacks callbacks;
#if defined(SSL_ENABLE) && (SSL_ENABLE == 1)
	SSL_CTX *ssl;
#endif
	uint8_t out_chunk[BUFFER_OUT_CHUNK_SIZE];
};

static void ws_log_callback (int level, const char *line)
{
	int len;
	if (line == NULL) {
		return;
	}
	len = strlen(line);
	if (len > 0 &&
	    line[len - 1] == '\n') {
		len -= 1;
	}
	if (level == LLL_ERR) {
		mbus_errorf("ws: %.*s", len, line);
	} else if (level == LLL_WARN) {
		mbus_warningf("ws: %.*s", len, line);
	} else if (level == LLL_NOTICE) {
		mbus_noticef("ws: %.*s", len, line);
	} else if (level >= LLL_DEBUG) {
		mbus_debugf("ws: %.*s", len, line);
	}
}

static int listener_ws_connection_close (struct connection *connection)
{
	struct connection_ws *connection_ws;
	if (connection == NULL) {
		mbus_errorf("connection is invalid");
		goto bail;
	}
	connection_ws = (struct connection_ws *) connection;
	lws_callback_on_writable(connection_ws->wsi);
	connection_ws->wsi = NULL;
	return 0;
bail:	return -1;
}

static int listener_ws_connection_get_fd (struct connection *connection)
{
	struct connection_ws *connection_ws;
	if (connection == NULL) {
		mbus_errorf("connection is invalid");
		goto bail;
	}
	connection_ws = (struct connection_ws *) connection;
	return lws_get_socket_fd(connection_ws->wsi);
bail:	return -1;
}

static int listener_ws_connection_get_want_read (struct connection *connection)
{
	struct connection_ws *connection_ws;
	if (connection == NULL) {
		mbus_errorf("connection is invalid");
		goto bail;
	}
	connection_ws = (struct connection_ws *) connection;
	(void) connection_ws;
	return 0;
bail:	return -1;
}

static int listener_ws_connection_get_want_write (struct connection *connection)
{
	struct connection_ws *connection_ws;
	if (connection == NULL) {
		mbus_errorf("connection is invalid");
		goto bail;
	}
	connection_ws = (struct connection_ws *) connection;
	(void) connection_ws;
	return 0;
bail:	return -1;
}

static int ws_protocol_mbus_callback (struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
	int rc;
	struct listener_ws *listener_ws;
	struct connection_ws *connection_ws;
	const struct lws_protocols *protocol;
	mbus_debugf("ws callback");
	listener_ws = NULL;
	connection_ws = (struct connection_ws *) user;
	protocol = lws_get_protocol(wsi);
	if (protocol != NULL) {
		listener_ws = protocol->user;
	}
	switch (reason) {
		case LWS_CALLBACK_LOCK_POLL:
			mbus_debugf("  lock poll");
			break;
		case LWS_CALLBACK_ADD_POLL_FD: {
			struct lws_pollargs *pa = (struct lws_pollargs *) in;
			mbus_debugf("  add poll fd: %d, events: 0x%08x", pa->fd, pa->events);
			rc = listener_ws->callbacks.poll_add(
					listener_ws->callbacks.context,
					&listener_ws->private.listener,
					pa->fd,
					pa->events);
			if (rc != 0) {
				mbus_errorf("can not add poll fd: %d, events: 0x%08x", pa->fd, pa->events);
				goto bail;
			}
			break;
		}
		case LWS_CALLBACK_CHANGE_MODE_POLL_FD: {
			struct lws_pollargs *pa = (struct lws_pollargs *) in;
			mbus_debugf("  mod poll fd: %d, events: 0x%08x", pa->fd, pa->events);
			rc = listener_ws->callbacks.poll_mod(
					listener_ws->callbacks.context,
					&listener_ws->private.listener,
					pa->fd,
					pa->events);
			if (rc != 0) {
				mbus_errorf("can not mod poll fd: %d, events: 0x%08x", pa->fd, pa->events);
				goto bail;
			}
			break;
		}
		case LWS_CALLBACK_DEL_POLL_FD: {
			struct lws_pollargs *pa = (struct lws_pollargs *) in;
			mbus_debugf("  del poll fd: %d, events: 0x%08x", pa->fd, pa->events);
			rc = listener_ws->callbacks.poll_del(
					listener_ws->callbacks.context,
					&listener_ws->private.listener,
					pa->fd);
			if (rc != 0) {
				mbus_errorf("can not del poll fd: %d", pa->fd);
				goto bail;
			}
			break;
		}
		case LWS_CALLBACK_UNLOCK_POLL:
			mbus_debugf("  unlock poll");
			break;
		case LWS_CALLBACK_GET_THREAD_ID:
			mbus_debugf("  get thread id");
			break;
		case LWS_CALLBACK_PROTOCOL_INIT:
			mbus_debugf("  protocol init");
			break;
		case LWS_CALLBACK_FILTER_NETWORK_CONNECTION:
			mbus_debugf("  filter network connection");
			break;
		case LWS_CALLBACK_SERVER_NEW_CLIENT_INSTANTIATED:
			mbus_debugf("  new client instantiated");
			break;
		case LWS_CALLBACK_FILTER_PROTOCOL_CONNECTION:
			mbus_debugf("  filter protocol connection");
			break;
		case LWS_CALLBACK_CONFIRM_EXTENSION_OKAY:
			mbus_debugf("  confirm extensions okay");
			break;
		case LWS_CALLBACK_ESTABLISHED:
			mbus_debugf("  established");
			memset(connection_ws, 0, sizeof(struct connection_ws));
			connection_ws->wsi = wsi;
			connection_ws->private.close          = listener_ws_connection_close;
			connection_ws->private.get_fd         = listener_ws_connection_get_fd;
			connection_ws->private.get_want_read  = listener_ws_connection_get_want_read;
			connection_ws->private.get_want_write = listener_ws_connection_get_want_write;
			rc = listener_ws->callbacks.connection_established(
					listener_ws->callbacks.context,
					&listener_ws->private.listener,
					&connection_ws->private.connection);
			if (rc != 0) {
				mbus_errorf("can not accept client");
				goto bail;
			}
			lws_callback_on_writable(connection_ws->wsi);
			break;
#if 0
		case LWS_CALLBACK_HTTP_DROP_PROTOCOL:
			mbus_debugf("  http drop protocol");
			break;
#endif
		case LWS_CALLBACK_PROTOCOL_DESTROY:
			mbus_debugf("  protocol destroy");
			break;
		case LWS_CALLBACK_WS_PEER_INITIATED_CLOSE:
			mbus_debugf("  ws peer initiated close");
			break;
		case LWS_CALLBACK_WSI_CREATE:
			mbus_debugf("  wsi create");
			break;
		case LWS_CALLBACK_WSI_DESTROY:
			mbus_debugf("  wsi destroy");
			break;
		case LWS_CALLBACK_CLOSED:
			mbus_debugf("  closed");
			if (connection_ws == NULL) {
				mbus_errorf("connection is invalid");
				goto bail;
			}
			if (connection_ws->wsi != NULL) {
				rc = listener_ws->callbacks.connection_closed(
						listener_ws->callbacks.context,
						&listener_ws->private.listener,
						&connection_ws->private.connection);
				if (rc != 0) {
					mbus_errorf("can not close client");
					goto bail;
				}
				lws_callback_on_writable(connection_ws->wsi);
				connection_ws->wsi = NULL;
			}
			break;
		case LWS_CALLBACK_RECEIVE:
			mbus_debugf("  server receive");
			if (connection_ws == NULL) {
				mbus_errorf("connection is invalid");
				goto bail;
			}
			if (connection_ws->wsi != NULL) {
				mbus_errorf("wsi is invalid");
				goto bail;
			}
			rc = listener_ws->callbacks.connection_receive(
					listener_ws->callbacks.context,
					&listener_ws->private.listener,
					&connection_ws->private.connection,
					in,
					len);
			if (rc != 0) {
				mbus_errorf("can not receive connection_ws");
				goto bail;
			}
			break;
		case LWS_CALLBACK_SERVER_WRITEABLE: {
			struct mbus_buffer *buffer;
			mbus_debugf("  server writable");
			if (connection_ws == NULL) {
				mbus_errorf("connection is invalid");
				goto bail;
			}
			if (connection_ws->wsi != NULL) {
				mbus_errorf("wsi is invalid");
				goto bail;
			}
			buffer = listener_ws->callbacks.connection_writable(
					listener_ws->callbacks.context,
					&listener_ws->private.listener,
					&connection_ws->private.connection);
			if (buffer == NULL) {
				mbus_errorf("can not get writable connection_ws");
				goto bail;
			}
			while (mbus_buffer_get_length(buffer) > 0 &&
			       lws_send_pipe_choked(connection_ws->wsi) == 0) {
				uint8_t *ptr;
				uint8_t *end;
				uint32_t expected;
				ptr = mbus_buffer_get_base(buffer);
				end = ptr + mbus_buffer_get_length(buffer);
				expected = end - ptr;
				if (end - ptr < (int32_t) expected) {
					break;
				}
				if (expected > BUFFER_OUT_CHUNK_SIZE - LWS_PRE) {
					expected = BUFFER_OUT_CHUNK_SIZE - LWS_PRE;
				}
				mbus_debugf("write");
				memset(listener_ws->out_chunk, 0, LWS_PRE + expected);
				memcpy(listener_ws->out_chunk + LWS_PRE, ptr, expected);
				mbus_debugf("payload: %d, %.*s", expected - 4, expected - 4, ptr + 4);
				rc = lws_write(connection_ws->wsi, listener_ws->out_chunk + LWS_PRE, expected, LWS_WRITE_BINARY);
				mbus_debugf("expected: %d, rc: %d", expected, rc);
				rc = mbus_buffer_shift(buffer, rc);
				if (rc != 0) {
					mbus_errorf("can not shift in");
					goto bail;
				}
				break;
			}
			if (mbus_buffer_get_length(buffer) > 0) {
				lws_callback_on_writable(connection_ws->wsi);
			}
			break;
		}
		case LWS_CALLBACK_OPENSSL_LOAD_EXTRA_SERVER_VERIFY_CERTS:
			mbus_debugf("  load extra server verify certs");
			break;
		case LWS_CALLBACK_FILTER_HTTP_CONNECTION:
			mbus_debugf("  filter http connection");
			break;
		case LWS_CALLBACK_CLOSED_HTTP:
			mbus_debugf("  closed http");
			break;
		default:
			mbus_errorf("unknown reason: %d", reason);
			break;
	}
	return 0;
bail:	return -1;
}

static struct lws_protocols ws_protocols[] = {
	{
		"mbus",
		ws_protocol_mbus_callback,
		sizeof(struct connection_ws),
		0,
		0,
		NULL,
#if (LWS_LIBRARY_VERSION_NUMBER >= 2004001)
		0,
#endif
	},
	{
		NULL,
		NULL,
		0,
		0,
		0,
		NULL,
#if (LWS_LIBRARY_VERSION_NUMBER >= 2004001)
		0,
#endif
	}
};

static const struct lws_extension ws_extensions[] = {
#if 0
	{
		"permessage-deflate",
		lws_extension_callback_pm_deflate,
		"permessage-deflate"
	},
#endif
	{
		"deflate-frame",
		lws_extension_callback_pm_deflate,
		"deflate_frame"
	},
	{
		NULL,
		NULL,
		NULL
	}
};

#if defined(SSL_ENABLE) && (SSL_ENABLE == 1)

static struct lws_protocols wss_protocols[] = {
	{
		"mbus",
		ws_protocol_mbus_callback,
		sizeof(struct connection_ws),
		0,
		0,
		NULL,
#if (LWS_LIBRARY_VERSION_NUMBER >= 2004001)
		0,
#endif
	},
	{
		NULL,
		NULL,
		0,
		0,
		0,
		NULL,
#if (LWS_LIBRARY_VERSION_NUMBER >= 2004001)
		0,
#endif
	}
};

static const struct lws_extension wss_extensions[] = {
#if 0
	{
		"permessage-deflate",
		lws_extension_callback_pm_deflate,
		"permessage-deflate"
	},
#endif
	{
		"deflate-frame",
		lws_extension_callback_pm_deflate,
		"deflate_frame"
	},
	{
		NULL,
		NULL,
		NULL
	}
};

#endif

static const char * listener_ws_get_name (struct listener *listener)
{
	struct listener_ws *listener_ws;
	if (listener == NULL) {
		mbus_errorf("listener is invalid");
		goto bail;
	}
	listener_ws = (struct listener_ws *) listener;
	return listener_ws->name;
bail:	return NULL;
}

static enum listener_type listener_ws_get_type (struct listener *listener)
{
	if (listener == NULL) {
		mbus_errorf("listener is invalid");
		goto bail;
	}
	return listener_type_ws;
bail:	return listener_type_unknown;
}

static int listener_ws_get_fd (struct listener *listener)
{
	struct listener_ws *listener_ws;
	if (listener == NULL) {
		mbus_errorf("listener is invalid");
		goto bail;
	}
	listener_ws = (struct listener_ws *) listener;
	(void) listener_ws;
	return -1;
bail:	return -1;
}

static void listener_ws_destroy (struct listener *listener)
{
	struct listener_ws *listener_ws;
	if (listener == NULL) {
		return;
	}
	listener_ws = (struct listener_ws *) listener;
	if (listener_ws->name != NULL) {
		free(listener_ws->name);
	}
	if (listener_ws->lws != NULL) {
		lws_context_destroy(listener_ws->lws);
	}
#if defined(SSL_ENABLE) && (SSL_ENABLE == 1)
	if (listener_ws->ssl != NULL) {
		SSL_CTX_free(listener_ws->ssl);
	}
#endif
}

struct listener * listener_ws_create (const struct listener_ws_options *options)
{
	int rc;
	struct listener_ws *listener_ws;
	struct lws_context_creation_info info;
	listener_ws = NULL;
	if (options == NULL) {
		mbus_errorf("options is invalid");
		goto bail;
	}
	if (options->name == NULL) {
		mbus_errorf("name is invalid");
		goto bail;
	}
	if (options->address == NULL) {
		mbus_errorf("address is invalid");
		goto bail;
	}
	if (options->port <= 0) {
		mbus_errorf("port is invalid");
		goto bail;
	}
	if (options->callbacks.connection_established == NULL) {
		mbus_errorf("callbacks.connection_established is invalid");
		goto bail;
	}
	if (options->callbacks.connection_receive == NULL) {
		mbus_errorf("callbacks.connection_receive is invalid");
		goto bail;
	}
	if (options->callbacks.connection_writable == NULL) {
		mbus_errorf("callbacks.connection_writable is invalid");
		goto bail;
	}
	if (options->callbacks.connection_closed == NULL) {
		mbus_errorf("callbacks.connection_closed is invalid");
		goto bail;
	}
	if (options->callbacks.poll_add == NULL) {
		mbus_errorf("callbacks.poll_add is invalid");
		goto bail;
	}
	if (options->callbacks.poll_mod == NULL) {
		mbus_errorf("callbacks.poll_mod is invalid");
		goto bail;
	}
	if (options->callbacks.poll_del == NULL) {
		mbus_errorf("callbacks.poll_del is invalid");
		goto bail;
	}
	if (options->certificate != NULL ||
	    options->privatekey != NULL) {
		if (options->certificate == NULL) {
			mbus_errorf("ssl certificate is invalid");
			goto bail;
		}
		if (options->privatekey == NULL) {
			mbus_errorf("ssl privatekey is invalid");
			goto bail;
		}
	}
	listener_ws = malloc(sizeof(struct listener_ws));
	if (listener_ws == NULL) {
		mbus_errorf("can not allocate memory");
		goto bail;
	}
	memset(listener_ws, 0, sizeof(struct listener_ws));
	listener_ws->name = strdup(options->name);
	if (listener_ws->name == NULL) {
		mbus_errorf("can not allocate memory");
		goto bail;
	}
	memcpy(&listener_ws->callbacks, &options->callbacks, sizeof(struct listener_ws_callbacks));
	ws_protocols[0].user = listener_ws;
	wss_protocols[0].user = listener_ws;
	lws_set_log_level((1 << LLL_COUNT) - 1, ws_log_callback);
	memset(&info, 0, sizeof(info));
	info.iface = NULL;
	info.port = options->port;
	info.gid = -1;
	info.uid = -1;
	if (options->certificate == NULL &&
	    options->privatekey == NULL) {
		info.protocols = ws_protocols;
		info.extensions = ws_extensions;
	} else {
		memset(&info, 0, sizeof(info));
		info.protocols = wss_protocols;
		info.extensions = wss_extensions;
		info.ssl_ca_filepath = NULL; //"ca.crt";
		info.ssl_cert_filepath = options->certificate;
		info.ssl_private_key_filepath = options->privatekey;
//		info.options |= LWS_SERVER_OPTION_REQUIRE_VALID_OPENSSL_CLIENT_CERT;
		info.options |= LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
	}
	listener_ws->lws = lws_create_context(&info);
	if (listener_ws->lws == NULL) {
		mbus_errorf("can not create ws context for: '%s:%s:%d'", "wss", options->address, options->port);
		goto bail;
	}
#if defined(SSL_ENABLE) && (SSL_ENABLE == 1)
	if (options->certificate != NULL ||
	    options->privatekey != NULL) {
		const SSL_METHOD *method;
		method = SSLv23_server_method();
		if (method == NULL) {
			mbus_errorf("can not get server method");
			goto bail;
		}
		listener_ws->ssl = SSL_CTX_new(method);
		if (listener_ws->ssl == NULL) {
			mbus_errorf("can not create ssl context");
			goto bail;
		}
#if (OPENSSL_VERSION_NUMBER >= 0x10100000L)
		SSL_CTX_set_ecdh_auto(listener_ws->ssl, 1);
#endif
		rc = SSL_CTX_use_certificate_file(listener_ws->ssl, options->certificate, SSL_FILETYPE_PEM);
		if (rc <= 0) {
			mbus_errorf("can not use ssl certificate: %s", options->certificate);
			goto bail;
		}
		rc = SSL_CTX_use_PrivateKey_file(listener_ws->ssl, options->privatekey, SSL_FILETYPE_PEM);
		if (rc <= 0) {
			mbus_errorf("can not use ssl privatekey: %s", options->privatekey);
			goto bail;
		}
	}
#endif
	listener_ws->private.get_name          = listener_ws_get_name;
	listener_ws->private.get_type          = listener_ws_get_type;
	listener_ws->private.get_fd            = listener_ws_get_fd;
	listener_ws->private.connection_accept = NULL;
	listener_ws->private.destroy           = listener_ws_destroy;
	return &listener_ws->private.listener;
bail:	if (listener_ws != NULL) {
		listener_ws_destroy(&listener_ws->private.listener);
	}
	return NULL;
}

const char * listener_get_name (struct listener *listener)
{
	struct listener_private *private;
	if (listener == NULL) {
		mbus_errorf("listener is invalid");
		goto bail;
	}
	private = (struct listener_private *) listener;
	if (private->get_name == NULL) {
		mbus_errorf("listener->get_name is invalid");
		goto bail;
	}
	return private->get_name(listener);
bail:	return NULL;
}

enum listener_type listener_get_type (struct listener *listener)
{
	struct listener_private *private;
	if (listener == NULL) {
		mbus_errorf("listener is invalid");
		goto bail;
	}
	private = (struct listener_private *) listener;
	if (private->get_type == NULL) {
		mbus_errorf("listener->get_type is invalid");
		goto bail;
	}
	return private->get_type(listener);
bail:	return listener_type_unknown;
}

int listener_get_fd (struct listener *listener)
{
	struct listener_private *private;
	if (listener == NULL) {
		mbus_errorf("listener is invalid");
		goto bail;
	}
	private = (struct listener_private *) listener;
	if (private->get_fd == NULL) {
		mbus_errorf("listener->get_fd is invalid");
		goto bail;
	}
	return private->get_fd(listener);
bail:	return -1;
}

struct connection * listener_connection_accept (struct listener *listener)
{
	struct listener_private *private;
	if (listener == NULL) {
		mbus_errorf("listener is invalid");
		goto bail;
	}
	private = (struct listener_private *) listener;
	if (private->connection_accept == NULL) {
		mbus_errorf("listener->connection_accept is invalid");
		goto bail;
	}
	return private->connection_accept(listener);
bail:	return NULL;
}

int connection_close (struct connection *connection)
{
	struct connection_private *private;
	if (connection == NULL) {
		mbus_errorf("connection is invalid");
		goto bail;
	}
	private = (struct connection_private *) connection;
	if (private->close == NULL) {
		mbus_errorf("connection->close is invalid");
		goto bail;
	}
	return private->close(connection);
bail:	return -1;
}

int connection_get_fd (struct connection *connection)
{
	struct connection_private *private;
	if (connection == NULL) {
		mbus_errorf("connection is invalid");
		goto bail;
	}
	private = (struct connection_private *) connection;
	if (private->get_fd == NULL) {
		mbus_errorf("connection->get_fd is invalid");
		goto bail;
	}
	return private->get_fd(connection);
bail:	return -1;
}

int connection_get_want_read (struct connection *connection)
{
	struct connection_private *private;
	if (connection == NULL) {
		mbus_errorf("connection is invalid");
		goto bail;
	}
	private = (struct connection_private *) connection;
	if (private->get_want_read == NULL) {
		mbus_errorf("connection->get_want_read is invalid");
		goto bail;
	}
	return private->get_want_read(connection);
bail:	return -1;
}
int connection_get_want_write (struct connection *connection)
{
	struct connection_private *private;
	if (connection == NULL) {
		mbus_errorf("connection is invalid");
		goto bail;
	}
	private = (struct connection_private *) connection;
	if (private->get_want_write == NULL) {
		mbus_errorf("connection->get_want_write is invalid");
		goto bail;
	}
	return private->get_want_write(connection);
bail:	return -1;
}
