#include <unistd.h>
#include <sys/types.h>
#include <error.h>

#include "shim.h"
#include "rpc.h"

static int g_sockfd = -1;

static int g_inrpc;

static void
init_sockfd()
{
	const char *p;

	/* TODO something better on error */
	p = real_getenv("RTR_SOCKFD");
	if (p == 0)
		error(1, 0, "retrace env{RTR_SOCKFD} not set.");

	g_sockfd = 0;
	for (p; *p; ++p) {
		if (*p < '0' || *p > '9')
			error(1, 0, "retrace env{RTR_SOCKFD} bad.");
		g_sockfd = g_sockfd * 10 + *p - '0';
	}

	send(g_sockfd, rpc_version, 32, 0);
}

void rpc_recv(struct msghdr *msg)
{
	if (g_inrpc == 1)
		return;

	if (g_sockfd == -1)
		init_sockfd();

	g_inrpc = 1;
	recvmsg(g_sockfd, msg, 0);
	g_inrpc = 0;
}

void
rpc_send(struct msghdr *msg)
{
	if (g_inrpc == 1)
		return;

	if (g_sockfd == -1)
		init_sockfd();

	g_inrpc = 1;
	sendmsg(g_sockfd, msg, 0);
	g_inrpc = 0;
}
