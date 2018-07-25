#ifndef __RETRACE_READ_H__
#define __RETRACE_READ_H__

#include <unistd.h>

typedef ssize_t (*rtr_read_t)(int fd, void *buf, size_t nbytes);
typedef ssize_t (*rtr_readv_t)(int fd, const struct iovec *iov, int iovcnt);

RETRACE_DECL(read);
RETRACE_DECL(readv);

#endif /* __RETRACE_READ_H__ */
