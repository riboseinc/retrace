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
#include "printf.h"
#include "netfuzz.h"
#include "sock.h"
#include <string.h>
#include <netinet/in.h>

int RETRACE_IMPLEMENTATION(socket)(int domain, int type, int protocol)
{
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_INT, PARAMETER_TYPE_INT, PARAMETER_TYPE_INT, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&domain, &type, &protocol};
	int sock;

	int err = 0;

	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "socket";
	event_info.function_group = RTR_FUNC_GRP_NET;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &sock;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;
	retrace_log_and_redirect_before(&event_info);

	if (rtr_get_net_fuzzing(NET_FUNC_ID_SOCKET, &err)) {
		event_info.extra_info = "[redirected]";
		event_info.event_flags = EVENT_FLAGS_PRINT_RAND_SEED;
		event_info.logging_level |= RTR_LOG_LEVEL_FUZZ;

		errno = err;
		sock = -1;
	} else {
		sock = real_socket(domain, type, protocol);
		if (sock > 0)
			file_descriptor_update(sock, FILE_DESCRIPTOR_TYPE_SOCK, "socket");
		else
			event_info.logging_level |= RTR_LOG_LEVEL_ERR;
	}

	retrace_log_and_redirect_after(&event_info);

	return sock;
}

RETRACE_REPLACE(socket, int, (int domain, int type, int protocol),
	(domain, type, protocol))


#ifdef __linux__
int RETRACE_IMPLEMENTATION(connect)(int fd, __CONST_SOCKADDR_ARG _address, socklen_t len)
#else
int RETRACE_IMPLEMENTATION(connect)(int fd, const struct sockaddr *address, socklen_t len)
#endif
{
	struct rtr_event_info event_info;
#ifdef __linux__
	const struct sockaddr *address = _address.__sockaddr__;
#endif
	const struct sockaddr *remote_addr = address;
	unsigned int parameter_types[] = {PARAMETER_TYPE_INT, PARAMETER_TYPE_STRUCT_SOCKADDR, PARAMETER_TYPE_INT, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&fd, &remote_addr, &len};
	int ret;

	int err;
	int fuzzing_enabled = 0;

	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "connect";
	event_info.function_group = RTR_FUNC_GRP_NET;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &ret;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;
	retrace_log_and_redirect_before(&event_info);

	if (!get_tracing_enabled())
		return real_connect(fd, address, len);

	/* check fuzzing config */
	if (rtr_get_net_fuzzing(NET_FUNC_ID_CONNECT, &err)) {
		event_info.extra_info = "[redirected]";
		event_info.event_flags = EVENT_FLAGS_PRINT_RAND_SEED;
		event_info.logging_level |= RTR_LOG_LEVEL_FUZZ;

		errno = err;
		fuzzing_enabled = 1;
		ret = -1;
	} else {
		char location[128];

		if (address->sa_family == AF_INET || address->sa_family == AF_INET6) {
			char remote_ipaddr[INET6_ADDRSTRLEN];
			int remote_port;

			char dst_ipaddr[INET6_ADDRSTRLEN];
			int dst_port;

			struct sockaddr redirect_addr;

			char *redirect_ipaddr = NULL;
			int redirect_port;

			int enabled_redirect = 0;

			RTR_CONFIG_HANDLE config = RTR_CONFIG_START;

			/* get IP address and port number to connect */
			if (address->sa_family == AF_INET) {
				inet_ntop(address->sa_family, &(((struct sockaddr_in *) address)->sin_addr),
					dst_ipaddr, INET_ADDRSTRLEN);
				dst_port = ntohs(((struct sockaddr_in *) address)->sin_port);
			} else {
				inet_ntop(address->sa_family, &(((struct sockaddr_in6 *) address)->sin6_addr),
					dst_ipaddr, INET6_ADDRSTRLEN);
				dst_port = ntohs(((struct sockaddr_in6 *) address)->sin6_port);
			}

			/* get configuration for redirection */
			while (1) {
				char *match_ipaddr = NULL;
				int match_port;

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

					redirect_addr.sa_family = address->sa_family;
					if (redirect_addr.sa_family == AF_INET) {
						inet_pton(redirect_addr.sa_family, redirect_ipaddr,
							&(((struct sockaddr_in *) &redirect_addr)->sin_addr));
						((struct sockaddr_in *) &redirect_addr)->sin_port = htons(redirect_port);
					} else {
						inet_pton(redirect_addr.sa_family, redirect_ipaddr,
							&(((struct sockaddr_in6 *) &redirect_addr)->sin6_addr));
						((struct sockaddr_in6 *) &redirect_addr)->sin6_port = htons(redirect_port);
					}

					/* set redirect flag */
					enabled_redirect = 1;
					break;
				}
			}

			if (enabled_redirect) {
				event_info.extra_info = "redirected";
				remote_addr = &redirect_addr;
			}

			/* connect to remote */
			ret = real_connect(fd, remote_addr, len);
			if (ret == 0) {
				real_snprintf(location, sizeof(location), "connected[%s:%d]",
					enabled_redirect ? redirect_ipaddr : dst_ipaddr,
					enabled_redirect ? redirect_port : dst_port);

				file_descriptor_update(fd, FILE_DESCRIPTOR_TYPE_SOCK, location);
			}

			event_info.logging_level |= RTR_LOG_LEVEL_REDIRECT;
			retrace_log_and_redirect_after(&event_info);

			return ret;
		} else if (address->sa_family == AF_UNIX) {
			struct sockaddr_un *un_addr = (struct sockaddr_un *) address;
			const char *sun_path = un_addr->sun_path;

			/* connect to local */
			ret = real_connect(fd, (struct sockaddr *) un_addr, sizeof(struct sockaddr_un));
			if (ret == 0) {
				real_snprintf(location, sizeof(location), "connected[%s]", sun_path);
				file_descriptor_update(fd, FILE_DESCRIPTOR_TYPE_SOCK, sun_path);
			}

			event_info.logging_level |= RTR_LOG_LEVEL_REDIRECT;
			retrace_log_and_redirect_after(&event_info);

			return ret;
		}
	}

	if (!fuzzing_enabled) {
		ret = real_connect(fd, address, len);
		if (errno)
			event_info.logging_level |= RTR_LOG_LEVEL_ERR;
	}

	retrace_log_and_redirect_after(&event_info);

	return ret;
}

#ifdef __linux__
RETRACE_REPLACE(connect, int,
	(int fd, __CONST_SOCKADDR_ARG _address, socklen_t len),
	(fd, _address, len))
#else
RETRACE_REPLACE(connect, int,
	(int fd, const struct sockaddr *address, socklen_t len),
	(fd, address, len))
#endif

#ifdef __linux__
int RETRACE_IMPLEMENTATION(bind)(int fd, __CONST_SOCKADDR_ARG _address, socklen_t len)
{
	const struct sockaddr *address = _address.__sockaddr__;
#else
int RETRACE_IMPLEMENTATION(bind)(int fd, const struct sockaddr *address, socklen_t len)
{
#endif
	int ret;
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_FILE_DESCRIPTOR, PARAMETER_TYPE_POINTER, PARAMETER_TYPE_INT, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&fd, &address, &len};

	int err;

	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "bind";
	event_info.function_group = RTR_FUNC_GRP_NET;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &ret;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;
	retrace_log_and_redirect_before(&event_info);

	if (rtr_get_net_fuzzing(NET_FUNC_ID_BIND, &err)) {
		event_info.extra_info = "[redirected]";
		event_info.event_flags = EVENT_FLAGS_PRINT_RAND_SEED;
		event_info.logging_level |= RTR_LOG_LEVEL_FUZZ;

		errno = err;
		ret = -1;
	} else {
		char location[128];

		ret = real_bind(fd, address, len);
		if (errno)
			event_info.logging_level |= RTR_LOG_LEVEL_ERR;

		if (ret == 0) {
			if (address->sa_family == AF_INET) {
				struct sockaddr_in *bind_addr = (struct sockaddr_in *) address;

				const char *bind_ipaddr = inet_ntoa(bind_addr->sin_addr);
				int bind_port = ntohs(bind_addr->sin_port);

				real_snprintf(location, sizeof(location), "bind[%s:%d]", bind_ipaddr, bind_port);
				file_descriptor_update(fd, FILE_DESCRIPTOR_TYPE_SOCK, location);
			} else if (address->sa_family == AF_UNIX) {
				struct sockaddr_un *un_addr = (struct sockaddr_un *) address;
				const char *sun_path = un_addr->sun_path;

				real_snprintf(location, sizeof(location), "bind[%s]", sun_path);
				file_descriptor_update(fd, FILE_DESCRIPTOR_TYPE_SOCK, location);
			}
		}
	}

	retrace_log_and_redirect_after(&event_info);

	return (ret);
}

#ifdef __linux__
RETRACE_REPLACE(bind, int,
	(int fd, __CONST_SOCKADDR_ARG _address, socklen_t len),
	(fd, _address, len))
#else
RETRACE_REPLACE(bind, int,
	(int fd, const struct sockaddr *address, socklen_t len),
	(fd, address, len))

#endif

#ifdef __linux__
int RETRACE_IMPLEMENTATION(accept)(int fd, __SOCKADDR_ARG _address, socklen_t *len)
{
	struct sockaddr *address = _address.__sockaddr__;
#else
int RETRACE_IMPLEMENTATION(accept)(int fd, struct sockaddr *address, socklen_t *len)
{
#endif
	struct descriptor_info *di;
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_FILE_DESCRIPTOR,
					  PARAMETER_TYPE_POINTER,
					  PARAMETER_TYPE_POINTER,
					  PARAMETER_TYPE_END};
	void const *parameter_values[] = {&fd, &address, &len};
	int clnt_fd;
	struct sockaddr_in local_addr;
	socklen_t local_len = 0;

	int err;

	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "accept";
	event_info.function_group = RTR_FUNC_GRP_NET;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_FILE_DESCRIPTOR;
	event_info.return_value = &clnt_fd;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;
	retrace_log_and_redirect_before(&event_info);

	if (rtr_get_net_fuzzing(NET_FUNC_ID_ACCEPT, &err)) {
		event_info.extra_info = "[redirected]";
		event_info.event_flags = EVENT_FLAGS_PRINT_RAND_SEED;
		event_info.logging_level |= RTR_LOG_LEVEL_FUZZ;

		errno = err;
		clnt_fd = -1;
	} else {
		char location[128];
		char ipaddr[INET6_ADDRSTRLEN];

		memset(location, 0, sizeof(location));

		clnt_fd = real_accept(fd, address, len);
		if (clnt_fd > 0) {
			switch (address->sa_family) {
			case AF_INET:
				inet_ntop(AF_INET, &(((struct sockaddr_in *) address)->sin_addr), ipaddr, INET_ADDRSTRLEN);
				real_snprintf(location, sizeof(location), "%s:%d", ipaddr,
					ntohs(((struct sockaddr_in *) address)->sin_port));

				break;

			case AF_INET6:
				inet_ntop(AF_INET6, &(((struct sockaddr_in6 *) address)->sin6_addr), ipaddr, INET6_ADDRSTRLEN);
				real_snprintf(location, sizeof(location), "%s:%d", ipaddr,
					ntohs(((struct sockaddr_in6 *) address)->sin6_port));

				break;

			case AF_UNIX:
				real_strcpy(location, ((struct sockaddr_un *) address)->sun_path);

				break;

			default:
				real_strcpy(location, "Unknown");
				break;
			}

			file_descriptor_update(clnt_fd, FILE_DESCRIPTOR_TYPE_SOCK, location);
		} else
			event_info.logging_level |= RTR_LOG_LEVEL_ERR;
	}

	retrace_log_and_redirect_after(&event_info);

	return (clnt_fd);
}

#ifdef __linux__
RETRACE_REPLACE(accept, int,
	(int fd, __SOCKADDR_ARG _address, socklen_t *len),
	(fd, _address, len))
#else
RETRACE_REPLACE(accept, int,
	(int fd, struct sockaddr *address, socklen_t *len),
	(fd, address, len))
#endif

int RETRACE_IMPLEMENTATION(setsockopt)(int fd, int level, int optname, const void *optval, socklen_t optlen)
{
	int ret;
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_FILE_DESCRIPTOR,
					  PARAMETER_TYPE_INT,
					  PARAMETER_TYPE_INT,
					  PARAMETER_TYPE_POINTER,
					  PARAMETER_TYPE_INT,
					  PARAMETER_TYPE_END};
	void const *parameter_values[] = {&fd, &level, &optname, &optval, &optlen};


	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "setsockopt";
	event_info.function_group = RTR_FUNC_GRP_NET;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &ret;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;
	retrace_log_and_redirect_before(&event_info);

	ret = real_setsockopt(fd, level, optname, optval, optlen);
	if (errno)
		event_info.logging_level |= RTR_LOG_LEVEL_ERR;

	retrace_log_and_redirect_after(&event_info);

	return ret;
}

RETRACE_REPLACE(setsockopt, int,
	(int fd, int level, int optname, const void *optval,
	    socklen_t optlen),
	(fd, level, optname, optval, optlen))


ssize_t RETRACE_IMPLEMENTATION(send)(int sockfd, const void *buf, size_t len, int flags)
{
	int ret;
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_FILE_DESCRIPTOR, PARAMETER_TYPE_MEMORY_BUFFER, PARAMETER_TYPE_INT, PARAMETER_TYPE_END};
	void const *parameter_values[] = {&sockfd, &len, &buf, &len, &flags};

	int err;

	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "send";
	event_info.function_group = RTR_FUNC_GRP_NET;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &ret;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;
	retrace_log_and_redirect_before(&event_info);

	if (rtr_get_net_fuzzing(NET_FUNC_ID_SEND, &err)) {
		event_info.extra_info = "[redirected]";
		event_info.event_flags = EVENT_FLAGS_PRINT_RAND_SEED;
		event_info.logging_level |= RTR_LOG_LEVEL_FUZZ;

		errno = err;
		ret = -1;
	} else {
		ret = real_send(sockfd, buf, len, flags);
		if (errno)
			event_info.logging_level |= RTR_LOG_LEVEL_ERR;
	}

	retrace_log_and_redirect_after(&event_info);

	return ret;
}

RETRACE_REPLACE(send, ssize_t,
	(int sockfd, const void *buf, size_t len, int flags),
	(sockfd, buf, len, flags))


ssize_t RETRACE_IMPLEMENTATION(sendto)(int sockfd, const void *buf, size_t len, int flags,
		const struct sockaddr *dest_addr, socklen_t addrlen)
{
	int ret;
	struct descriptor_info *di;
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_FILE_DESCRIPTOR,
					  PARAMETER_TYPE_MEMORY_BUFFER,
					  PARAMETER_TYPE_INT,
					  PARAMETER_TYPE_POINTER,
					  PARAMETER_TYPE_INT,
					  PARAMETER_TYPE_END};
	void const *parameter_values[] = {&sockfd, &len, &buf, &len, &flags, &dest_addr, &addrlen};

	int err;

	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "sendto";
	event_info.function_group = RTR_FUNC_GRP_NET;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &ret;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;
	retrace_log_and_redirect_before(&event_info);

	if (rtr_get_net_fuzzing(NET_FUNC_ID_SENDTO, &err)) {
		event_info.extra_info = "[redirected]";
		event_info.event_flags = EVENT_FLAGS_PRINT_RAND_SEED;
		event_info.logging_level |= RTR_LOG_LEVEL_FUZZ;

		errno = err;
		ret = -1;
	} else {
		ret = real_sendto(sockfd, buf, len, flags, dest_addr, addrlen);
		if (errno)
			event_info.logging_level |= RTR_LOG_LEVEL_ERR;
	}

	retrace_log_and_redirect_after(&event_info);

	return ret;
}

RETRACE_REPLACE(sendto, ssize_t,
	(int sockfd, const void *buf, size_t len, int flags,
	    const struct sockaddr *dest_addr, socklen_t addrlen),
	(sockfd, buf, len, flags, dest_addr, addrlen))


ssize_t RETRACE_IMPLEMENTATION(sendmsg)(int sockfd, const struct msghdr *msg, int flags)
{
	int ret;
	struct descriptor_info *di;
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_FILE_DESCRIPTOR,
					  PARAMETER_TYPE_POINTER,
					  PARAMETER_TYPE_IOVEC,
					  PARAMETER_TYPE_INT,
					  PARAMETER_TYPE_END};
	void const *parameter_values[] = {&sockfd, &msg, &msg->msg_iovlen, &msg->msg_iov, &flags};

	int err;

	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "sendmsg";
	event_info.function_group = RTR_FUNC_GRP_NET;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &ret;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;
	retrace_log_and_redirect_before(&event_info);

	if (rtr_get_net_fuzzing(NET_FUNC_ID_SENDMSG, &err)) {
		event_info.extra_info = "[redirected]";
		event_info.event_flags = EVENT_FLAGS_PRINT_RAND_SEED;
		event_info.logging_level |= RTR_LOG_LEVEL_FUZZ;

		errno = err;
		ret = -1;
	} else {
		ret = real_sendmsg(sockfd, msg, flags);
		if (errno)
			event_info.logging_level |= RTR_LOG_LEVEL_ERR;
	}

	retrace_log_and_redirect_after(&event_info);

	return ret;
}

RETRACE_REPLACE(sendmsg, ssize_t,
	(int sockfd, const struct msghdr *msg, int flags),
	(sockfd, msg, flags))


ssize_t RETRACE_IMPLEMENTATION(recv)(int sockfd, void *buf, size_t len, int flags)
{
	ssize_t recv_len = 0;
	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_FILE_DESCRIPTOR,
					  PARAMETER_TYPE_MEMORY_BUFFER,
					  PARAMETER_TYPE_INT,
					  PARAMETER_TYPE_INT,
					  PARAMETER_TYPE_END};
	void const *parameter_values[] = {&sockfd, &recv_len, &buf, &len, &flags};

	int err;

	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "recv";
	event_info.function_group = RTR_FUNC_GRP_NET;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &recv_len;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;
	retrace_log_and_redirect_before(&event_info);

	if (rtr_get_net_fuzzing(NET_FUNC_ID_RECV, &err)) {
		event_info.extra_info = "[redirected]";
		event_info.event_flags = EVENT_FLAGS_PRINT_RAND_SEED;
		event_info.logging_level |= RTR_LOG_LEVEL_FUZZ;

		errno = err;
		recv_len = -1;
	} else {
		recv_len = real_recv(sockfd, buf, len, flags);
		if (errno)
			event_info.logging_level |= RTR_LOG_LEVEL_ERR;
	}

	retrace_log_and_redirect_after(&event_info);

	return recv_len;
}

RETRACE_REPLACE(recv, ssize_t,
	(int sockfd, void *buf, size_t len, int flags),
	(sockfd, buf, len, flags))


ssize_t RETRACE_IMPLEMENTATION(recvfrom)(int sockfd, void *buf, size_t len, int flags,
	struct sockaddr *src_addr, socklen_t *addrlen)
{
	ssize_t recv_len = 0;
	struct rtr_event_info event_info;
	unsigned int parameter_types_full[] = {PARAMETER_TYPE_FILE_DESCRIPTOR,
						PARAMETER_TYPE_MEMORY_BUFFER,
						PARAMETER_TYPE_INT,
						PARAMETER_TYPE_INT,
						PARAMETER_TYPE_STRUCT_SOCKADDR,
						PARAMETER_TYPE_END};
	void const *parameter_values_full[] = {&sockfd, &recv_len, &buf, &len, &flags, &src_addr};

	unsigned int parameter_types_short[] = {PARAMETER_TYPE_FILE_DESCRIPTOR,
						PARAMETER_TYPE_MEMORY_BUFFER,
						PARAMETER_TYPE_INT,
						PARAMETER_TYPE_INT,
						PARAMETER_TYPE_END};
	void const *parameter_values_short[] = {&sockfd, &recv_len, &buf, &len, &flags};

	int err;

	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "recvfrom";
	if (src_addr) {
		event_info.parameter_types = parameter_types_full;
		event_info.parameter_values = (void **) parameter_values_full;
	} else {
		event_info.parameter_types = parameter_types_short;
		event_info.parameter_values = (void **) parameter_values_short;
	}
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &recv_len;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;

	retrace_log_and_redirect_before(&event_info);

	if (rtr_get_net_fuzzing(NET_FUNC_ID_RECVFROM, &err)) {
		event_info.extra_info = "[redirected]";
		event_info.event_flags = EVENT_FLAGS_PRINT_RAND_SEED;
		event_info.logging_level |= RTR_LOG_LEVEL_FUZZ;

		errno = err;
		recv_len = -1;
	} else {
		recv_len = real_recvfrom(sockfd, buf, len, flags, src_addr, addrlen);
		if (errno)
			event_info.logging_level |= RTR_LOG_LEVEL_ERR;
	}

	retrace_log_and_redirect_after(&event_info);

	return recv_len;
}

RETRACE_REPLACE(recvfrom, ssize_t,
	(int sockfd, void *buf, size_t len, int flags,
	    struct sockaddr *src_addr, socklen_t *addrlen),
	(sockfd, buf, len, flags, src_addr, addrlen))


ssize_t RETRACE_IMPLEMENTATION(recvmsg)(int sockfd, struct msghdr *msg, int flags)
{
	ssize_t recv_len = 0;

	struct rtr_event_info event_info;
	unsigned int parameter_types[] = {PARAMETER_TYPE_FILE_DESCRIPTOR,
						PARAMETER_TYPE_IOVEC,
						PARAMETER_TYPE_INT,
						PARAMETER_TYPE_END};
	void const *parameter_values[] = {&sockfd, &msg->msg_iovlen, &msg->msg_iov, &flags};

	int err;

	memset(&event_info, 0, sizeof(event_info));
	event_info.function_name = "recvmsg";
	event_info.function_group = RTR_FUNC_GRP_NET;
	event_info.parameter_types = parameter_types;
	event_info.parameter_values = (void **) parameter_values;
	event_info.return_value_type = PARAMETER_TYPE_INT;
	event_info.return_value = &recv_len;
	event_info.logging_level = RTR_LOG_LEVEL_NOR;

	retrace_log_and_redirect_before(&event_info);

	if (rtr_get_net_fuzzing(NET_FUNC_ID_RECVMSG, &err)) {
		event_info.extra_info = "[redirected]";
		event_info.event_flags = EVENT_FLAGS_PRINT_RAND_SEED;
		event_info.logging_level |= RTR_LOG_LEVEL_FUZZ;

		errno = err;
		recv_len = -1;
	} else {
		recv_len = real_recvmsg(sockfd, msg, flags);
		if (errno)
			event_info.logging_level |= RTR_LOG_LEVEL_ERR;
	}

	retrace_log_and_redirect_after(&event_info);

	return recv_len;
}

RETRACE_REPLACE(recvmsg, ssize_t, (int sockfd, struct msghdr *msg, int flags),
	(sockfd, msg, flags))
