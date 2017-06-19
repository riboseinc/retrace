/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "common.h"
#include "str.h"
#include "malloc.h"
#include "sock.h"

#define RETRACE_MAX_IP_ADDR_LEN 15

int RETRACE_IMPLEMENTATION(socket)(int domain, int type, int protocol)
{
	int sock;
	rtr_socket_t real_socket;

	real_socket = RETRACE_GET_REAL(socket);

	sock = real_socket(domain, type, protocol);
	trace_printf(1, "socket(%d, %d, %d) [return: %d]\n", domain, type, protocol, sock);

	return sock;
}

RETRACE_REPLACE(socket)

#ifdef __linux__
int RETRACE_IMPLEMENTATION(connect)(int fd, __CONST_SOCKADDR_ARG _address, socklen_t len)
#else
int RETRACE_IMPLEMENTATION(connect)(int fd, const struct sockaddr *address, socklen_t len)
#endif
{
	rtr_connect_t real_connect;
	rtr_strcmp_t  real_strcmp;
	rtr_free_t    real_free;
#ifdef __linux__
	const struct sockaddr *address = _address.__sockaddr__;
#endif

	int ret;

	real_connect = RETRACE_GET_REAL(connect);
	real_strcmp = RETRACE_GET_REAL(strcmp);
	real_free = RETRACE_GET_REAL(free);

	if (!get_tracing_enabled())
		return real_connect(fd, address, len);

	if (address->sa_family == AF_INET) {
		struct sockaddr_in *dst_addr = (struct sockaddr_in *) address;

		struct sockaddr_in *remote_addr;
		const char *remote_ipaddr;
		int remote_port;

		struct sockaddr_in redirect_addr;
		int enabled_redirect = 0;

		FILE *config = NULL;

		/* get IP address and port number to connect */
		const char *dst_ipaddr = inet_ntoa(dst_addr->sin_addr);
		int dst_port = ntohs(dst_addr->sin_port);

		/* get configuration for redirection */
		while (1) {
			char *match_ipaddr = NULL;
			int match_port;

			char *redirect_ipaddr = NULL;
			int redirect_port;

			/* get redirect info from configuration file */
			if (rtr_get_config_multiple(&config, "connect",
					ARGUMENT_TYPE_STRING,
					ARGUMENT_TYPE_INT,
					ARGUMENT_TYPE_STRING,
					ARGUMENT_TYPE_INT,
					ARGUMENT_TYPE_END,
					&match_ipaddr,
					&match_port,
					&redirect_ipaddr,
					&redirect_port) == 0)
				break;

			/* check if IP address and port number is matched */
			if (real_strcmp(match_ipaddr, dst_ipaddr) == 0 && match_port == dst_port) {
				/* set redirect address info */
				memset(&redirect_addr, 0, sizeof(redirect_addr));

				redirect_addr.sin_family = AF_INET;
				redirect_addr.sin_addr.s_addr = inet_addr(redirect_ipaddr);
				redirect_addr.sin_port = htons(redirect_port);

				/* set redirect flag */
				enabled_redirect = 1;
			}

			/* free buffers */
			if (redirect_ipaddr)
				real_free(redirect_ipaddr);

			if (match_ipaddr)
				real_free(match_ipaddr);

			/* check redirect flag */
			if (enabled_redirect)
				break;
		}

		/* close config */
		if (config)
			rtr_config_close(config);

		/* set remote address info */
		remote_addr = enabled_redirect ? &redirect_addr : (struct sockaddr_in *) address;
		remote_ipaddr = inet_ntoa(remote_addr->sin_addr);
		remote_port = ntohs(remote_addr->sin_port);

		/* connect to remote */
		ret = real_connect(fd, (struct sockaddr *) remote_addr, sizeof(struct sockaddr_in));
		if (ret == 0)
			file_descriptor_update(fd, FILE_DESCRIPTOR_TYPE_IPV4_CONNECT, remote_ipaddr, remote_port);

		trace_printf(1, "connect(%d, %s:%d, ); [%d](AF_INET)\n", fd, remote_ipaddr, remote_port, ret);

		return ret;
	} else if (address->sa_family == AF_UNIX) {
		struct sockaddr_un *un_addr = (struct sockaddr_un *) address;
		const char *sun_path = un_addr->sun_path;

		/* connect to local */
		ret = real_connect(fd, (struct sockaddr *) un_addr, sizeof(struct sockaddr_un));
		if (ret == 0)
			file_descriptor_update(fd, FILE_DESCRIPTOR_TYPE_UNIX_DOMAIN, sun_path, -1);

		trace_printf(1, "connect(%d, \"%s\", ); [%d](AF_UNIX|AF_LOCAL)\n", fd, sun_path, ret);

		return ret;
	}

	trace_printf(1, "connect(%d, sa_family:%d)\n", fd, address->sa_family);

	return real_connect(fd, address, len);
}

RETRACE_REPLACE(connect)

#ifdef __linux__
int RETRACE_IMPLEMENTATION(bind)(int fd, __CONST_SOCKADDR_ARG _address, socklen_t len)
#else
int RETRACE_IMPLEMENTATION(bind)(int fd, const struct sockaddr *address, socklen_t len)
#endif
{
	rtr_bind_t real_bind;
#ifdef __linux__
	const struct sockaddr *address = _address.__sockaddr__;
#endif

	int ret;

	real_bind = RETRACE_GET_REAL(bind);

	if (address->sa_family == AF_INET) {
		struct sockaddr_in *bind_addr = (struct sockaddr_in *) address;

		const char *bind_ipaddr = inet_ntoa(bind_addr->sin_addr);
		int bind_port = ntohs(bind_addr->sin_port);

		/* connect to remote */
		ret = real_bind(fd, (struct sockaddr *) bind_addr, sizeof(struct sockaddr_in));
		if (ret == 0)
			file_descriptor_update(fd, FILE_DESCRIPTOR_TYPE_IPV4_BIND, bind_ipaddr, bind_port);

		trace_printf(1, "bind(%d, %s:%d, ); [%d](AF_INET)\n", fd, bind_ipaddr, bind_port, ret);

		return ret;
	} else if (address->sa_family == AF_UNIX) {
		struct sockaddr_un *un_addr = (struct sockaddr_un *) address;
		const char *sun_path = un_addr->sun_path;

		/* connect to local */
		ret = real_bind(fd, (struct sockaddr *) un_addr, sizeof(struct sockaddr_un));
		if (ret == 0)
			file_descriptor_update(fd, FILE_DESCRIPTOR_TYPE_IPV4_BIND, sun_path, -1);

		trace_printf(1, "bind(%d, \"%s\", ); [%d](AF_UNIX|AF_LOCAL)\n", fd, sun_path, ret);

		return ret;
	}

	return real_bind(fd, address, len);
}

RETRACE_REPLACE(bind)

#ifdef __linux__
int RETRACE_IMPLEMENTATION(accept)(int fd, __SOCKADDR_ARG _address, socklen_t *len)
#else
int RETRACE_IMPLEMENTATION(accept)(int fd, struct sockaddr *address, socklen_t *len)
#endif
{
	rtr_accept_t real_accept;
#ifdef __linux__
	struct sockaddr *address = _address.__sockaddr__;
#endif
	struct descriptor_info *di;
	int clnt_fd;

	real_accept = RETRACE_GET_REAL(accept);

	/* get descriptor info */
	di = file_descriptor_get(fd);
	if (di && di->type == FILE_DESCRIPTOR_TYPE_IPV4_BIND) {
		struct sockaddr_in clnt_addr;
		socklen_t addr_len = sizeof(struct sockaddr_in);

		clnt_fd = real_accept(fd, (struct sockaddr *) &clnt_addr, &addr_len);
		if (clnt_fd > 0) {
			const char *clnt_ipaddr = inet_ntoa(clnt_addr.sin_addr);
			int clnt_port = ntohs(clnt_addr.sin_port);

			/* add file descriptor for client socket */
			file_descriptor_update(clnt_fd, FILE_DESCRIPTOR_TYPE_IPV4_ACCEPT, clnt_ipaddr, clnt_port);

			trace_printf(1, "accept(%d, %s, %d); [client socket:%d]\n", fd, clnt_ipaddr, clnt_port, clnt_fd);
		} else
			trace_printf(1, "accept(%d, , , ); [error]\n", fd);

		return clnt_fd;
	}

	return real_accept(fd, address, len);
}

RETRACE_REPLACE(accept)
