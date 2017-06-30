#ifndef __RETRACE_COMMON_H__
#define __RETRACE_COMMON_H__

#include "config.h"
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
#define FILE_DESCRIPTOR_TYPE_UNIX_BIND		8 /* bind AF_UNIX */

#define PARAMETER_TYPE_END		0
#define PARAMETER_TYPE_INT		1 /* int */
#define PARAMETER_TYPE_POINTER		2 /* opaque pointer, we will print the address */
#define PARAMETER_TYPE_UINT		3 /* unsigned int */
#define PARAMETER_TYPE_FLOAT		4 /* float */
#define PARAMETER_TYPE_DOUBLE		5 /* double */
#define PARAMETER_TYPE_STRING		6 /* Zero terminated string */
#define PARAMETER_TYPE_STRING_LEN	7 /* Non zero terminated string, the length of the string precedes the string */
#define PARAMETER_TYPE_MEMORY_BUFFER	8 /* A pointer to a memory buffer, the length of the string precedes the string */
#define PARAMETER_TYPE_CHAR		9 /* char */
#define PARAMETER_TYPE_FILE_STREAM	10 /* FILE* */
#define PARAMETER_TYPE_FILE_DESCRIPTOR	11 /* int used as a file descriptor */
#define PARAMETER_TYPE_DIR		12 /* DIR */
#define PARAMETER_TYPE_INT_OCTAL	13 /* int, print as octal */
#define PARAMETER_TYPE_MEM_BUFFER_ARRAY	14 /* first size of buffers, then number of buffers, memory pointer at the end */
#define PARAMETER_TYPE_PRINTF_FORMAT	15 /* printf format string followed by pointer to a va_list */
#define PARAMETER_TYPE_STRING_ARRAY	16 /* fist the size of the array, then an array of char * */
#define PARAMETER_TYPE_IOVEC		17 /* number of buffers, then array of struct iovec */
#define PARAMETER_TYPE_UTSNAME		18 /* struct utsname*  */
#define PARAMETER_TYPE_TIMEVAL		19 /* struct timeval* */
#define PARAMETER_TYPE_TIMEZONE		20 /* struct timezone* */
#define PARAMETER_TYPE_SSL		21 /* OpenSSL's SSL* */
#define PARAMETER_TYPE_SSL_WITH_KEY	22 /* OpenSSL's SSL* but attempt to dump the ssl key */


#define PARAMETER_FLAG_OUTPUT_VARIABLE		0x40000000 /* This is an output variable, is uninitialized in EVENT_TYPE_BEFORE_CALL */
#define PARAMETER_FLAG_STRING_NEXT		0x80000000 /* There's a string parameter that describes the string */
#define PARAMETER_FLAGS_ALL (PARAMETER_FLAG_STRING_NEXT | PARAMETER_FLAG_OUTPUT_VARIABLE)

#define EVENT_TYPE_BEFORE_CALL		0
#define EVENT_TYPE_AFTER_CALL		1

#define GET_PARAMETER_TYPE(param) (param & ~PARAMETER_FLAGS_ALL)
#define GET_PARAMETER_FLAGS(param) (param & PARAMETER_FLAGS_ALL)

struct rtr_event_info {
	unsigned int event_type;

	char *function_name;

	unsigned int *parameter_types;
	void **parameter_values;

	unsigned int return_value_type;
	void *return_value;
};

#define RETRACE_DECL(func) extern rtr_##func##_t real_##func

#ifdef __APPLE__

#define DYLD_INTERPOSE(_replacment, _replacee)						\
static struct {										\
	const void *replacment;								\
	const void *replacee;								\
} _interpose_##_replacee __attribute__((used, section("__DATA,__interpose"))) = {	\
	(const void *)(unsigned long)&_replacment,					\
	(const void *)(unsigned long)&_replacee						\
};
#define RETRACE_IMPLEMENTATION(func) retrace_impl_##func

#define RETRACE_REPLACE(func, type, defn, args)				\
DYLD_INTERPOSE(retrace_impl_##func, func)				\
rtr_##func##_t real_##func = func;

#define RETRACE_REPLACE_V(func, type, defn, last, vfunc, vargs)		\
DYLD_INTERPOSE(retrace_impl_##func, func)				\
rtr_##func##_t real_##func = func;

#else /* !__APPLE__ */

#define RETRACE_IMPLEMENTATION(func) (func)

#ifdef __OpenBSD__

#define RETRACE_FIXUP(func) real_##func = dlsym(RTLD_NEXT, #func)

#else /* !__OpenBSD */

__attribute__((regparm (3))) extern void *_dl_sym(void *handle, const char *symbol, const void *rtraddr);

#ifdef HAVE_ATOMIC_BUILTINS

#define RETRACE_FIXUP(func) __atomic_store_n(&real_##func,		\
	_dl_sym(RTLD_NEXT, #func, rtr_fixup_##func), __ATOMIC_RELAXED)	\

#else /* !HAVE_ATOMIC_BUILTINS */

#define RETRACE_FIXUP(func) real_##func =				\
	_dl_sym(RTLD_NEXT, #func, __func__)				\

#endif /* HAVE_ATOMIC_BUILTINS */

#endif /* __OpenBSD__ */

#define RETRACE_REPLACE(func, type, defn, args)				\
type rtr_fixup_##func defn {						\
	RETRACE_FIXUP(func);						\
	return real_##func args;					\
}									\
rtr_##func##_t real_##func = rtr_fixup_##func;

#define RETRACE_REPLACE_V(func, type, defn, last, vfunc, vargs)		\
type rtr_fixup_##func defn {						\
	RETRACE_FIXUP(func);						\
	va_list ap;							\
	va_start(ap, last);						\
	type ret = vfunc vargs;						\
	va_end(ap);							\
	return (ret);							\
}									\
rtr_##func##_t real_##func = rtr_fixup_##func;

#endif /* !__APPLE__ */

struct descriptor_info {
	int fd;
	unsigned int type;
	char *location; /* File name or address */
	int port;
};

void retrace_log_and_redirect_before(struct rtr_event_info *event_info);
void retrace_log_and_redirect_after(struct rtr_event_info *event_info);


void trace_printf(int hdr, const char *fmt, ...);
void trace_printf_str(const char *string);
void trace_dump_data(const unsigned char *buf, size_t nbytes);
void trace_mode(mode_t mode, char *p);

typedef const void *RTR_CONFIG_HANDLE;

int rtr_get_config_multiple(RTR_CONFIG_HANDLE *config, const char *function, ...);
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
