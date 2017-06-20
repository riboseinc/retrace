#ifndef __RETRACE_COMMON_H__
#define __RETRACE_COMMON_H__

#include <sys/stat.h>
#include <sys/utsname.h>
#include <sys/time.h>

#include <dlfcn.h>
#include <dirent.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define MAXLEN		40

#define VAR "\033[33m"  /* ANSI yellow for variable values */
#define INF "\033[31m"  /* ANSI red for information notices */
#define RST "\033[0m"   /* ANSI white */
#define BEG "\033[100D" /* ANSI goto 1st char */

#define ARGUMENT_TYPE_END	0
#define ARGUMENT_TYPE_INT	1
#define ARGUMENT_TYPE_STRING	2
#define ARGUMENT_TYPE_DOUBLE	3

#define FILE_DESCRIPTOR_TYPE_UNKNOWN		0
#define FILE_DESCRIPTOR_TYPE_FILE		1 /* from open() */
#define FILE_DESCRIPTOR_TYPE_IPV4_CONNECT	2 /* from connect() using AF_INET */
#define FILE_DESCRIPTOR_TYPE_IPV4_BIND		3 /* from bind() */
#define FILE_DESCRIPTOR_TYPE_IPV4_ACCEPT	4 /* from accept() */
#define FILE_DESCRIPTOR_TYPE_UNIX_DOMAIN	5 /* from connect() using AF_UNIX */
#define FILE_DESCRIPTOR_TYPE_UDP_SENDTO		6 /* from sendto() over UDP */
#define FILE_DESCRIPTOR_TYPE_UDP_SENDMSG	7 /* from sendmsg() over UDP local socket */

extern void *_dl_sym(void *handle, const char *symbol, const void *rtraddr);

#ifdef __APPLE__

#define DYLD_INTERPOSE(_replacment, _replacee)						\
static struct {										\
	const void *replacment;								\
	const void *replacee;								\
} _interpose_##_replacee __attribute__((used, section("__DATA,__interpose"))) = {	\
	(const void *)(unsigned long)&_replacment,					\
	(const void *)(unsigned long)&_replacee						\
};
#define RETRACE_DECL(func)
#define RETRACE_IMPLEMENTATION(func) retrace_impl_##func
#define RETRACE_REPLACE(func) DYLD_INTERPOSE(retrace_impl_##func, func)
#define RETRACE_GET_REAL(func) func

#else /* !__APPLE__ */

#define RETRACE_DECL(func) rtr_##func##_t rtr_get_real_##func()
#define RETRACE_IMPLEMENTATION(func) (func)
#ifdef __OpenBSD__
#define RETRACE_REPLACE(func)                                              \
rtr_##func##_t rtr_get_real_##func() {                                     \
	static rtr_##func##_t ptr = (rtr_##func##_t) NULL;                 \
	if (ptr == NULL)                                                   \
		*(void **) (&ptr) = dlsym(RTLD_NEXT, #func);               \
	return ptr;                                                        \
}
#else
#define RETRACE_REPLACE(func)                                                                  \
rtr_##func##_t rtr_get_real_##func() {                                                         \
	static rtr_##func##_t ptr;                                                             \
	if (__atomic_load_n(&ptr, __ATOMIC_RELAXED) == NULL)                                   \
		__atomic_store_n(&ptr, _dl_sym(RTLD_NEXT, #func, __func__), __ATOMIC_RELAXED); \
	return ptr;                                                                            \
}
#endif
#define RETRACE_GET_REAL(func) rtr_get_real_##func()

#endif /* !__APPLE__ */

struct descriptor_info {
	int fd;
	unsigned int type;
	char *location; /* File name or address */
	int port;
};

void trace_printf(int hdr, const char *fmt, ...);
void trace_printf_str(const char *string);
void trace_dump_data(const unsigned char *buf, size_t nbytes);
void trace_mode(mode_t mode, char *p);

int rtr_get_config_multiple(FILE **config, const char *function, ...);
int rtr_get_config_single(const char *function, ...);
void rtr_config_close(FILE *config);

int get_tracing_enabled(void);
int trace_disable();
void trace_restore(int oldstate);

/* Descriptor tracking */
void file_descriptor_update(int fd, unsigned int type, const char *location, int port);
struct descriptor_info *file_descriptor_get(int fd);
void file_descriptor_remove(int fd);

#endif /* __RETRACE_COMMON_H__ */
