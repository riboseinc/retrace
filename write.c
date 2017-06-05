#include "common.h"
#include "write.h"

ssize_t
write(int fd, const void *buf, size_t nbytes)
{
    ssize_t ret;

    real_write = dlsym(RTLD_NEXT, "write");
    ret = real_write(fd, buf, nbytes);
    trace_printf(1, "write(%d, %p, %d); [%d]\n", fd, buf, nbytes, ret);

    return ret;
}
