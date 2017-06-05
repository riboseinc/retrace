#include "common.h"
#include "pipe.h"

int
pipe(int pipefd[2])
{
    int ret;

    real_pipe = dlsym(RTLD_NEXT, "pipe");
    ret = real_pipe(pipefd);
    trace_printf(1, "pipe(%p); [%d]\n", (void *) pipefd, ret);

    return ret;
}

int
pipe2(int pipefd[2], int flags)
{
    int ret;

    real_pipe2 = dlsym(RTLD_NEXT, "pipe2");
    ret = real_pipe2(pipefd, flags);
    trace_printf(1, "pipe2(%p, %d); [%d]\n", (void *) pipefd, flags, ret);

    return ret;
}
