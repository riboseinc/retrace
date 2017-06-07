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
#else
#define RETRACE_IMPLEMENTATION(func) func
#define RETRACE_REPLACE(func)
#endif

typedef enum {
    rtr_accept,
    rtr_atoi,
    rtr_bind,
    rtr_chmod,
    rtr_close,
    rtr_closedir,
    rtr_connect,
    rtr_ctime,
    rtr_ctime_r,
    rtr_dup,
    rtr_dup2,
    rtr_execve,
    rtr_exit,
    rtr_fchmod,
    rtr_fclose,
    rtr_fileno,
    rtr_fopen,
    rtr_fork,
    rtr_free,
    rtr_fseek,
    rtr_getegid,
    rtr_getenv,
    rtr_geteuid,
    rtr_getgid,
    rtr_getpid,
    rtr_getppid,
    rtr_getuid,
    rtr_malloc,
    rtr_opendir,
    rtr_pclose,
    rtr_perror,
    rtr_pipe,
    rtr_pipe2,
    rtr_popen,
    rtr_putc,
    rtr_putenv,
    rtr_read,
    rtr_seteuid,
    rtr_setgid,
    rtr_setuid,
    rtr_stat,
    rtr_strcat,
    rtr_strcmp,
    rtr_strcpy,
    rtr_strlen,
    rtr_strncat,
    rtr_strncmp,
    rtr_strncpy,
    rtr_strstr,
    rtr_system,
    rtr_tolower,
    rtr_toupper,
    rtr_unsetenv,
    rtr_write,

    rtr_end_func
} rtr_func_id;

void trace_printf(int hdr, char *buf, ...);
void trace_printf_str(const char *string);

int get_redirect(const char *function, ...);

int get_tracing_enabled();
int set_tracing_enabled(int enabled);

void *rtr_dlsym(rtr_func_id id);

#endif /* __RETRACE_COMMON_H__ */
