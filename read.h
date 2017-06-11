#ifndef __RETRACE_READ_H__
#define __RETRACE_READ_H__

#include <unistd.h>

typedef ssize_t (*rtr_read_t)(int fd, void *buf, size_t nbytes);

RETRACE_DECL(read);

#endif /* __RETRACE_READ_H__ */
