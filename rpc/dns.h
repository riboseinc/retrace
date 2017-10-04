#ifndef __DNS_H__
#define __DNS_H__

struct dns_sock_info {
	SLIST_ENTRY(dns_sock_info) next;
	int fd;
	int pid;
	int domain;
	int type;
	struct sockaddr_storage addr;
};

SLIST_HEAD(dns_sock_infos, dns_sock_info);

struct dns_info {
	struct dns_sock_infos sock_infos;
};

void init_dns_handlers(struct retrace_handle *handle);

#endif
