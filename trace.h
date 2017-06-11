#ifndef __RETRACE_EXIT_H__
#define __RETRACE_EXIT_H__

#include <unistd.h>
#include <sys/types.h>
#include <sys/ptrace.h>


typedef int (*rtr_ptrace_t)(int request, pid_t pid, caddr_t addr, int data);

RETRACE_DECL(ptrace);

#endif /* __RETRACE_EXIT_H__ */

