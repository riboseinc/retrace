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
#include <error.h>
#include <sys/socket.h>
#include <pthread.h>
#include <errno.h>

#include "shim.h"
#include "rpc.h"

static pthread_once_t g_once_control = PTHREAD_ONCE_INIT;
static pthread_key_t g_fdkey;
static int g_sockfd = -1;

static void
free_tls(void *p)
{
	close((long int)p);
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

	/*
	 * get fd of control socket from environment
	 * initialise the thread specific key
	 * add fork handler for child
	 */

	p = real_getenv("RTR_SOCKFD");
	if (p == 0)
		error(1, 0, "retrace env{RTR_SOCKFD} not set.");

	g_sockfd = 0;
	for (; *p; ++p) {
		if (*p < '0' || *p > '9')
			error(1, 0, "retrace env{RTR_SOCKFD} bad.");
		g_sockfd = g_sockfd * 10 + *p - '0';
	}

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

	int sv[2], *pfd;
	struct rpc_control_header control_header;
	struct iovec iov[] = {
	    { &control_header, sizeof(control_header) },
	    { (char *)rpc_version, 32 }
	};
	struct msghdr msg = { 0 };
	struct cmsghdr *cmsg;
	union {
		char buf[CMSG_SPACE(sizeof(int))];
		struct cmsghdr align;
	} u;

	msg.msg_iov = iov;
	msg.msg_iovlen = 2;
	msg.msg_control = u.buf;
	msg.msg_controllen = sizeof(u.buf);
	cmsg = CMSG_FIRSTHDR(&msg);
	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type = SCM_RIGHTS;
	cmsg->cmsg_len = CMSG_LEN(sizeof(int));
	pfd = (int *)CMSG_DATA(cmsg);

	socketpair(AF_UNIX, SOCK_SEQPACKET, 0, sv);
	*pfd = sv[0];

	control_header.pid = real_getpid();
	control_header.tid = pthread_self();

	sendmsg(g_sockfd, &msg, 0);

	close(sv[0]);
	return (sv[1]);
}

int
rpc_sockfd()
{
	/*
	 * we only need an fd per thread
	 * so we'll store (void *)(fd + 1)
	 * as address of thread local.
	 * malloc no good at this point
	 * for firefox
	 */

	long int fd;

	pthread_once(&g_once_control, init);

	fd = (long int)pthread_getspecific(g_fdkey);
	if (fd == 0) {
		fd = new_rpc_endpoint();
		pthread_setspecific(g_fdkey, (void *)fd);
	}
	return fd;
}

int
do_rpc(struct msghdr *send_msg, struct msghdr *recv_msg)
{
	int fd;

	fd = rpc_sockfd();

	if (fd == -1)
		return 0;

	while (sendmsg(fd, send_msg, 0) == -1) {
		if (errno != EINTR) {
			pthread_setspecific(g_fdkey, (void *)-1);
			return 0;
		}
	}

	while (recvmsg(fd, recv_msg, 0) == -1) {
		if (errno != EINTR) {
			pthread_setspecific(g_fdkey, (void *)-1);
			return 0;
		}
	}

	return 1;
}
