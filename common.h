#ifndef __RETRACE_COMMON_H__
#define __RETRACE_COMMON_H__

#include <sys/types.h>
#include <sys/stat.h>

#include <ctype.h>
#include <dlfcn.h>
#include <dirent.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SOMAXCONN 128
#define MAXLEN 40

#define VAR "\e[33m"  /* ANSI yellow for variable values */
#define INF "\e[31m"  /* ANSI red for information notices */
#define RST "\e[0m"   /* ANSI white */
#define BEG "\e[100D" /* ANSI goto 1st char */

#define ARGUMENT_TYPE_END (int) 0
#define ARGUMENT_TYPE_INT (int) 1
#define ARGUMENT_TYPE_STRING (int) 2

void trace_printf(int hdr, char *buf, ...);
void trace_printf_str(const char *string);

int get_redirect(const char *function, ...);

int get_tracing_enabled();
int set_tracing_enabled(int enabled);

#endif /* __RETRACE_COMMON_H__ */
