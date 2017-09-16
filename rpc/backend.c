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

#include "../config.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>
#include <errno.h>

#if BACKTRACE
#include <execinfo.h>
#endif

#include "shim.h"
#include "rpc.h"
#include "backend.h"

/*
 * TODO: pthread_setspecific(g_fdkey, (void *)-1);
 */

static pthread_once_t g_once_control = PTHREAD_ONCE_INIT;
static pthread_key_t g_fdkey;
static int g_sockfd = -1;

static void
free_tls(void *p)
{
	real_close((long int)p);
}

static void
atfork_child()
{
	/*
	 * remove all existing connections in the child
	 * so forked process gets a new connection
	 * Unfortunately, we can't determine whether there are
	 * other pthread_atfork handlers so possibly some tracing
	 * will go over the old connection
	 */

	pthread_key_delete(g_fdkey);
	pthread_key_create(&g_fdkey, free_tls);
}

static void
init(void)
{
	const char *p;
	int i;

	/*
	 * get fd of control socket from environment
	 * initialise the thread specific key
	 * add fork handler for child
	 */

	p = real_getenv("RTR_SOCKFD");
	if (p != 0) {
		g_sockfd = 0;
		for (; *p; ++p) {
			if (*p < '0' || *p > '9') {
				real_fprintf(stderr,
				    "retrace env{RTR_SOCKFD} bad.");
				g_sockfd = -1;
				break;
			}
			g_sockfd = g_sockfd * 10 + *p - '0';
		}
	} else
		real_fprintf(stderr, "retrace env{RTR_SOCKFD} not set.");

	/*
	 * setup traced functions
	 */

	p = real_getenv("RTR_FUNCTIONS");
	if (p)
		for (i = 0; i < sizeof(trace_functions) && p[i]; i++)
			trace_functions[i] = p[i] == '1' ? 1 : 0;

	pthread_key_create(&g_fdkey, free_tls);

	pthread_atfork(NULL, NULL, atfork_child);
}

static int
new_rpc_endpoint()
{
	/*
	 * create a socketpair and send one to front end
	 * via the control socket
	 */

	int sv[2], *pfd, err;
	struct rpc_control_header control_header;
	struct iovec iov[] = {
	    {&control_header, sizeof(control_header)},
	    {(char *)retrace_version, 32 } };
	struct msghdr msg = { 0 };
	struct cmsghdr *cmsg;
	union {
		char buf[CMSG_SPACE(sizeof(int))];
		struct cmsghdr align;
	} u;

	if (g_sockfd == -1)
		return -1;
	msg.msg_iov = iov;
	msg.msg_iovlen = 2;
	msg.msg_control = u.buf;
	msg.msg_controllen = sizeof(u.buf);
	cmsg = CMSG_FIRSTHDR(&msg);
	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type = SCM_RIGHTS;
	cmsg->cmsg_len = CMSG_LEN(sizeof(int));
	pfd = (int *)CMSG_DATA(cmsg);

	err = real_socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
	if (err == -1)
		return -1;

	*pfd = sv[0];

	control_header.pid = real_getpid();
	control_header.tid = pthread_self();

	err = real_sendmsg(g_sockfd, &msg, 0);
	if (err == -1) {
		real_close(sv[0]);
		real_close(sv[1]);
		return -1;
	}

	real_close(sv[0]);
	return (sv[1]);
}

int
#if defined(__OpenBSD__)
rpc_get_sockfd(enum retrace_function_id fid)
#else
rpc_get_sockfd()
#endif
{
	/*
	 * we only need an fd per thread
	 * so we'll store (void *)fd
	 * as address of thread local.
	 */

	long int fd;

#if defined(__OpenBSD__)
	static int initialised;

	/*
	 * BSDs pthread_once calls these so tracing them
	 * before init is complete causes infinite recusion
	 */
	if ((fid == RPC_getenv || fid == RPC_malloc) && initialised == 0)
		return -1;

	pthread_once(&g_once_control, init);

	initialised = 1;
#else
	pthread_once(&g_once_control, init);
#endif

	fd = (long int)pthread_getspecific(g_fdkey);
	if (fd == 0) {
		fd = new_rpc_endpoint();
		pthread_setspecific(g_fdkey, (void *)fd);
	}
	return fd;
}

void
rpc_set_sockfd(long int fd)
{
	pthread_setspecific(g_fdkey, (void *)fd);
}

int
rpc_send_message(int fd, enum rpc_msg_type msg_type, const void *buf, size_t length)
{
	struct iovec iov[] = {
	    {&msg_type, sizeof(msg_type)}, {(void *)buf, length} };
	struct msghdr msg = {NULL, 0, iov, 2, NULL, 0, 0};
	ssize_t result;

	do {
		result = real_sendmsg(fd, &msg, 0);
	} while (result == -1 && errno == EINTR);

	return (result != -1 ? 1 : 0);
}

int
rpc_recv_message(int fd, enum rpc_msg_type *msg_type, void *buf)
{
	struct iovec iov[] = {
	    {msg_type, sizeof(*msg_type)}, {(void *)buf, RPC_MSG_LEN_MAX} };
	struct msghdr msg = {NULL, 0, iov, 2, NULL, 0, 0};
	ssize_t result;

	do {
		result = real_recvmsg(fd, &msg, 0);
	} while (result == -1 && errno == EINTR);

	return (result != -1 ? 1 : 0);
}

static int
send_string(int fd, const char *s, size_t len)
{
	size_t n;
	ssize_t result;

	n = real_strlen(s) + 1;

	if (n > len)
		n = len;

	do {
		result = real_send(fd, s, n, 0);
	} while (result == -1 && errno == EINTR);

	return (result != -1 ? 1 : 0);
}

#if BACKTRACE
static int
send_backtrace(int fd, int depth)
{
	void *addresses[depth + 3];
	int frames;

	rpc_set_sockfd(-1);
	frames = backtrace(addresses, depth + 3);
	backtrace_symbols_fd(addresses + 3, frames - 3, fd);
	rpc_set_sockfd(fd);

	return send_string(fd, "", 1);
}
#endif

int
rpc_handle_message(int fd, enum rpc_msg_type msg_type, void *buf)
{
#if BACKTRACE
	struct rpc_backtrace_params *btp = buf;
#endif
	struct rpc_string_params *sp = buf;

	if (fd == -1)
		return 0;

	switch (msg_type) {
	case RPC_MSG_GET_STRING:
		if (!send_string(fd, (char *)sp->address, sp->length))
			return 0;
		break;
#if BACKTRACE
	case RPC_MSG_BACKTRACE:
		if (!send_backtrace(fd, btp->depth))
			return 0;
		break;
#endif
	default:
		real_fprintf(stderr, "Unknown RPC message type (%d)", msg_type);
		break;
	}
	return 1;
}
