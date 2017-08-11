#ifndef __RETRACE_FORK_H__
#define __RETRACE_FORK_H__

#include <unistd.h>

typedef pid_t (*rtr_fork_t)(void);

RETRACE_DECL(fork);

#endif
