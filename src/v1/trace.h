#ifndef __RETRACE_TRACE_H__
#define __RETRACE_TRACE_H__

#include <unistd.h>
#include <sys/types.h>
#include <sys/ptrace.h>

#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__)
typedef int (*rtr_ptrace_t)(int request, pid_t pid, caddr_t addr, int data);
#elif defined(__NetBSD__)
typedef int (*rtr_ptrace_t)(int request, pid_t pid, void *addr, int data);
#else
typedef long int (*rtr_ptrace_t)(enum __ptrace_request request, ...);
#endif

RETRACE_DECL(ptrace);

#endif /* __RETRACE_TRACE_H__ */

