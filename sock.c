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
#include <string.h>


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
#ifdef __linux__
	const struct sockaddr *address = _address.__sockaddr__;
#endif

	int ret;

	real_connect = RETRACE_GET_REAL(connect);
	real_strcmp = RETRACE_GET_REAL(strcmp);

	if (!get_tracing_enabled())
		return real_connect(fd, address, len);

	if (address->sa_family == AF_INET) {
		struct sockaddr_in *dst_addr = (struct sockaddr_in *) address;

		struct sockaddr_in *remote_addr;
		const char *remote_ipaddr;
		int remote_port;

		struct sockaddr_in redirect_addr;
		int enabled_redirect = 0;

		RTR_CONFIG_HANDLE config = NULL;

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
				break;
			}
		}

		/* set remote address info */
		remote_addr = enabled_redirect ? &redirect_addr : (struct sockaddr_in *) address;
		remote_ipaddr = inet_ntoa(remote_addr->sin_addr);
		remote_port = ntohs(remote_addr->sin_port);

		/* connect to remote */
		ret = real_connect(fd, (struct sockaddr *) remote_addr, sizeof(struct sockaddr_in));
		if (ret == 0)
			file_descriptor_update(fd, FILE_DESCRIPTOR_TYPE_IPV4_CONNECT, remote_ipaddr, remote_port);

		trace_printf(1, "connect(%d, %s:%d%s); [%d](AF_INET)\n",
		    fd, remote_ipaddr, remote_port,
		    enabled_redirect ? " [redirected]" : "", ret);

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

		/* bind socket */
		ret = real_bind(fd, (struct sockaddr *) bind_addr, sizeof(struct sockaddr_in));
		if (ret == 0)
			file_descriptor_update(fd, FILE_DESCRIPTOR_TYPE_IPV4_BIND, bind_ipaddr, bind_port);

		trace_printf(1, "bind(%d, %s:%d, ); [%d](AF_INET)\n", fd, bind_ipaddr, bind_port, ret);

		return ret;
	} else if (address->sa_family == AF_UNIX) {
		struct sockaddr_un *un_addr = (struct sockaddr_un *) address;
		const char *sun_path = un_addr->sun_path;

		/* bind local socket */
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

int RETRACE_IMPLEMENTATION(setsockopt)(int fd, int level, int optname, const void *optval, socklen_t optlen)
{
	rtr_setsockopt_t real_setsockopt;
	int ret;

	real_setsockopt = RETRACE_GET_REAL(setsockopt);

	ret = real_setsockopt(fd, level, optname, optval, optlen);

	trace_printf(1, "setsockopt(%d, %d, %d, %p) [return:%d]\n", fd, level, optname, optval, ret);
#if 0
	for (i = 0; i < optlen; i++)
		trace_dump_data((unsigned char *)optval + i, sizeof(socklen_t));
#endif

	return ret;
}

RETRACE_REPLACE(setsockopt)

ssize_t RETRACE_IMPLEMENTATION(send)(int sockfd, const void *buf, size_t len, int flags)
{
	rtr_send_t real_send;
	int ret;

	real_send = RETRACE_GET_REAL(send);

	ret = real_send(sockfd, buf, len, flags);
	trace_printf(1, "send(%d, %p, %d, %d) [return: %d]\n", sockfd, buf, len, flags, ret);
	if (ret > 0)
		trace_dump_data((unsigned char *)buf, ret);

	return ret;
}

RETRACE_REPLACE(send)

ssize_t RETRACE_IMPLEMENTATION(sendto)(int sockfd, const void *buf, size_t len, int flags,
		const struct sockaddr *dest_addr, socklen_t addrlen)
{
	rtr_sendto_t real_sendto;
	int ret;

	struct descriptor_info *di;

	real_sendto = RETRACE_GET_REAL(sendto);

	ret = real_sendto(sockfd, buf, len, flags, dest_addr, addrlen);
	if (dest_addr) {
		if (dest_addr->sa_family == AF_INET) {
			struct sockaddr_in *in_addr = (struct sockaddr_in *)dest_addr;

			const char *remote_addr = inet_ntoa(in_addr->sin_addr);
			int remote_port = ntohs(in_addr->sin_port);

			/* update descriptor info */
			di = file_descriptor_get(sockfd);
			if (!di && ret > 0)
				file_descriptor_update(sockfd, FILE_DESCRIPTOR_TYPE_UDP_SENDTO, remote_addr, remote_port);

			trace_printf(1, "sendto(%d, %p, %d, %d, %s:%d[AF_INET]), [return: %d]\n",
				sockfd, buf, len, flags, remote_addr, remote_port, ret);
		} else if (dest_addr->sa_family == AF_UNIX) {
			struct sockaddr_un *un_addr = (struct sockaddr_un *)dest_addr;
			const char *remote_path = un_addr->sun_path;

			/* update descriptor info */
			di = file_descriptor_get(sockfd);
			if (!di && ret > 0)
				file_descriptor_update(sockfd, FILE_DESCRIPTOR_TYPE_UDP_SENDTO, remote_path, -1);

			trace_printf(1, "sendto(%d, %p, %d, %d, %s[AF_UNIX|AF_LOCAL]), [return: %d]\n",
				sockfd, buf, len, flags, remote_path, ret);
		}
	} else
		trace_printf(1, "sendto(%d, %p, %d, %d), [return: %d]\n",
			sockfd, buf, len, flags, ret);

	/* dump sending data */
	if (ret > 0)
		trace_dump_data((unsigned char *)buf, ret);

	return ret;
}

RETRACE_REPLACE(sendto)

ssize_t RETRACE_IMPLEMENTATION(sendmsg)(int sockfd, const struct msghdr *msg, int flags)
{
	rtr_sendmsg_t real_sendmsg;
	int i, ret;

	struct descriptor_info *di;

	real_sendmsg = RETRACE_GET_REAL(sendmsg);

	ret = real_sendmsg(sockfd, msg, flags);
	trace_printf(1, "sendmsg(%d, %p, %d) [return:%d]\n", sockfd, msg, flags, ret);

	/* update descriptor info */
	di = file_descriptor_get(sockfd);
	if (!di && msg->msg_name)
		file_descriptor_update(sockfd, FILE_DESCRIPTOR_TYPE_UDP_SENDMSG, (char *)msg->msg_name, -1);

	/* dump message data */
	for (i = 0; i < msg->msg_iovlen; i++) {
		struct iovec *msg_iov = &msg->msg_iov[i];

		if (msg_iov->iov_len > 0)
			trace_dump_data((unsigned char *) msg_iov->iov_base, msg_iov->iov_len);
	}

	return ret;
}

RETRACE_REPLACE(sendmsg)

ssize_t RETRACE_IMPLEMENTATION(recv)(int sockfd, void *buf, size_t len, int flags)
{
	rtr_recv_t real_recv;
	int recv_len;

	real_recv = RETRACE_GET_REAL(recv);

	recv_len = real_recv(sockfd, buf, len, flags);
	trace_printf(1, "recv(%d, %p, %d, %d) [return: %d]\n", sockfd, buf, len, flags, recv_len);

	if (recv_len > 0)
		trace_dump_data((unsigned char *)buf, recv_len);

	return recv_len;
}

RETRACE_REPLACE(recv)

ssize_t RETRACE_IMPLEMENTATION(recvfrom)(int sockfd, void *buf, size_t len, int flags,
	struct sockaddr *src_addr, socklen_t *addrlen)
{
	rtr_recvfrom_t real_recvfrom;
	int recv_len;

	real_recvfrom = RETRACE_GET_REAL(recvfrom);

	recv_len = real_recvfrom(sockfd, buf, len, flags, src_addr, addrlen);
	if (src_addr) {
		if (src_addr->sa_family == AF_INET) {
			struct sockaddr_in *in_addr = (struct sockaddr_in *)src_addr;
			const char *src_ipaddr = inet_ntoa(in_addr->sin_addr);
			int src_port = ntohs(in_addr->sin_port);

			trace_printf(1, "recvfrom(%d, %p, %d, %d, %s:%d[AF_INET]), [return: %d]\n",
				sockfd, buf, len, flags, src_ipaddr, src_port, recv_len);
		} else if (src_addr->sa_family == AF_UNIX) {
			struct sockaddr_un *un_addr = (struct sockaddr_un *)src_addr;
			const char *src_path = un_addr->sun_path;

			trace_printf(1, "recvfrom(%d, %p, %d, %d, %s[AF_UNIX|AF_LOCAL]), [return: %d]\n",
				sockfd, buf, len, flags, src_path, recv_len);
		}
	} else
		trace_printf(1, "recvfrom(%d, %p, %d, %d), [return: %d]\n",
			sockfd, buf, len, flags, recv_len);

	/* dump sending data */
	if (recv_len > 0)
		trace_dump_data((unsigned char *)buf, recv_len);

	return recv_len;
}

RETRACE_REPLACE(recvfrom)

ssize_t RETRACE_IMPLEMENTATION(recvmsg)(int sockfd, struct msghdr *msg, int flags)
{
	rtr_recvmsg_t real_recvmsg;
	int i, recv_len;

	real_recvmsg = RETRACE_GET_REAL(recvmsg);

	recv_len = real_recvmsg(sockfd, msg, flags);
	trace_printf(1, "recvmsg(%d, %p, %d) [return:%d]\n", sockfd, msg, flags, recv_len);

	/* dump message data */
	for (i = 0; i < msg->msg_iovlen; i++) {
		struct iovec *msg_iov = &msg->msg_iov[i];

		if (msg_iov->iov_len > 0)
			trace_dump_data((unsigned char *) msg_iov->iov_base, msg_iov->iov_len);
	}

	return recv_len;
}

RETRACE_REPLACE(recvmsg)
