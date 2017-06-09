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

#define FILE_DESCRIPTOR_TYPE_UNKNOW		0
#define FILE_DESCRIPTOR_TYPE_FILE		1 // from open()
#define FILE_DESCRIPTOR_TYPE_IPV4_CONNECT	2 // from connect()
#define FILE_DESCRIPTOR_TYPE_IPV4_BIND		3 // from bind()
#define FILE_DESCRIPTOR_TYPE_IPV4_ACCEPT	4 // from accept()

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

typedef struct {
	int fd;
	unsigned int type;
	char *location; // File name or address
	int port;
} descriptor_info_t;


void trace_printf(int hdr, const char *fmt, ...);
void trace_printf_str(const char *string);
void trace_dump_data(const void *buf, size_t nbytes);
void trace_mode(mode_t mode, char *p);

int get_redirect(const char *function, ...);

int get_tracing_enabled();
int set_tracing_enabled(int enabled);


/* Descriptor tracking */
void file_descriptor_update(int fd, unsigned int type, char *location, int port);
descriptor_info_t *file_descriptor_get (int fd);
void file_descriptor_remove (int fd);


#endif /* __RETRACE_COMMON_H__ */
