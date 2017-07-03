#ifndef __RTR_INSPECT_H__
#define __RTR_INSPECT_H__

#include "shim.h"
#include "rpc.h"

struct rtr_arg_info {
	const char *name;
	const char *ctype;
	enum rpc_type rpctype;
	void *value;
};

struct rtr_call_info {
	const char *name;
	const char *ctype;
	enum rpc_type rpctype;
	void *result;
	struct rtr_arg_info *args;
};

struct rtr_call_info *rtr_get_call_info(enum rpc_function_id, void *call);
#endif
