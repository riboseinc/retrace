#ifndef __RTR_NETFUZZ_H__
#define __RTR_NETFUZZ_H__

/* index of networking functions */
enum RTR_NET_FUNC_ID {
	NET_FUNC_ID_SOCKET = 0,

	NET_FUNC_ID_CONNECT,

	NET_FUNC_ID_BIND,
	NET_FUNC_ID_LISTEN,
	NET_FUNC_ID_ACCEPT,

	NET_FUNC_ID_SEND,
	NET_FUNC_ID_RECV,
	NET_FUNC_ID_SENDTO,
	NET_FUNC_ID_RECVFROM,
	NET_FUNC_ID_SENDMSG,
	NET_FUNC_ID_RECVMSG,

	NET_FUNC_ID_GETHOSTNAME,
	NET_FUNC_ID_GETHOSTADDR,
	NET_FUNC_ID_GETADDRINFO,

	NET_FUNC_ID_MAX
};

/* networking fuzzing types */
#define NET_FUZZ_TYPE_NOMEM		0x0001
#define NET_FUZZ_TYPE_LIMIT		0x0002
#define NET_FUZZ_TYPE_INUSE		0x0004
#define NET_FUZZ_TYPE_UNREACH		0x0008
#define NET_FUZZ_TYPE_TIMEOUT		0x0010
#define NET_FUZZ_TYPE_RESET		0x0020
#define NET_FUZZ_TYPE_REFUSE		0x0040
#define NET_FUZZ_TYPE_HOST_NOT_FOUND	0x0080
#define NET_FUZZ_TYPE_TRY_AGAIN		0x0100
#define NET_FUZZ_TYPE_END		0x0000

/* network fuzzing structure */
typedef struct _rtr_netfuzz_config {
	int init_flag;
	int fuzz_types[NET_FUNC_ID_MAX];
	int fuzz_err[NET_FUNC_ID_MAX];
	double fuzz_rates[NET_FUNC_ID_MAX];
} rtr_netfuzz_config_t;

/* fuzzing statistics */
typedef struct _rtr_netfuzz_stats {
	unsigned int error_fuzz[NET_FUNC_ID_MAX];
} rtr_netfuzz_stats_t;

int rtr_get_net_fuzzing(enum RTR_NET_FUNC_ID func_id, int *err_val);

#endif /* __RTR_NETFUZZ_H__ */
