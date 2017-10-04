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

#include "config.h"

#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/wait.h>
#include <ctype.h>
#include <assert.h>

#include "rpc.h"
#include "frontend.h"

#define IOBUFLEN (64 * 1024)

static struct retrace_endpoint *
recv_endpoint(int fd)
{
	char version[32];
	struct retrace_endpoint *endpoint;
	struct rpc_control_header header;
	struct iovec iov[2] = {
		{&header, sizeof(header)},
		{version, 32} };
	struct msghdr msg = { 0 };
	struct cmsghdr *cmsg;
	union {
		char buf[CMSG_SPACE(sizeof(int))];
		struct cmsghdr align;
	} u;
	ssize_t iolen;

	msg.msg_iov = iov;
	msg.msg_iovlen = 2;
	msg.msg_control = u.buf;
	msg.msg_controllen = sizeof(u.buf);

	iolen = recvmsg(fd, &msg, 0);
	if (iolen == 0)
		return NULL;
	if (iolen == -1) {
		perror("error reading control socket");
		exit(EXIT_FAILURE);
	}

	/*
	 * check version sent with new fd
	 */
	if (memcmp(version, retrace_version, 32) != 0) {
		fprintf(stderr, "Version mismatch");
		exit(EXIT_FAILURE);
	}

	cmsg = CMSG_FIRSTHDR(&msg);

	endpoint = malloc(sizeof(struct retrace_endpoint));
	memcpy(&endpoint->fd, CMSG_DATA(cmsg), sizeof(int));
	endpoint->pid = header.pid;
	endpoint->ppid = header.ppid;
	SLIST_INIT(&endpoint->call_stack);

	return endpoint;
}

static struct retrace_endpoint *
add_endpoint(struct retrace_handle *handle)
{
	struct retrace_process_info *pi, *procinfo = NULL;
	struct retrace_endpoint *endpoint;
	struct retrace_process_handler *process_handler;

	endpoint = recv_endpoint(handle->control_fd);
	if (!endpoint)
		return NULL;

	SLIST_FOREACH(pi, &handle->processes, next) {
		if (pi->pid == endpoint->pid) {
			procinfo = pi;
			break;
		}
	}

	if (!procinfo) {
		procinfo = malloc(sizeof(struct retrace_process_info));
		if (!procinfo) {
			perror(__func__);
			exit(EXIT_FAILURE);
		}
		procinfo->pid = endpoint->pid;
		procinfo->next_thread_num = 0;
		SLIST_INSERT_HEAD(&handle->processes, procinfo, next);
	}

	endpoint->thread_num = procinfo->next_thread_num++;
	endpoint->call_num = 0;
	endpoint->call_depth = 0;
	endpoint->handle = handle;
	SLIST_INSERT_HEAD(&handle->endpoints, endpoint, next);

	SLIST_FOREACH(process_handler, &handle->process_handlers, next)
		process_handler->fn(endpoint);

	return endpoint;
}

struct retrace_handle *
retrace_start(char *const argv[], const int *trace_flags)
{
	int sv[2], i;
	char fd_str[16], trace_funcs[RPC_FUNCTION_COUNT + 1];
	pid_t pid;
	struct retrace_handle *handle;

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv)) {
		perror("Unable to create socketpair.");
		exit(EXIT_FAILURE);
	}

	pid = fork();

	if (pid == 0) {
		close(sv[0]);

		/*
		 * TODO: get proper path for shared library
		 */
#ifndef __APPLE__
		putenv("LD_PRELOAD=.libs/libretracerpc.so");
#else
		putenv("DYLD_INSERT_LIBRARIES=.libs/libretracerpc.dylib");
		putenv("DYLD_FORCE_FLAT_NAMESPACE=1");
#endif
		snprintf(fd_str, sizeof(fd_str), "%d", sv[1]);
		setenv("RTR_SOCKFD", fd_str, 1);

		for (i = 0; i < RPC_FUNCTION_COUNT; i++)
			trace_funcs[i] =
			    trace_flags[i] & RETRACE_TRACE ? '1' : '0';
		trace_funcs[RPC_FUNCTION_COUNT] = '\0';
		setenv("RTR_FUNCTIONS", trace_funcs, 1);

		execv(argv[0], argv);

		perror("Failed to exec");
		close(sv[1]);
		exit(EXIT_FAILURE);
	} else {
		close(sv[1]);

		handle = malloc(sizeof(struct retrace_handle));
		if (handle == NULL) {
			perror(NULL);
			exit(EXIT_FAILURE);
		}
		SLIST_INIT(&handle->endpoints);
		SLIST_INIT(&handle->processes);
		SLIST_INIT(&handle->process_handlers);

		for (i = 0; i < RPC_FUNCTION_COUNT; i++) {
			SLIST_INIT(&handle->precall_handlers[i]);
			SLIST_INIT(&handle->postcall_handlers[i]);
		}


		handle->control_fd = sv[0];

		return handle;
	}
}

void
retrace_add_precall_handler(
	struct retrace_handle *handle,
	enum retrace_function_id fid,
	retrace_precall_handler_t fn)
{
	struct retrace_precall_handler *handler;
	struct retrace_precall_handlers *handlers;

	handler = malloc(sizeof(struct retrace_precall_handler));
	handlers = &handle->precall_handlers[fid];
	if (handler) {
		handler->fn = fn;
		SLIST_INSERT_HEAD(handlers, handler, next);
	}
}

void
retrace_add_postcall_handler(
	struct retrace_handle *handle,
	enum retrace_function_id fid,
	retrace_postcall_handler_t fn)
{
	struct retrace_postcall_handler *handler;
	struct retrace_postcall_handlers *handlers;

	handler = malloc(sizeof(struct retrace_postcall_handler));
	handlers = &handle->postcall_handlers[fid];
	if (handler) {
		handler->fn = fn;
		SLIST_INSERT_HEAD(handlers, handler, next);
	}
}

void
retrace_add_process_handler(struct retrace_handle *handle,
	retrace_process_handler_t fn)
{
	struct retrace_process_handler *handler;
	struct retrace_process_handlers *handlers;

	handler = malloc(sizeof(struct retrace_process_handler));
	handlers = &handle->process_handlers;
	if (handler) {
		handler->fn = fn;
		SLIST_INSERT_HEAD(handlers, handler, next);
	}
}

void
retrace_set_user_data(struct retrace_handle *handle, void *data)
{
	handle->user_data = data;
}

void
retrace_close(struct retrace_handle *handle)
{
	struct retrace_endpoint *endpoint;
	struct retrace_process_info *procinfo;

	close(handle->control_fd);

	while (!SLIST_EMPTY(&handle->endpoints)) {
		endpoint = SLIST_FIRST(&handle->endpoints);
		SLIST_REMOVE_HEAD(&handle->endpoints, next);
		if (endpoint->fd != -1)
			close(endpoint->fd);
		free(endpoint);
	}

	while (!SLIST_EMPTY(&handle->processes)) {
		procinfo = SLIST_FIRST(&handle->processes);
		SLIST_REMOVE_HEAD(&handle->processes, next);
		waitpid(procinfo->pid, NULL, 0);
		free(procinfo);
	}

	free(handle);
}

int
rpc_send(int fd, const void *buf, size_t len)
{
	ssize_t result;

	do {
		result = send(fd, buf, len, 0);
	} while (result == -1 && errno == EINTR);

	return (result != -1 ? 1 : 0);
}

static void
init_call_context(struct retrace_endpoint *ep, const void *buf, size_t len)
{
	struct retrace_call_context *context;

	assert(len < sizeof(enum retrace_function_id) + sizeof(context->params));

	context = malloc(sizeof(struct retrace_call_context));
	context->function_id = *(enum retrace_function_id *)buf;
	len -= sizeof(enum retrace_function_id);
	if (len > 0) {
		buf = (enum retrace_function_id *)buf + 1;
		memcpy(context->params, buf, len);
	}
	context->user_data = NULL;
	context->free_user_data = NULL;
	SLIST_INSERT_HEAD(&ep->call_stack, context, next);
	++ep->call_depth;
}

static void
update_call_context(struct retrace_endpoint *ep, const void *buf, size_t len)
{
	struct retrace_call_context *context;

	assert(len <= sizeof(int) + sizeof(context->result));

	context = SLIST_FIRST(&ep->call_stack);
	context->_errno = *(int *)buf;
	memcpy(context->result, (((int *)buf) + 1), len - sizeof(int));
}

static void
free_call_context(struct retrace_endpoint *ep)
{
	struct retrace_call_context *context;

	context = SLIST_FIRST(&ep->call_stack);
	SLIST_REMOVE_HEAD(&ep->call_stack, next);
	if (context->user_data != NULL && context->free_user_data != NULL)
		context->free_user_data(context->user_data);
	free(context);
	--ep->call_depth;
	++ep->call_num;
}

static int
handle_precall(struct retrace_endpoint *ep)
{
	struct retrace_call_context *context;
	struct retrace_precall_handlers *handlers;
	struct retrace_precall_handler *handler;

	context = SLIST_FIRST(&ep->call_stack);
	handlers = &ep->handle->precall_handlers[context->function_id];

	SLIST_FOREACH(handler, handlers, next)
		if (handler->fn(ep, context) == 0)
			return 0;

	return 1;
}

static void
handle_postcall(struct retrace_endpoint *ep)
{
	struct retrace_call_context *context;
	struct retrace_postcall_handlers *handlers;
	struct retrace_postcall_handler *handler;

	context = SLIST_FIRST(&ep->call_stack);
	handlers = &ep->handle->postcall_handlers[context->function_id];

	SLIST_FOREACH(handler, handlers, next)
		handler->fn(ep, context);
}

void
retrace_trace(struct retrace_handle *handle)
{
	enum rpc_msg_type msg_type;
	char buf[RPC_MSG_LEN_MAX];
	fd_set readfds;
	struct retrace_endpoint *endpoint;
	int numfds, err;

	FD_ZERO(&readfds);

	for (;;) {
		numfds = handle->control_fd;
		FD_SET(handle->control_fd, &readfds);

		SLIST_FOREACH(endpoint, &handle->endpoints, next) {
			if (endpoint->fd != -1) {
				FD_SET(endpoint->fd, &readfds);
				if (endpoint->fd > numfds)
					numfds = endpoint->fd;
			}
		}
		err = select(numfds + 1, &readfds, NULL, NULL, NULL);
		if (err == -1 && errno == EINTR)
			continue;
		if (err == -1) {
			perror(__func__);
			exit(EXIT_FAILURE);
		}

		SLIST_FOREACH(endpoint, &handle->endpoints, next) {
			ssize_t len;

			if (endpoint->fd == -1)
				continue;

			if (!FD_ISSET(endpoint->fd, &readfds))
				continue;

			len = rpc_recv_message(endpoint->fd, &msg_type, buf);
			if (len <= 0) {
				FD_CLR(endpoint->fd, &readfds);
				close(endpoint->fd);
				endpoint->fd = -1;
				continue;
			}

			if (msg_type == RPC_MSG_CALL_INIT) {
				init_call_context(endpoint, buf, len);

				if (handle_precall(endpoint))
					rpc_send_message(endpoint->fd, RPC_MSG_DO_CALL, NULL, 0);
				else {
					handle_postcall(endpoint);
					rpc_send_message(endpoint->fd, RPC_MSG_DONE, NULL, 0);
					free_call_context(endpoint);
				}
			} else if (msg_type == RPC_MSG_CALL_RESULT) {
				update_call_context(endpoint, buf, len);
				handle_postcall(endpoint);
				rpc_send_message(endpoint->fd, RPC_MSG_DONE, NULL, 0);
				free_call_context(endpoint);
			} else
				assert(0);
		}

		if (FD_ISSET(handle->control_fd, &readfds)) {
			if (!add_endpoint(handle))
				break;
		}
	}
}

int
rpc_send_message(int fd, enum rpc_msg_type msg_type, const void *buf, size_t length)
{
	struct iovec iov[] = {
	    {&msg_type, sizeof(msg_type)}, {(void *)buf, length} };
	struct msghdr msg = {NULL, 0, iov, 2, NULL, 0, 0};
	ssize_t result;

	do {
		result = sendmsg(fd, &msg, 0);
	} while (result == -1 && errno == EINTR);

	return (result != -1 ? 1 : 0);
}

ssize_t
rpc_recv_message(int fd, enum rpc_msg_type *msg_type, void *buf)
{
	struct iovec iov[] = {
	    {msg_type, sizeof(*msg_type)}, {(void *)buf, RPC_MSG_LEN_MAX} };
	struct msghdr msg = {NULL, 0, iov, 2, NULL, 0, 0};
	ssize_t result;

	do {
		result = recvmsg(fd, &msg, 0);
	} while (result == -1 && errno == EINTR);

	return result;
}

int
rpc_recv(int fd, void *buf, size_t len)
{
	ssize_t n;
	size_t count = 0;

	while (count < len) {
		n = recv(fd, buf + count, len - count, 0);
		if (n == -1) {
			if (errno == EINTR)
				continue;
			return 0;
		}
		count += n;
	}

	return 1;
}

int
rpc_recv_string(int fd, char *buf, size_t len)
{
	ssize_t n;
	size_t count = 0;

	while (count < len) {
		n = recv(fd, buf + count, len - count, 0);
		if (n == -1) {
			if (errno == EINTR)
				continue;
			return 0;
		}
		count += n;
		if (buf[count - 1] == '\0')
			break;
	}
	return 1;
}

#if BACKTRACE
static void
discard_string(int fd)
{
	static const size_t len = 256;
	char buf[len];

	while (1) {
		buf[len - 1] = '\0';
		if (!rpc_recv_string(fd, buf, len)
		    || buf[len - 1] == '\0')
			return;
	}
}

int
retrace_fetch_backtrace(int fd, int depth, char *buffer, size_t len)
{
	struct rpc_backtrace_params bp = {depth};
	int result;

	rpc_send_message(fd, RPC_MSG_BACKTRACE, &bp, sizeof(bp));

	buffer[len - 1] = '\0';
	result = rpc_recv_string(fd, buffer, len);
	if (result && buffer[len - 1] != '\0') {
		discard_string(fd);
		buffer[len - 1] = '\0';
	}
	return result;
}
#endif

int
retrace_fetch_string(int fd, const char *address, char *buffer, size_t len)
{
	struct rpc_string_params sp = {(char *)address, len};

	rpc_send_message(fd, RPC_MSG_GET_STRING, &sp, sizeof(sp));

	return (rpc_recv_string(fd, buffer, len));
}

int
retrace_fetch_memory(int fd, const void *address, void *buffer,
	size_t len)
{
	struct rpc_memory_params mp = {(char *)address, len};

	rpc_send_message(fd, RPC_MSG_GET_MEMORY, &mp, sizeof(mp));

	if (rpc_recv(fd, buffer, len) != -1)
		return 1;

	return 0;
}

int
retrace_fetch_fileno(int fd, FILE *s, int *result)
{
	struct rpc_fileno_params fp = {s};

	rpc_send_message(fd, RPC_MSG_GET_FILENO, &fp, sizeof(fp));

	if (rpc_recv(fd, result, sizeof(*result)) != -1)
		return 1;

	return 0;
}

int
retrace_inject_errno(int fd, int e)
{
	struct rpc_errno_params ep = {e};

	return (rpc_send_message(fd, RPC_MSG_SET_ERRNO, &ep, sizeof(ep)));
}
