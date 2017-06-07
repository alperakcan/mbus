
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
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>

#define MBUS_DEBUG_NAME	"mbus-socket"

#include "mbus/debug.h"
#include "socket.h"

struct websocket_per_session_data {
	int fd;
};

struct mbus_socket {
	int domain;
	int type;
	int protocol;
	int fd;
	struct {
		unsigned int size;
		char *buffer;
	} buffer;
};

static int mbus_poll_event_to_posix (enum mbus_poll_event event)
{
	int e;
	e = 0;
	if (event & mbus_poll_event_in) {
		e |= POLLIN;
	}
	if (event & mbus_poll_event_out) {
		e |= POLLOUT;
	}
	return e;
}

enum mbus_poll_event mbus_poll_event_from_posix (int event)
{
	enum mbus_poll_event e;
	e = 0;
	if (event & POLLIN) {
		e |= mbus_poll_event_in;
		event &= ~POLLIN;
	}
	if (event & POLLOUT) {
		e |= mbus_poll_event_out;
		event &= ~POLLOUT;
	}
	if (event & POLLERR) {
		e |= mbus_poll_event_err;
		event &= ~POLLERR;
	}
	if (event & POLLHUP) {
		e |= mbus_poll_event_err;
		e |= mbus_poll_event_hup;
		event &= ~POLLHUP;
	}
	if (event & POLLNVAL) {
		e |= mbus_poll_event_err;
		e |= mbus_poll_event_nval;
		event &= ~POLLNVAL;
	}
	if (event != 0) {
		e |= mbus_poll_event_err;
	}
	return e;
}

static int mbus_socket_domain_to_posix (enum mbus_socket_domain domain)
{
	switch (domain) {
		case mbus_socket_domain_af_inet:		return AF_INET;
		case mbus_socket_domain_af_unix:		return AF_UNIX;
	}
	return -1;
}

static int mbus_socket_type_to_posix (enum mbus_socket_type type)
{
	switch (type) {
		case mbus_socket_type_sock_stream:		return SOCK_STREAM;
	}
	return -1;
}

static int mbus_socket_protocol_to_posix (enum mbus_socket_protocol protocol)
{
	switch (protocol) {
		case mbus_socket_protocol_any:		return 0;
	}
	return -1;
}

struct mbus_socket * mbus_socket_create (enum mbus_socket_domain domain, enum mbus_socket_type type, enum mbus_socket_protocol protocol)
{
	struct mbus_socket *s;
	s = malloc(sizeof(struct mbus_socket));
	if (s == NULL) {
		mbus_errorf("can not allocate memory");
		goto bail;
	}
	memset(s, 0, sizeof(struct mbus_socket));
	s->domain = mbus_socket_domain_to_posix(domain);
	if (s->domain < 0) {
		mbus_errorf("invalid domain");
		goto bail;
	}
	s->type = mbus_socket_type_to_posix(type);
	if (s->type < 0) {
		mbus_errorf("invalid type");
		goto bail;
	}
	s->protocol = mbus_socket_protocol_to_posix(protocol);
	if (s->protocol < 0) {
		mbus_errorf("invalid protocol");
		goto bail;
	}
	s->fd = socket(s->domain, s->type, s->protocol);
	if (s->fd < 0) {
		mbus_errorf("can not open socket");
		goto bail;
	}
	return s;
bail:	mbus_socket_destroy(s);
	return NULL;
}

void mbus_socket_destroy (struct mbus_socket *socket)
{
	if (socket == NULL) {
		return;
	}
	if (socket->fd >= 0) {
		close(socket->fd);
	}
	if (socket->buffer.buffer != NULL) {
		free(socket->buffer.buffer);
	}
	free(socket);
}

void mbus_socket_close (struct mbus_socket *socket)
{
	if (socket == NULL) {
		return;
	}
	if (socket->fd >= 0) {
		close(socket->fd);
		socket->fd = -1;
	}
}

int mbus_socket_get_fd (struct mbus_socket *socket)
{
	if (socket == NULL) {
		return -1;
	}
	return socket->fd;
}

int mbus_socket_set_reuseaddr (struct mbus_socket *socket, int on)
{
	int rc;
	int opt;
	if (socket == NULL) {
		mbus_errorf("socket is null");
		return -1;
	}
	opt = !!on;
	rc = setsockopt(socket->fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	if (rc < 0) {
		mbus_errorf("setsockopt reuseaddr failed");
		return -1;
	}
	return 0;
}

int mbus_socket_get_reuseaddr (struct mbus_socket *socket)
{
	int rc;
	int opt;
	socklen_t optlen;
	if (socket == NULL) {
		mbus_errorf("socket is null");
		return -1;
	}
	optlen = sizeof(opt);
	rc = getsockopt(socket->fd, SOL_SOCKET, SO_REUSEADDR, &opt, &optlen);
	if (rc < 0) {
		mbus_errorf("setsockopt reuseaddr failed");
		return -1;
	}
	return !!opt;
}

int mbus_socket_set_blocking (struct mbus_socket *socket, int on)
{
	int rc;
	int flags;
	if (socket == NULL) {
		mbus_errorf("socket is null");
		return -1;
	}
	flags = fcntl(socket->fd, F_GETFL, 0);
	if (flags < 0) {
		mbus_errorf("can not get flags");
		return -1;
	}
	flags = on ? (flags & ~O_NONBLOCK) : (flags | O_NONBLOCK);
	rc = fcntl(socket->fd, F_SETFL, flags);
	if (rc != 0) {
		mbus_errorf("can not set flags");
		return -1;
	}
	return 0;
}

int mbus_socket_get_blocking (struct mbus_socket *socket)
{
	int flags;
	if (socket == NULL) {
		mbus_errorf("socket is null");
		return -1;
	}
	flags = fcntl(socket->fd, F_GETFL, 0);
	if (flags < 0) {
		mbus_errorf("can not get flags");
		return -1;
	}
	return flags & O_NONBLOCK;
}

int mbus_socket_set_keepalive (struct mbus_socket *socket, int on)
{
	int rc;
	int opt;
	if (socket == NULL) {
		mbus_errorf("socket is null");
		return -1;
	}
	opt = !!on;
	rc = setsockopt(socket->fd, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt));
	if (rc < 0) {
		mbus_errorf("setsockopt keepalive failed");
		return -1;
	}
	return 0;
}

int mbus_socket_get_keepalive (struct mbus_socket *socket)
{
	int rc;
	int opt;
	socklen_t optlen;
	if (socket == NULL) {
		mbus_errorf("socket is null");
		return -1;
	}
	optlen = sizeof(opt);
	rc = getsockopt(socket->fd, SOL_SOCKET, SO_KEEPALIVE, &opt, &optlen);
	if (rc < 0) {
		mbus_errorf("setsockopt keepalive failed");
		return -1;
	}
	return opt;
}

int mbus_socket_set_keepcnt (struct mbus_socket *socket, int value)
{
	int rc;
	int opt;
	if (socket == NULL) {
		mbus_errorf("socket is null");
		return -1;
	}
	opt = value;
	rc = setsockopt(socket->fd, SOL_SOCKET, TCP_KEEPCNT, &opt, sizeof(opt));
	if (rc < 0) {
		mbus_errorf("setsockopt keepcnt failed");
		return -1;
	}
	return 0;
}

int mbus_socket_get_keepcnt (struct mbus_socket *socket)
{
	int rc;
	int opt;
	socklen_t optlen;
	if (socket == NULL) {
		mbus_errorf("socket is null");
		return -1;
	}
	optlen = sizeof(opt);
	rc = getsockopt(socket->fd, SOL_SOCKET, TCP_KEEPCNT, &opt, &optlen);
	if (rc < 0) {
		mbus_errorf("setsockopt keepcnt failed");
		return -1;
	}
	return opt;
}

int mbus_socket_set_keepidle (struct mbus_socket *socket, int value)
{
#if defined(TCP_KEEPIDLE)
	int rc;
	int opt;
	if (socket == NULL) {
		mbus_errorf("socket is null");
		return -1;
	}
	opt = value;
	rc = setsockopt(socket->fd, SOL_SOCKET, TCP_KEEPIDLE, &opt, sizeof(opt));
	if (rc < 0) {
		mbus_errorf("setsockopt keepidle failed");
		return -1;
	}
	return 0;
#else
	if (socket == NULL) {
		mbus_errorf("socket is null");
		return -1;
	}
	(void) value;
	mbus_errorf("setsockopt keepidle is not supported");
	return -1;
#endif
}

int mbus_socket_get_keepidle (struct mbus_socket *socket)
{
#if defined(TCP_KEEPIDLE)
	int rc;
	int opt;
	socklen_t optlen;
	if (socket == NULL) {
		mbus_errorf("socket is null");
		return -1;
	}
	optlen = sizeof(opt);
	rc = getsockopt(socket->fd, SOL_SOCKET, TCP_KEEPIDLE, &opt, &optlen);
	if (rc < 0) {
		mbus_errorf("setsockopt keepidle failed");
		return -1;
	}
	return opt;
#else
	if (socket == NULL) {
		mbus_errorf("socket is null");
		return -1;
	}
	mbus_errorf("setsockopt keepidle is not supported");
	return -1;
#endif
}

int mbus_socket_set_keepintvl (struct mbus_socket *socket, int value)
{
	int rc;
	int opt;
	if (socket == NULL) {
		mbus_errorf("socket is null");
		return -1;
	}
	opt = value;
	rc = setsockopt(socket->fd, SOL_SOCKET, TCP_KEEPINTVL, &opt, sizeof(opt));
	if (rc < 0) {
		mbus_errorf("setsockopt keepintvl failed");
		return -1;
	}
	return 0;
}

int mbus_socket_get_keepintvl (struct mbus_socket *socket)
{
	int rc;
	int opt;
	socklen_t optlen;
	if (socket == NULL) {
		mbus_errorf("socket is null");
		return -1;
	}
	optlen = sizeof(opt);
	rc = getsockopt(socket->fd, SOL_SOCKET, TCP_KEEPINTVL, &opt, &optlen);
	if (rc < 0) {
		mbus_errorf("setsockopt keepintvl failed");
		return -1;
	}
	return opt;
}

int mbus_socket_connect (struct mbus_socket *socket, const char *address, unsigned short port)
{
	int rc;
	struct sockaddr_in sockaddr_in;
	struct sockaddr_un sockaddr_un;
	if (address == NULL) {
		mbus_errorf("address is null");
		return -1;
	}
	if (socket->domain == AF_INET) {
		sockaddr_in.sin_family = socket->domain;
		rc = inet_pton(socket->domain, address, &sockaddr_in.sin_addr);
		if (rc <= 0) {
			mbus_errorf("inet_pton failed for: '%s'", address);
			goto bail;
		}
		sockaddr_in.sin_port = htons(port);
		rc = connect(socket->fd, (struct sockaddr *) &sockaddr_in , sizeof(sockaddr_in));
	} else if (socket->domain == AF_UNIX) {
		sockaddr_un.sun_family = socket->domain;
		snprintf(sockaddr_un.sun_path, sizeof(sockaddr_un.sun_path) - 1, "%s:%d", address, port);
		rc = connect(socket->fd, (struct sockaddr *) &sockaddr_un , sizeof(sockaddr_un));
	} else {
		mbus_errorf("unknown socket domain");
		goto bail;
	}
	if (rc < 0) {
		mbus_errorf("connect failed");
		goto bail;
	}
	return 0;
bail:	return -1;
}

int mbus_socket_bind (struct mbus_socket *socket, const char *address, unsigned short port)
{
	int rc;
	struct sockaddr_in sockaddr_in;
	struct sockaddr_un sockaddr_un;
	if (socket->domain == AF_INET) {
		sockaddr_in.sin_family = socket->domain;
		if (address == NULL) {
			sockaddr_in.sin_addr.s_addr = INADDR_ANY;
		} else {
			rc = inet_pton(socket->domain, address, &sockaddr_in.sin_addr);
			if (rc <= 0) {
				mbus_errorf("inet_pton failed for: '%s'", address);
				goto bail;
			}
		}
		sockaddr_in.sin_port = htons(port);
		rc = bind(socket->fd, (struct sockaddr *) &sockaddr_in , sizeof(sockaddr_in));
	} else if (socket->domain == AF_UNIX) {
		sockaddr_un.sun_family = socket->domain;
		snprintf(sockaddr_un.sun_path, sizeof(sockaddr_un.sun_path) - 1, "%s:%d", address, port);
		if (mbus_socket_get_reuseaddr(socket) == 1) {
			unlink(sockaddr_un.sun_path);
		}
		rc = bind(socket->fd, (struct sockaddr *) &sockaddr_un , sizeof(sockaddr_un));
	} else {
		mbus_errorf("unknown socket domain");
		goto bail;
	}
	if (rc < 0) {
		mbus_errorf("bind failed (%s)", strerror(errno));
		goto bail;
	}
	return 0;
bail:	return -1;
}

int mbus_socket_listen (struct mbus_socket *socket, int backlog)
{
	int rc;
	if (socket == NULL) {
		mbus_errorf("socket is null");
		return -1;
	}
	rc = listen(socket->fd, backlog);
	if (rc < 0) {
		mbus_errorf("listen failed");
		return -1;
	}
	return 0;
}

struct mbus_socket * mbus_socket_accept (struct mbus_socket *socket)
{
	struct mbus_socket *s;
	struct sockaddr_in sockaddr_in;
	socklen_t socklen;
	s = malloc(sizeof(struct mbus_socket));
	if (s == NULL) {
		mbus_errorf("can not allocate memory");
		goto bail;
	}
	memset(s, 0, sizeof(struct mbus_socket));
	s->domain = socket->domain;
	s->type = socket->type;
	socklen = sizeof(struct sockaddr_in);
	s->fd = accept(socket->fd, (struct sockaddr *) &sockaddr_in, &socklen);
	if (s->fd < 0) {
		mbus_errorf("can not accept socket");
		goto bail;
	}
	return s;
bail:	mbus_socket_destroy(s);
	return NULL;
}

int mbus_socket_read (struct mbus_socket *socket, void *vptr, int n)
{
	int nleft;
	int nread;
	char *ptr;
	ptr = vptr;
	nleft = n;
	while (nleft > 0){
		if ((nread = read(socket->fd, ptr, nleft)) < 0 ){
			if (errno == EINTR) {
				nread = 0;
			} else if (errno == EAGAIN) {
				break;
			} else if (errno == EWOULDBLOCK) {
				break;
			} else {
				return -1;
			}
		} else if (nread == 0) {
			/* EOF */
			break;
		}
		nleft -= nread;
		ptr   += nread;
	}
	return n - nleft;
}

int mbus_socket_write (struct mbus_socket *socket, const void *vptr, int n)
{
	int nleft;
	int nwritten;
	const char *ptr;
	ptr = vptr;
	nleft = n;
	while (nleft > 0) {
		if ((nwritten = write(socket->fd, ptr, nleft)) <= 0) {
			if (errno == EINTR) {
				nwritten = 0;
			} else if (errno == EAGAIN) {
				break;
			} else if (errno == EWOULDBLOCK) {
				break;
			} else {
				return -1;
			}
		}
		nleft -= nwritten;
		ptr   += nwritten;
	}
	return n - nleft;
}

char * mbus_socket_read_string (struct mbus_socket *socket)
{
	int rc;
	char *string;
	uint32_t length;
	if (socket == NULL) {
		mbus_errorf("socket is null");
		return NULL;
	}
	rc = mbus_socket_read(socket, &length, sizeof(uint32_t));
	if (rc != sizeof(uint32_t)) {
		return NULL;
	}
	length = ntohl(length);
	string = malloc(length + 1);
	if (string == NULL) {
		mbus_errorf("can not allocate memory");
		return NULL;
	}
	rc = mbus_socket_read(socket, string, length);
	if (rc != (int) length) {
		free(string);
		return NULL;
	}
	string[length] = '\0';
	return string;
}

int mbus_socket_write_string (struct mbus_socket *socket, const char *string)
{
	int rc;
	uint32_t slength;
	uint32_t nlength;
	uint32_t wlength;
	if (socket == NULL) {
		mbus_errorf("socket is null");
		return -1;
	}
	if (string == NULL) {
		mbus_errorf("string is null");
		return -1;
	}
	slength = strlen(string);
	wlength = sizeof(uint32_t) + slength;
	if (wlength > socket->buffer.size) {
		char *tmp;
		while (wlength > socket->buffer.size) {
			socket->buffer.size += 4096;
		}
		tmp = realloc(socket->buffer.buffer, socket->buffer.size);
		if (tmp == NULL) {
			tmp = malloc(socket->buffer.size);
			if (tmp == NULL) {
				return -1;
			}
			free(socket->buffer.buffer);
		}
		socket->buffer.buffer = tmp;
	}
	nlength = htonl(slength);
	memcpy(socket->buffer.buffer, &nlength, sizeof(uint32_t));
	memcpy(socket->buffer.buffer + sizeof(uint32_t), string, slength);
	rc = mbus_socket_write(socket, socket->buffer.buffer, wlength);
	if (rc != (int) wlength) {
		return -1;
	}
	return 0;
}

int mbus_socket_poll (struct mbus_poll *polls, int npolls, int timeout)
{
	int p;
	int rc;
	struct pollfd *pollfd;
	struct pollfd _pollfds[32];
	if (polls == NULL) {
		mbus_errorf("polls is null");
		return -1;
	}
	if (npolls <= 0) {
		mbus_errorf("invalid polls count");
		return -1;
	}
	if (npolls < (int) (sizeof(_pollfds) / sizeof(_pollfds[0]))) {
		pollfd = _pollfds;
	} else {
		pollfd = malloc(sizeof(struct pollfd) * npolls);
		if (pollfd == NULL) {
			mbus_errorf("can not allocate memory");
			return -1;
		}
	}
	for (p = 0; p < npolls; p++) {
		pollfd[p].fd = polls[p].socket->fd;
		pollfd[p].events = mbus_poll_event_to_posix(polls[p].events);
		pollfd[p].revents = 0;
		polls[p].revents = 0;
	}
	rc = poll(pollfd, npolls, timeout);
	if (rc < 0) {
		if (errno == EINTR) {
			goto intr;
		}
		mbus_errorf("poll error (rc: %d, errno: %d - %s)", rc, errno, strerror(errno));
		goto bail;
	}
	if (rc == 0) {
		goto out;
	}
	for (p = 0; p < npolls; p++) {
		if (pollfd[p].revents == 0) {
			continue;
		}
		polls[p].revents = mbus_poll_event_from_posix(pollfd[p].revents);
	}
out:	if (pollfd != _pollfds) {
		free(pollfd);
	}
	return rc;
intr:	if (pollfd != _pollfds) {
		free(pollfd);
	}
	return 0;
bail:	if (pollfd != _pollfds) {
		free(pollfd);
	}
	return -1;
}
