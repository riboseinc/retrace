#ifndef __RTR_RPC_H__
#define __RTR_RPC_H__

#include <sys/types.h>
#include <sys/socket.h>
#include "shim.h"

extern const char *rpc_version;

enum rpc_call_type {
	RPC_PRECALL,
	RPC_POSTCALL
};

enum rpc_type {
	RPC_VOID,
	RPC_PTR,
	RPC_INT,
	RPC_UINT,
	RPC_STR
};

enum rpc_inout {
	RPC_INPARAM,
	RPC_OUTPARAM,
	RPC_INOUTPARAM
};

union rpc_value {
	void *ptr;
	int sint;
	size_t size_t;
	ssize_t ssize_t;
	const char *str;
	pid_t pid_t;
};

struct rpc_call_header {
	enum rpc_call_type call_type;
	enum rpc_function_id function_id;
};

struct rpc_redirect_header {
	int redirect;
	int complete;
};

void rpc_send(struct msghdr *msg);
void rpc_recv(struct msghdr *msg);

#endif

