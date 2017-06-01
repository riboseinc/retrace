#include <sys/types.h>

#ifndef __socklen_t_defined
typedef __socklen_t socklen_t;
# define __socklen_t_defined
#endif

#include <bits/sockaddr.h>

struct sockaddr
  {
    __SOCKADDR_COMMON (sa_);
    char sa_data[14];
  };

#if ULONG_MAX > 0xffffffff
# define __ss_aligntype __uint64_t
#else
# define __ss_aligntype __uint32_t
#endif

struct msghdr
  {
    void *msg_name;
    socklen_t msg_namelen;

    struct iovec *msg_iov;
    size_t msg_iovlen;

    void *msg_control;
    size_t msg_controllen;

    int msg_flags;
  };

extern struct cmsghdr *__cmsg_nxthdr (struct msghdr *__mhdr,
	struct cmsghdr *__cmsg) __THROW;

static int (*real_atoi)(const char *str);
static int (*real_bind)(int fd, const struct sockaddr *address, socklen_t len);
static int (*real_connect)(int fd, const struct sockaddr *address, socklen_t len);
