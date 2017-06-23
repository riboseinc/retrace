#ifndef __RETRACE_NETDB_H__
#define __RETRACE_NETDB_H__

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

typedef struct hostent *(*rtr_gethostbyname_t)(const char *name);
typedef struct hostent *(*rtr_gethostbyaddr_t)(const void *addr, socklen_t len, int type);

typedef void (*rtr_sethostent_t)(int stayopen);
typedef void (*rtr_endhostent_t)(void);

typedef struct hostent *(*rtr_gethostent_t)(void);
typedef struct hostent *(*rtr_gethostbyname2_t)(const char *name, int af);

typedef int (*rtr_getaddrinfo_t)(const char *node, const char *service,
	const struct addrinfo *hints, struct addrinfo **res);
typedef void (*rtr_freeaddrinfo_t)(struct addrinfo *res);

RETRACE_DECL(gethostbyname);
RETRACE_DECL(gethostbyaddr);
RETRACE_DECL(sethostent);
RETRACE_DECL(endhostent);
RETRACE_DECL(gethostent);
RETRACE_DECL(gethostbyname2);

RETRACE_DECL(getaddrinfo);
RETRACE_DECL(freeaddrinfo);

#endif /* __RETRACE_NETDB_H__ */
