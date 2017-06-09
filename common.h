#ifndef __RETRACE_COMMON_H__
#define __RETRACE_COMMON_H__

#include <sys/stat.h>

#include <dlfcn.h>
#include <dirent.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define SOMAXCONN	128
#define MAXLEN		40

#define VAR "\e[33m"  /* ANSI yellow for variable values */
#define INF "\e[31m"  /* ANSI red for information notices */
#define RST "\e[0m"   /* ANSI white */
#define BEG "\e[100D" /* ANSI goto 1st char */

#define ARGUMENT_TYPE_END (int) 0
#define ARGUMENT_TYPE_INT (int) 1
#define ARGUMENT_TYPE_STRING (int) 2

#ifdef __APPLE__

#define DYLD_INTERPOSE(_replacment,_replacee) \
__attribute__((used)) static struct{ const void* replacment; const void* replacee; } _interpose_##_replacee \
__attribute__ ((section ("__DATA,__interpose"))) = { (const void*)(unsigned long)&_replacment, (const void*)(unsigned long)&_replacee };
#define RETRACE_IMPLEMENTATION(func) retrace_impl_##func
#define RETRACE_REPLACE(func) DYLD_INTERPOSE(retrace_impl_##func, func)
#define RETRACE_GET_REAL(func) func
#else
#define RETRACE_IMPLEMENTATION(func) func
#define RETRACE_REPLACE(func)
#define RETRACE_GET_REAL(func) dlsym(RTLD_NEXT, #func)
#endif



void trace_printf(int hdr, char *buf, ...);
void trace_printf_str(const char *string);
void trace_dump_data(const void *buf, size_t nbytes);

int get_redirect(const char *function, ...);

int get_tracing_enabled();
int set_tracing_enabled(int enabled);

#endif /* __RETRACE_COMMON_H__ */
