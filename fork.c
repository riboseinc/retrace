#include "common.h"
#include "fork.h"

pid_t
fork(void)
{
    pid_t p;

    real_fork = dlsym(RTLD_NEXT, "fork");
    p = real_fork();
    trace_printf(1, "fork(); [%d]\n", p);

    return p;
}
