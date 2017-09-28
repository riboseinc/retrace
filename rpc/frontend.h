#ifndef __RETRACE_FRONTEND_H__
#define __RETRACE_FRONTEND_H__

#include <sys/types.h>
#include <sys/queue.h>

#include "rpc.h"
#include "shim.h"

#define RETRACE_TRACE 0x01

struct retrace_call_context {
	SLIST_ENTRY(retrace_call_context) next;
	enum retrace_function_id function_id;
	char params[128];
	char result[16];
	int _errno;
	void *user_data;
};

SLIST_HEAD(retrace_call_stack, retrace_call_context);

struct retrace_endpoint {
	SLIST_ENTRY(retrace_endpoint) next;
	int fd;
	pid_t pid;
	int thread_num;
	unsigned int call_num;
	unsigned int call_depth;
	struct retrace_call_stack call_stack;
	struct retrace_handle *handle;
};

SLIST_HEAD(retrace_endpoints, retrace_endpoint);

struct retrace_process_info {
	SLIST_ENTRY(retrace_process_info) next;
	pid_t pid;
	int next_thread_num;
};

SLIST_HEAD(process_list, retrace_process_info);

typedef int (*retrace_precall_handler_t)(struct retrace_endpoint *ep, struct retrace_call_context *context);
typedef void (*retrace_postcall_handler_t)(struct retrace_endpoint *ep, struct retrace_call_context *context);

struct retrace_precall_handler {
	SLIST_ENTRY(retrace_precall_handler) next;
	retrace_precall_handler_t fn;
};

SLIST_HEAD(retrace_precall_handlers, retrace_precall_handler);

struct retrace_postcall_handler {
	SLIST_ENTRY(retrace_postcall_handler) next;
	retrace_postcall_handler_t fn;
};

SLIST_HEAD(retrace_postcall_handlers, retrace_postcall_handler);

struct retrace_handle {
	struct retrace_endpoints endpoints;
	struct process_list processes;
	int control_fd;
	struct retrace_precall_handlers precall_handlers[RPC_FUNCTION_COUNT];
	struct retrace_postcall_handlers postcall_handlers[RPC_FUNCTION_COUNT];
	void *user_data;
};

struct retrace_handle *retrace_start(char *const argv[], const int *trace_flags);
void retrace_close(struct retrace_handle *handle);
void retrace_trace(struct retrace_handle *handle);
void retrace_handle_call(const struct retrace_endpoint *ep);
void retrace_add_precall_handler(struct retrace_handle *handle,
	enum retrace_function_id fid, retrace_precall_handler_t fn);
void retrace_add_postcall_handler(struct retrace_handle *handle,
	enum retrace_function_id fid, retrace_postcall_handler_t fn);
void retrace_set_user_data(struct retrace_handle *handle, void *data);
int retrace_fetch_backtrace(int fd, int depth, char *buf, size_t len);
int retrace_fetch_memory(int fd, const void *address, void *buffer, size_t len);
int retrace_fetch_string(int fd, const char *address, char *buffer, size_t len);
int retrace_fetch_fileno(int fd, FILE *s, int *result);
enum retrace_function_id retrace_function_id(const char *name);
const char *retrace_function_name(enum retrace_function_id id);

int rpc_send_message(int fd, enum rpc_msg_type msg_type, const void *buf, size_t len);
int rpc_send(int fd, const void *buf, size_t len);
ssize_t rpc_recv_message(int fd, enum rpc_msg_type *msg_type, void *buf);
int rpc_recv(int fd, void *buf, size_t len);
int rpc_recv_string(int fd, char *buffer, size_t len);

#endif
