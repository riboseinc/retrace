/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "common.h"
#include <sys/types.h>

#include <pwd.h>
#define _WITH_GETLINE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#ifdef __linux__
#include <syscall.h>
#endif
#if defined(__FreeBSD__) || defined(__OpenBSD__)
#include <pthread_np.h>
#endif

#ifdef __NetBSD__
#include <lwp.h>
#endif

#include <stdarg.h>
#include <errno.h>
#include <sys/queue.h>
#include <dirent.h>
#include <sys/uio.h>
#include <sys/utsname.h>
#include <execinfo.h>
#include <fcntl.h>

#include "str.h"
#include "id.h"
#include "file.h"
#include "malloc.h"
#include "printf.h"
#include "env.h"
#include "dir.h"
#include "ssl.h"

/*
 * Global setting, we set this to disable all our
 * internal tracing when we are doing some internal
 * operations like reading config files, so we don't
 * confuse the program with calls that we ourselves
 * introduced. Default to enable
 *
 * This also fixes some looping issues. Image we are
 * faking a call to getuid, this calls get_redirect to
 * check if theres a redirect active. get_redirect calls
 * fopen, which in its internal implementation also
 * calls into your tapped version of getuid and we
 * get into an infinite loop.
 *
 * g_enable_tracing is used only for the main thread,
 * if we see ourselves being called from a different thread
 * we will initialized the key g_enable_tracing_key
 * and will use that to store a per-thread tracing
 * enabled/disabled setting. I used to declare
 * g_enable_tracing with the __thread modifier, but in
 * macOS that will eventually call malloc which would
 * get us into a loop.
 *
 **************************************************/
#define RTR_TRACE_UNKNOW 0
#define RTR_TRACE_ENABLED 1
#define RTR_TRACE_DISABLED 2

static int g_enable_tracing = RTR_TRACE_ENABLED;
static pthread_key_t g_enable_tracing_key = -1;
static pthread_once_t tracing_key_once = PTHREAD_ONCE_INIT;

static int g_init_rand;
static unsigned int g_rand_seed;

/*
 * Global list of file descriptors so we can track
 * their usage across different functions
 *
 */
#define DESCRIPTOR_LIST_INITIAL_SIZE 8
struct descriptor_info **g_descriptor_list;
unsigned int g_descriptor_list_size;

static pthread_mutex_t printing_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t logfile_lock = PTHREAD_MUTEX_INITIALIZER;
static int is_main_thread(void);
static void trace_set_color(char *color);


static void **
retrace_print_parameter(unsigned int event_type, unsigned int type, int flags, void **value)
{
	trace_set_color(VAR);

	switch (type) {
	case PARAMETER_TYPE_INT:
		trace_printf(0, "%d", (*(int *) *value));
		break;
	case PARAMETER_TYPE_POINTER:
		trace_printf(0, "%p", (*(void **) *value));
		break;
	case PARAMETER_TYPE_UINT:
		trace_printf(0, "%u", *((unsigned int *) *value));
		break;
	case PARAMETER_TYPE_LONG:
		trace_printf(0, "%ld", *((long *) *value));
		break;
	case PARAMETER_TYPE_ULONG:
		trace_printf(0, "%lu", *((unsigned long *) *value));
		break;
	case PARAMETER_TYPE_FLOAT:
		trace_printf(0, "%f", *((float *) *value));
		break;
	case PARAMETER_TYPE_DOUBLE:
		trace_printf(0, "%f", *((double *) *value));
		break;
	case PARAMETER_TYPE_STRING:

		if (event_type == EVENT_TYPE_BEFORE_CALL && flags & PARAMETER_FLAG_OUTPUT_VARIABLE) {
			trace_printf(0, "%p", (*(void **) *value));
		} else {
			if ((*(char **) *value) != NULL) {
				trace_printf(0, "\"");
				trace_printf_str((*(char **) *value), -1);
				trace_printf(0, "\"");
			} else
				trace_printf(0, "(nil)");
		}
		break;
	case PARAMETER_TYPE_STRING_LEN:
	{
		int len;

		len = (*(int *) *value);
		value++;

		trace_printf(0, "\"");
		trace_printf_str((*(char **) *value), len);
		trace_printf(0, "\"");

		break;
	}
	case PARAMETER_TYPE_MEMORY_BUFFER:
		value++;

		trace_printf(0, "%p", (*(void **) *value));
		break;
	case PARAMETER_TYPE_MEM_BUFFER_ARRAY:
		value += 2;
		trace_printf(0, "%p", (*(void **) *value));
		break;
	case PARAMETER_TYPE_CHAR:
		trace_printf(0, "'%c'(%d)", (*(char **) *value), *((int *) *value));
		break;
	case PARAMETER_TYPE_DIR:
	{
		int fd = -1;
		DIR *dirp;

		dirp = *((DIR **) *value);

		if (dirp)
			fd = real_dirfd(dirp);

		trace_printf(0, "%p", dirfd);
		trace_set_color(INF);
		trace_printf(0, " [fd %d]", fd);
		trace_set_color(VAR);

		if (fd > 0) {
			struct descriptor_info *di;

			di = file_descriptor_get(fd);

			if (di && di->location) {
				trace_set_color(INF);
				trace_printf(0, " [%s]", di->location);
				trace_set_color(VAR);
			}
		}

		break;
	}
	case PARAMETER_TYPE_FILE_STREAM:
	{
		int fd = -1;
		FILE *stream;
		struct descriptor_info *di;

		stream = *((FILE **) *value);

		if (stream)
			fd = real_fileno(stream);

		trace_printf(0, "%p", stream);
		trace_set_color(INF);
		trace_printf(0, " [fd %d]", fd);
		trace_set_color(VAR);

		if (fd > 0) {
			di = file_descriptor_get(fd);
			if (di && di->location) {
				trace_set_color(INF);
				trace_printf(0, " [%s]", di->location);
				trace_set_color(VAR);
			}
		}

		break;
	}
	case PARAMETER_TYPE_FILE_DESCRIPTOR:
	{
		int fd = *((int *) *value);
		struct descriptor_info *di;

		trace_printf(0, "%d", fd);

		if (event_type != EVENT_TYPE_BEFORE_CALL || (flags & PARAMETER_FLAG_OUTPUT_VARIABLE)) {
			di = file_descriptor_get(fd);
			if (di && di->location) {
				trace_set_color(INF);
				trace_printf(0, " [%s]", di->location);
				trace_set_color(VAR);
			}
		}


		break;
	}
	case PARAMETER_TYPE_INT_OCTAL:
		trace_printf(0, "%o", *((int *) *value));
		break;
	case PARAMETER_TYPE_PRINTF_FORMAT:
	{
		char *fmt;
		va_list *ap;
		char buf[1024];
		int old_trace_state;

		fmt = *((char **) *value);
		value++;
		ap = (va_list *) *value;

		old_trace_state = trace_disable();
		real_vsnprintf(buf, 1024, fmt, *ap);
		trace_restore(old_trace_state);

		trace_printf(0, "\"");
		trace_printf_str(fmt, -1);
		trace_printf(0, "\" -> \"");
		trace_printf_str(buf, -1);
		trace_printf(0, "\"");

		break;
	}
	case PARAMETER_TYPE_STRING_ARRAY:
	{
		char **argv;

		argv = *((char ***) *value);

		while (*argv) {
			trace_printf_str(*argv, -1);
			trace_printf(0, ", ");

			argv++;
		}
		trace_printf(0, "NULL");
		break;

	}
	case PARAMETER_TYPE_IOVEC:
	{
		value++;
		break;
	}
	case PARAMETER_TYPE_UTSNAME:
	{
		struct utsname *buf;

		buf = *((struct utsname **) *value);

		trace_printf(0, "%p [%s, %s, %s, %s, %s]", buf, buf->sysname, buf->nodename,
				buf->release, buf->version, buf->machine);
		break;

	}
	case PARAMETER_TYPE_TIMEVAL:
	{
		struct timeval *tv;
		time_t tv_sec;
		suseconds_t tv_usec;

		tv = *((struct timeval **) *value);

		trace_printf(1, "%p", tv);

		if (tv) {
			tv_sec  = tv->tv_sec;
			tv_usec = tv->tv_usec;

			trace_printf(1, "[%ld, %ld]", tv_sec, tv_usec);
		}
		break;
	}
	case PARAMETER_TYPE_TIMEZONE:
	{
		struct timezone *tz;
		int tz_minuteswest = 0;
		int tz_dsttime = 0;

		tz = *((struct timezone **) *value);

		trace_printf(0, "%p", tz);
		if (tz != NULL) {
			tz_minuteswest	= tz->tz_minuteswest;
			tz_dsttime	= tz->tz_dsttime;

			trace_printf(0, "[%d, %d]", tz_minuteswest, tz_dsttime);
		}

		break;
	}
	case PARAMETER_TYPE_SSL:
	case PARAMETER_TYPE_SSL_WITH_KEY:
		trace_printf(0, "%p", (*(void **) *value));
		break;

#if HAVE_STRUCT_FLOCK
	case PARAMETER_TYPE_STRUCT_FLOCK:
		trace_printf(1, "struct flock {\n");
		trace_printf(1, "\tl_start = %zu\n", (*(struct flock **) *value)->l_start);
		trace_printf(1, "\tl_len = %zu\n", (*(struct flock **) *value)->l_len);
		trace_printf(1, "\tl_pid = %d\n", (*(struct flock **) *value)->l_pid);
		trace_printf(1, "\tl_type = %d\n", (*(struct flock **) *value)->l_type);
		trace_printf(1, "\tl_whence = %d\n", (*(struct flock **) *value)->l_whence);
		trace_printf(1, "}\n");
		break;
#endif

#if HAVE_STRUCT_FSTORE
	case PARAMETER_TYPE_STRUCT_FSTORE:
		trace_printf(1, "struct fstore {\n");
		trace_printf(1, "\tfst_flags = %zu\n", (*(struct fstore **) *value)->fst_flags);
		trace_printf(1, "\tfst_posmode = %d\n", (*(struct fstore **) *value)->fst_posmode);
		trace_printf(1, "\tfst_offset = %zu\n", (*(struct fstore **) *value)->fst_offset);
		trace_printf(1, "\tfst_length = %zu\n", (*(struct fstore **) *value)->fst_length);
		trace_printf(1, "\tfst_bytesalloc = %zu\n", (*(struct fstore **) *value)->fst_bytesalloc);
		trace_printf(1, "}\n");
		break;
#endif

#if HAVE_STRUCT_FPUNCHHOLE
	case PARAMETER_TYPE_STRUCT_FPUNCHHOLE:
		trace_printf(1, "struct fpunchhole {\n");
		trace_printf(1, "\tfp_flags = %zu\n", (*(struct fpunchhole **) *value)->fp_flags);
		trace_printf(1, "\tfp_offset = %zu\n", (*(struct fpunchhole **) *value)->fp_offset);
		trace_printf(1, "\tfp_length = %zu\n", (*(struct fpunchhole **) *value)->fp_length);
		trace_printf(1, "}\n");
		break;
#endif

#if HAVE_STRUCT_RADVISORY
	case PARAMETER_TYPE_STRUCT_RADVISORY:
		trace_printf(1, "struct radvisory {\n");
		trace_printf(1, "\tra_offset = %zu\n", (*(struct radvisory **) *value)->ra_offset);
		trace_printf(1, "\tra_count = %zu\n", (*(struct radvisory **) *value)->ra_count);
		trace_printf(1, "}\n");
		break;
#endif

#if HAVE_STRUCT_FBOOTSTRAPTRANSFER
	case PARAMETER_TYPE_STRUCT_FBOOTSTRAPTRANSFER:
		trace_printf(1, "struct fbootstraptransfer {\n");
		trace_printf(1, "\tfbt_offset = %zu\n", (*(struct fbootstraptransfer **) *value)->fbt_offset);
		trace_printf(1, "\tfbt_length = %zu\n", (*(struct fbootstraptransfer **) *value)->fbt_length);
		trace_printf(1, "\tfbt_buffer = %p\n", (*(struct fbootstraptransfer **) *value)->fbt_buffer);
		trace_printf(1, "}\n");
		break;
#endif

#if HAVE_STRUCT_LOG2PHYS
	case PARAMETER_TYPE_STRUCT_LOG2PHYS:
		trace_printf(1, "struct log2phys {\n");
		trace_printf(1, "\tl2p_flags = %zu\n", (*(struct log2phys **) *value)->l2p_flags);
		trace_printf(1, "\tl2p_contigbytes = %zu\n", (*(struct log2phys **) *value)->l2p_contigbytes);
		trace_printf(1, "\tl2p_devoffset = %zu\n", (*(struct log2phys **) *value)->l2p_devoffset);
		trace_printf(1, "}\n");
		break;
#endif

#if HAVE_DECL_F_GETOWN_EX
	case PARAMETER_TYPE_STRUCT_F_GETOWN_EX:
		trace_printf(1, "struct f_owner_ex {\n");
		trace_printf(1, "\ttype = %d\n", (*(struct f_owner_ex **) *value)->type);
		trace_printf(1, "\tpid = %zu\n", (*(struct f_owner_ex **) *value)->pid);
		trace_printf(1, "}\n");
		break;
#endif
	}

	trace_set_color(RST);

	/* There's a string following this parameter that expands its meaning */
	if ((flags & PARAMETER_FLAG_STRING_NEXT) == PARAMETER_FLAG_STRING_NEXT) {
		value++;
		trace_set_color(INF);
		trace_printf(0, " [%s]", (*(char **) *value));
		trace_set_color(RST);
	}

	return value + 1;
}

void **
retrace_dump_parameter(unsigned int type, int flags, void **value)
{
	if (type == PARAMETER_TYPE_MEMORY_BUFFER) {
		int size;

		size = *((int *) *value);
		value++;

		if (size > 0)
			trace_dump_data((*(unsigned char **) *value), size);
	} else if (type == PARAMETER_TYPE_MEM_BUFFER_ARRAY) {
		int size;
		int nmemb;
		int i;
		void *data;

		size = *((int *) *value);
		value++;
		nmemb = *((int *) *value);
		value++;
		data = *((void **) (*value));

		if (size > 0)
			for (i = 0; i < nmemb; i++)
				trace_dump_data(data + i, size);
	} else if (type == PARAMETER_TYPE_IOVEC) {
		int i;
		int size;
		struct iovec *iov;

		size = *((size_t *) *value);
		value++;
		iov = *((struct iovec **) *value);

		for (i = 0; i < size; i++) {
			struct iovec *msg_iov = &iov[i];

			if (msg_iov->iov_len > 0)
				trace_dump_data((unsigned char *) iov->iov_base, msg_iov->iov_len);
		}
	} else if (type == PARAMETER_TYPE_SSL_WITH_KEY) {
#ifdef HAVE_OPENSSL_SSL_H
		void *ssl = (*(void **) *value);

		if (ssl != NULL)
			print_ssl_keys(ssl);
#endif /* HAVE_OPENSSL_SSL */
	}

	return value + 1;
}


void
retrace_event(struct rtr_event_info *event_info)
{
	if (!get_tracing_enabled())
		return;

	pthread_mutex_lock(&printing_lock);

	if (event_info->event_type == EVENT_TYPE_AFTER_CALL || event_info->event_type == EVENT_TYPE_BEFORE_CALL) {
		unsigned int *parameter_type;
		void **parameter_value;
		int has_memory_buffers = 0;

		parameter_type = event_info->parameter_types;
		parameter_value = event_info->parameter_values;

#if 0
		if (event_info->event_type == EVENT_TYPE_BEFORE_CALL)
			trace_printf(1, "->: ", event_info->function_name);
		else if (event_info->event_type == EVENT_TYPE_AFTER_CALL)
			trace_printf(1, "<-: ", event_info->function_name);
#endif

		trace_set_color(FUNC);
		trace_printf(1, "%s", event_info->function_name);
		trace_set_color(RST);
		trace_printf(0, "(");

		while (GET_PARAMETER_TYPE(*parameter_type) != PARAMETER_TYPE_END) {

			if (GET_PARAMETER_TYPE(*parameter_type) == PARAMETER_TYPE_MEMORY_BUFFER ||
			    GET_PARAMETER_TYPE(*parameter_type) == PARAMETER_TYPE_MEM_BUFFER_ARRAY ||
			    GET_PARAMETER_TYPE(*parameter_type) == PARAMETER_TYPE_IOVEC ||
			    GET_PARAMETER_TYPE(*parameter_type) == PARAMETER_TYPE_SSL_WITH_KEY)
				has_memory_buffers = 1;

			parameter_value = retrace_print_parameter(event_info->event_type,
								  GET_PARAMETER_TYPE(*parameter_type),
								  GET_PARAMETER_FLAGS(*parameter_type),
								  parameter_value);
			trace_printf(0, ", ");

			parameter_type++;
		}

		trace_printf(0, ")");

		/* Return value is only valid in EVENT_TYPE_AFTER_CALL */
		if (event_info->event_type == EVENT_TYPE_AFTER_CALL && event_info->return_value_type != PARAMETER_TYPE_END) {
			trace_printf(0, " = ");
			retrace_print_parameter(event_info->event_type,
						 GET_PARAMETER_TYPE(event_info->return_value_type),
						 GET_PARAMETER_FLAGS(event_info->return_value_type),
						 &event_info->return_value);
		}

		if (event_info->extra_info)
			trace_printf(0, " [%s]", event_info->extra_info);

		if (event_info->event_flags & EVENT_FLAGS_PRINT_RAND_SEED)
			trace_printf(0, " [fuzzing seed: %u]", g_rand_seed);

		trace_printf(0, "\n");

		/* Give another pass to dump memory buffers in case we have any */
		if (has_memory_buffers && event_info->event_type == EVENT_TYPE_AFTER_CALL) {
			parameter_type = event_info->parameter_types;
			parameter_value = event_info->parameter_values;

			while (GET_PARAMETER_TYPE(*parameter_type) != PARAMETER_TYPE_END) {

				parameter_value = retrace_dump_parameter(GET_PARAMETER_TYPE(*parameter_type), 0, parameter_value);

				parameter_type++;
			}

		}
	}

	pthread_mutex_unlock(&printing_lock);
}

void
retrace_log_and_redirect_before(struct rtr_event_info *event_info)
{
	/* Don't do anything for now */
#if 0
	event_info->event_type = EVENT_TYPE_BEFORE_CALL;
	retrace_event(event_info);
#endif
}

void
retrace_log_and_redirect_after(struct rtr_event_info *event_info)
{
	event_info->event_type = EVENT_TYPE_AFTER_CALL;
	retrace_event(event_info);
}


struct config_entry {
	SLIST_ENTRY(config_entry) next;
	char *line;  /* line with commas replaced by '\0' */
	int nargs;
};
SLIST_HEAD(config_head, config_entry);

void
trace_printfv(int hdr, char *color, const char *fmt, va_list arglist)
{
	int old_trace_state;
	FILE *output_file_current = stderr;
	static int output_file_flush;
	static FILE *output_file;
	static char *output_file_path;
	static int loaded_config;
	int is_a_tty = 0;

	if (!get_tracing_enabled())
		return;

	pthread_mutex_lock(&logfile_lock);
	if (!loaded_config) {
		loaded_config = 1;
		if (rtr_get_config_single("logtofile", ARGUMENT_TYPE_STRING, ARGUMENT_TYPE_INT, ARGUMENT_TYPE_END,
								  &output_file_path, &output_file_flush)) {
			old_trace_state = trace_disable();
			if (output_file_path) {
				FILE *out_file_tmp = real_fopen(output_file_path, "a");

				if (out_file_tmp)
					output_file = out_file_tmp;
			}

			trace_restore(old_trace_state);
		}
	}
	pthread_mutex_unlock(&logfile_lock);

	if (output_file)
		output_file_current = output_file;

	old_trace_state = trace_disable();

	if (hdr == 1) {
		real_fprintf(output_file_current, "(%d) ", real_getpid());

		if (!is_main_thread())
			real_fprintf(output_file_current, "(thread: %u) ", pthread_self());
	}

	if (color) {
		int fd;

		fd = real_fileno(output_file_current);
		is_a_tty = isatty(fd);

		if (is_a_tty)
			real_fputs(color, output_file_current);
	}

	if (arglist)
		real_vfprintf(output_file_current, fmt, arglist);

	if (output_file_flush)
		fflush(output_file_current);

	trace_restore(old_trace_state);
}

static void
trace_set_color(char *color)
{
	trace_printfv(0, color, "", NULL);
}


void
trace_printf(int hdr, const char *fmt, ...)
{
	va_list arglist;

	va_start(arglist, fmt);
	trace_printfv(hdr, NULL, fmt, arglist);
	va_end(arglist);
}

void
trace_printf_str(const char *string, int maxlength)
{
	static const char CR[] = VAR "\\r" RST;
	static const char LF[] = VAR "\\n" RST;
	static const char TAB[] = VAR "\\t" RST;
	static const char SNIP[] = "[SNIP]";
	char buf[MAXLEN * (sizeof(CR)-1) + sizeof(SNIP)];
	int i;
	char *p;
	int old_trace_state;

	if (!get_tracing_enabled() || string == NULL || *string == '\0')
		return;

	old_trace_state = trace_disable();

	if (maxlength != -1)
		maxlength = maxlength > MAXLEN ? MAXLEN : maxlength;
	else
		maxlength = MAXLEN;

	for (i = 0, p = buf; i < maxlength && string[i] != '\0'; i++) {
		if (string[i] == '\n')
			strcpy(p, LF);
		else if (string[i] == '\r')
			strcpy(p, CR);
		else if (string[i] == '\t')
			strcpy(p, TAB);
		else if (string[i] == '%')
			strcpy(p, "%%");
		else {
			*(p++) = string[i];
			*p = '\0';
		}
		while (*p)
			++p;
	}

	if (string[i] != '\0')
		strcpy(p, SNIP);

	trace_restore(old_trace_state);

	trace_printf(0, buf);
}

#define DUMP_LINE_SIZE 20
void
trace_dump_data(const unsigned char *buf, size_t nbytes)
{
	static const char fmt[] = "\t%07u\t%s | %s\n";
	static const size_t asc_len = DUMP_LINE_SIZE + 1;
	static const size_t hex_len = DUMP_LINE_SIZE * 2 + DUMP_LINE_SIZE/2 + 2;
	char *hex_str, *asc_str;
	char *hexp, *ascp;
	size_t i;
	int disable = 0;
	int old_trace_state;

	if (rtr_get_config_single("disabledatadump", ARGUMENT_TYPE_INT, ARGUMENT_TYPE_END, &disable)) {
		if (disable)
			return;
	}

	if (!get_tracing_enabled())
		return;

	old_trace_state = trace_disable();

	hex_str = alloca(hex_len);
	asc_str = alloca(asc_len);

	for (i = 0; i < nbytes; i++) {
		if (i % DUMP_LINE_SIZE == 0) {
			if (i) {
				trace_restore(old_trace_state);
				trace_printf(0, fmt, i - DUMP_LINE_SIZE, hex_str, asc_str);
				old_trace_state = trace_disable();
			}
			hexp = hex_str;
			memset(asc_str, 0, asc_len);
			ascp = asc_str;
		}
		*(ascp++) = buf[i] > 31 && buf[i] < 127 ? buf[i] : '.';

		hexp += real_sprintf(hexp, i % 2 ? "%02x" : " %02x", buf[i]);
	}
	if (nbytes % DUMP_LINE_SIZE) {
		int n = DUMP_LINE_SIZE - nbytes % DUMP_LINE_SIZE;

		real_sprintf(hexp, "%*s", n * 2 + n/2, "");

		trace_restore(old_trace_state);
		trace_printf(0, fmt, i - DUMP_LINE_SIZE + n, hex_str, asc_str);
		old_trace_state = trace_disable();
	}

	trace_restore(old_trace_state);
}

static void
initialize_tracing_key(void)
{
	pthread_key_create(&g_enable_tracing_key, NULL);
}

static int
is_main_thread(void)
{
#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__)
	return pthread_main_np();
#elif defined(__NetBSD__)
	return (_lwp_self() == 1);
#else
	return (syscall(SYS_gettid) == real_getpid());
#endif
}

int
get_tracing_state()
{
	if (!is_main_thread()) {
		pthread_once(&tracing_key_once, initialize_tracing_key);

		if (pthread_getspecific(g_enable_tracing_key) == NULL) {
			pthread_setspecific(g_enable_tracing_key , (void *) (size_t) RTR_TRACE_ENABLED);
		}

		return (int)(size_t) pthread_getspecific(g_enable_tracing_key);
	}

	return g_enable_tracing;
}

int
get_tracing_enabled()
{
	return (get_tracing_state() == RTR_TRACE_ENABLED);
}

void
trace_restore(int old_state)
{
	if (is_main_thread()) {
		g_enable_tracing = old_state;
        } else {
                pthread_setspecific(g_enable_tracing_key , (void *) (size_t) old_state);
        }
}

int
trace_disable()
{
	int oldstate;

	oldstate = get_tracing_state();

	trace_restore(RTR_TRACE_DISABLED);

	return oldstate;
}

static FILE *
get_config_file()
{
	FILE *config_file = NULL;
	char *file_path;
	int olderrno;

	olderrno = errno;

	/* If we have a RETRACE_CONFIG env var, try to open the config file from there. */
	file_path = real_getenv("RETRACE_CONFIG");

	if (file_path)
		config_file = real_fopen(file_path, "r");

	/* If we couldn't open the file from the env var try to home it from ~/.retrace.conf */
	if (!config_file) {
		file_path = real_getenv("HOME");

		if (file_path) {
			char *file_path_user;
			char *file_name_user = ".retrace.conf";

			file_path_user = (char *)real_malloc(real_strlen(file_path) + real_strlen(file_name_user) + 2);

			if (file_path_user) {
				real_strcpy(file_path_user, file_path);
				real_strcat(file_path_user, "/");
				real_strcat(file_path_user, file_name_user);

				config_file = real_fopen(file_path_user, "r");

				real_free(file_path_user);
			}
		}
	}

	/* Finally if the above failed try to open /etc/retrace.conf */
	if (!config_file)
		config_file = real_fopen("/etc/retrace.conf", "r");

	errno = olderrno;

	return config_file;
}

static const struct config_entry *
get_config() {
	static struct config_head empty_config
	    = SLIST_HEAD_INITIALIZER(empty_config);
	static struct config_head *pconfig;
	struct config_head *plist;
	struct config_entry *pentry, *ptail;
	FILE *config_file;
	char *buf = NULL, *p;
	size_t buflen = 0;
	ssize_t sz;

	if (pconfig != NULL)
		return (SLIST_FIRST(pconfig));

	config_file = get_config_file();
	if (config_file == NULL) {
		pconfig = &empty_config;
		return NULL;
	}

	for (plist = real_malloc(sizeof(struct config_head)); plist;) {
		sz = getline(&buf, &buflen, config_file);
		if (sz <= 0) {	/* done reading config */
			pconfig = plist;
			plist = NULL;
			break;
		}

		pentry = real_malloc(sizeof(struct config_entry));
		if (pentry == NULL)
			break;

		if (SLIST_EMPTY(plist))
			SLIST_INSERT_HEAD(plist, pentry, next);
		else
			SLIST_INSERT_AFTER(ptail, pentry, next);

		pentry->line =
		    strndup(buf, buf[sz - 1] == '\n' ? sz - 1 : sz);
		if (pentry->line == NULL)
			break;

		pentry->nargs = 0;
		for (p = real_strchr(pentry->line, ','); p; p = real_strchr(++p, ',')) {
			*p = '\0';
			++pentry->nargs;
		}

		ptail = pentry;
	}

	real_free(buf);
	real_fclose(config_file);
	if (plist != NULL) {  /* partially read config */
		while (!SLIST_EMPTY(plist)) {
			pentry = SLIST_FIRST(plist);
			SLIST_REMOVE_HEAD(plist, next);
			real_free(pentry->line);
			real_free(pentry);
		}
		real_free(plist);
		pconfig = &empty_config;
	}

	return (SLIST_FIRST(pconfig));
}

static int
rtr_parse_config(const struct config_entry **pentry,
	const char *function, va_list arg_types)
{
	int retval, nargs, old_trace_state;
	char *parg;
	void *pvar;
	va_list arg_values;

	/*
	 * If we disabled tracing because we are executing some internal code,
	 * don't honor any redirections.
	 */
	if (!get_tracing_enabled())
		return 0;

	/*
	 * Disable tracing so we don't get in loops when the functions we
	 * called here, call other functions that we have replaced.
	 */
	old_trace_state = trace_disable();

	if (*pentry == NULL)
		*pentry = get_config();

	/*
	 * Advance past the types until we find the values.
	 * Counting how many arguments we need to fill
	 */
	__va_copy(arg_values, arg_types);
	nargs = 0;
	while (va_arg(arg_values, int) != ARGUMENT_TYPE_END)
		++nargs;

	retval = 0;
	while (*pentry != NULL && retval == 0) {
		if (real_strcmp(function, (*pentry)->line) == 0
		    && (*pentry)->nargs == nargs) {
			parg = (*pentry)->line; /* points to func name */
			while (nargs--) {
				parg += real_strlen(parg) + 1;
				pvar = va_arg(arg_values, void *);
				switch (va_arg(arg_types, int)) {
				case ARGUMENT_TYPE_INT:
					*((int *)pvar) = atoi(parg);
					break;
				case ARGUMENT_TYPE_STRING:
					*((char **)pvar) = parg;
					break;
				case ARGUMENT_TYPE_DOUBLE:
					*((double *)pvar) = atof(parg);
					break;
				case ARGUMENT_TYPE_UINT:
					*((unsigned int *)pvar) = (unsigned int)strtoul(parg, NULL, 0);
					break;
				}
			}
			retval = 1;
		}
		(*pentry) = SLIST_NEXT(*pentry, next);
	}

	va_end(arg_values);
	trace_restore(old_trace_state);

	return retval;
}

int rtr_get_config_multiple(RTR_CONFIG_HANDLE *handle, const char *function, ...)
{
	int ret = 0;
	const struct config_entry **config = (const struct config_entry **)handle;

	va_list args;

	va_start(args, function);

	ret = rtr_parse_config(config, function, args);

	va_end(args);

	if (!ret)
		*config = NULL;

	return (ret);
}

int rtr_get_config_single(const char *function, ...)
{
	const struct config_entry *config = NULL;
	va_list args;
	int ret;

	va_start(args, function);
	ret = rtr_parse_config(&config, function, args);
	va_end(args);

	return (ret);
}

static char *
retrace_strdup(const char *s)
{
	size_t len;
	char *ret;

	len = real_strlen(s);

	ret = (char *) real_malloc(len + 1);

	if (ret)
		memcpy(ret, s, len + 1);

	return ret;
}


struct descriptor_info *
descriptor_info_new(int fd, unsigned int type, const char *location, int port)
{
	int old_trace_state;
	struct descriptor_info *di;

	old_trace_state = trace_disable();

	di = (struct descriptor_info *) real_malloc(sizeof(struct descriptor_info));

	if (di) {
		di->fd = fd;
		di->type = type;

		if (location)
			di->location = retrace_strdup(location);
		else
			di->location = NULL;

		di->port = port;
	}

	trace_restore(old_trace_state);

	return di;
}

void
descriptor_info_free(struct descriptor_info *di)
{
	int old_trace_state;

	old_trace_state = trace_disable();

	if (di->location)
		real_free(di->location);

	real_free(di);

	trace_restore(old_trace_state);
}

void
file_descriptor_add(int fd, unsigned int type, const char *location, int port)
{
	int free_spot = -1;
	unsigned int i = 0;
	int old_trace_state;
	struct descriptor_info *di;

	old_trace_state = trace_disable();

	di = descriptor_info_new(fd, type, location, port);

	if (!di) {
		trace_restore(old_trace_state);
		return;
	}

	if (g_descriptor_list == NULL) {
		g_descriptor_list_size = DESCRIPTOR_LIST_INITIAL_SIZE;
		g_descriptor_list = (struct descriptor_info **)real_malloc(
		  DESCRIPTOR_LIST_INITIAL_SIZE * sizeof(struct descriptor_info *));

		memset(g_descriptor_list,
		   0,
		   DESCRIPTOR_LIST_INITIAL_SIZE * sizeof(struct descriptor_info *));
	}

	for (i = 0; i < g_descriptor_list_size; i++) {
		if (g_descriptor_list[i] == NULL) {
			free_spot = i;
			break;
		}
	}

	if (free_spot == -1) {
		/* If we don't have any free spots double the list size */
		int new_size = g_descriptor_list_size * 2;

		g_descriptor_list = (struct descriptor_info **)realloc(
		  g_descriptor_list, new_size * sizeof(struct descriptor_info *));

		if (g_descriptor_list) {
			/* clear the new spots we added */
			memset(g_descriptor_list + g_descriptor_list_size,
			       0,
			       (new_size - g_descriptor_list_size) * sizeof(struct descriptor_info *));

			/* Insert at the end of old list */
			free_spot = g_descriptor_list_size;

			g_descriptor_list_size = new_size;
		}
	}

	if (free_spot != -1)
		g_descriptor_list[free_spot] = di;

	trace_restore(old_trace_state);
}

struct descriptor_info *
file_descriptor_get(int fd)
{
	unsigned int i;

	for (i = 0; i < g_descriptor_list_size; i++) {
		if (g_descriptor_list[i] && g_descriptor_list[i]->fd == fd)
			return g_descriptor_list[i];
	}

	return NULL;
}

void
file_descriptor_update(int fd, unsigned int type, const char *location, int port)
{
	struct descriptor_info *di;

	di = file_descriptor_get(fd);

	/* If found, update */
	if (di) {
		di->type = type;
		if (di->location) {
			real_free(di->location);
		}
		di->location = retrace_strdup(location);

		di->port = port;
	} else {
		/* If not found, add it */
		file_descriptor_add(fd, type, location, port);
	}
}

void
file_descriptor_remove(int fd)
{
	unsigned int i;

	for (i = 0; i < g_descriptor_list_size; i++) {
		if (g_descriptor_list[i] && g_descriptor_list[i]->fd == fd) {
			descriptor_info_free(g_descriptor_list[i]);
			g_descriptor_list[i] = NULL;
		}
	}
}

/* lightweight copy of strmode() from FreeBSD for displaying mode_t in chmod */
void
trace_mode(mode_t mode, char *p)
{
	/* usr */
	if (mode & S_IRUSR)
		*p++ = 'r';
	else
		*p++ = '-';
	if (mode & S_IWUSR)
		*p++ = 'w';
	else
		*p++ = '-';

	switch (mode & (S_IXUSR | S_ISUID)) {
	case 0:
		*p++ = '-';
		break;
	case S_IXUSR:
		*p++ = 'x';
		break;
	case S_ISUID:
		*p++ = 'S';
		break;
	case S_IXUSR | S_ISUID:
		*p++ = 's';
		break;
	}

	/* group */
	if (mode & S_IRGRP)
		*p++ = 'r';
	else
		*p++ = '-';
	if (mode & S_IWGRP)
		*p++ = 'w';
	else
		*p++ = '-';

	switch (mode & (S_IXGRP | S_ISGID)) {
	case 0:
		*p++ = '-';
		break;
	case S_IXGRP:
		*p++ = 'x';
		break;
	case S_ISGID:
		*p++ = 'S';
		break;
	case S_IXGRP | S_ISGID:
		*p++ = 's';
		break;
	}

	/* other */
	if (mode & S_IROTH)
		*p++ = 'r';
	else
		*p++ = '-';
	if (mode & S_IWOTH)
		*p++ = 'w';
	else
		*p++ = '-';

	switch (mode & (S_IXOTH | S_ISVTX)) {
	case 0:
		*p++ = '-';
		break;
	case S_IXOTH:
		*p++ = 'x';
		break;
	case S_ISVTX:
		*p++ = 'T';
		break;
	case S_IXOTH | S_ISVTX:
		*p++ = 't';
		break;
	}

	*p = '\0';
}

/* printf backtrace callback */
void trace_printf_backtrace(void)
{
	void *callstack[128];
	int old_trace_state;

	/* check tracing has enabled to avoid infinite loop, because backtrace() uses malloc() function */
	if (!get_tracing_enabled())
		return;

	if (!rtr_get_config_single("backtrace", ARGUMENT_TYPE_END))
		return;

	old_trace_state = trace_disable();

	int i, frames = backtrace(callstack, 128);
	char **strs = backtrace_symbols(callstack, frames);

	if (strs != NULL) {
		trace_set_color(INF);
		printf("======== begin callstack =========\n");
		for (i = 2; i < frames; ++i)
			printf("%s\n", strs[i]);
		printf("======== end callstack =========\n");
		trace_set_color(RST);

		real_free(strs);
	}

	trace_restore(old_trace_state);
}

static void
rtr_init_random(void)
{
	if (!g_init_rand) {
		if (!rtr_get_config_single("fuzzingseed", ARGUMENT_TYPE_UINT, ARGUMENT_TYPE_END, &g_rand_seed))
			g_rand_seed = time(NULL);

		srand(g_rand_seed);
		g_init_rand = 1;
	}
}

/* get fuzzing flag by caculating fail status randomly */
int
rtr_get_fuzzing_flag(double fail_rate)
{
	long int random_value;

	rtr_init_random();

	random_value = rand();
	if (random_value <= (RAND_MAX * fail_rate))
		return 1;

	return 0;
}

int
rtr_get_fuzzing_random(void)
{
	rtr_init_random();

	return rand();
}

/* get string from type value */
void
rtr_get_type_string(int type, const struct ts_info *ts_info, char *str, size_t size)
{
	const struct ts_info *p;
	size_t str_len = 0;

	/* init result string */
	memset(str, 0, size);

	for (p = ts_info; p->str != NULL; p++) {
		if ((p->type & type) != p->type)
			continue;

		if ((str_len + real_strlen(p->str) + 2) > size)
			break;

		if (str_len > 0) {
			real_strcat(str + str_len, "|");
			str_len++;
		}

		real_strcpy(str + str_len, p->str);
		str_len += real_strlen(p->str);
	}
}
