#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <error.h>
#include <string.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "rpc.h"
#include "inspect.h"

static int g_sockfd;

int main(int argc, char **argv)
{
	char **exec_args;
	int i, pid, t;
	char buf[100];
	int sv[2];
	int n;
	struct rpc_redirect_header rh;
	struct rtr_call_info *info;
	struct rtr_arg_info *parg;

	if (socketpair(AF_UNIX, SOCK_SEQPACKET, 0, sv))
		error(1, 0, "Unable to create socketpair.");

	pid = fork();
	if (pid == 0) {
		/* make a copy of args for execv */
		exec_args = alloca(argc * sizeof(char *));
		for (i = 1; i < argc; i++)
			exec_args[i-1] = argv[i];
		exec_args[i] = NULL;

		close(sv[0]);

		sprintf(buf, "%d", sv[1]);
		setenv("RTR_SOCKFD", buf, 1);
		putenv("LD_PRELOAD=.libs/libretracerpc.so");

		execv(exec_args[0], exec_args);
		error(1, 0, "exec failed %d %s", errno, strerror(errno));
	} else {
		g_sockfd = sv[0];
		close(sv[1]);

		if (recv(g_sockfd, buf, 100, 0) == 0)
			error(1, 0, "Failed to trace program");

		if (memcmp(buf, rpc_version, 32) != 0)
			error(1, 0, "Version mismatch");

		for (;;) {
			if (recv(g_sockfd, buf, 100, 0) == 0) {
				waitpid(pid, NULL, 0);
				break;
			}
			if (((struct rpc_call_header *)buf)->call_type == RPC_POSTCALL) {
				info = rtr_get_call_info(((struct rpc_call_header *)buf)->function_id, buf + sizeof(struct rpc_call_header));
				printf("%s(", info->name);
				for (parg = info->args; parg->name; ++parg) {
					switch (parg->rpctype) {
					case RPC_INT:
						printf("%d", parg->value);
						break;
					case RPC_UINT:
						printf("%u", parg->value);
						break;
					case RPC_PTR:
						printf("%p", parg->value);
						break;
					case RPC_STR:
						printf("\"%s\"", parg->value);
						break;
					}
					if (parg[1].name)
						printf(", ");
				}
				printf(") = ");
				switch (info->rpctype) {
				case RPC_VOID:
					printf("void\n");
					break;
				case RPC_INT:
					printf("%d\n", info->result);
					break;
				case RPC_UINT:
					printf("%u\n", info->result);
					break;
				case RPC_PTR:
					printf("%p\n", info->result);
					break;
				case RPC_STR:
					printf("\"%s\"\n", info->result);
					break;
				}
				free(info);
			}
			((struct rpc_redirect_header *)buf)->complete = 0;
			((struct rpc_redirect_header *)buf)->redirect = 0;
			send(g_sockfd, buf, sizeof(struct rpc_redirect_header), 0);
		}
	}
}

