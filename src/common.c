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

#if __OpenBSD__
#include <amd64/spinlock.h>
#endif

#include <stdarg.h>
#include <errno.h>
#include <sys/queue.h>
#include <dirent.h>
#include <sys/uio.h>
#include <sys/utsname.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <netdb.h>
#include <sys/socket.h>

#if HAVE_EXECINFO_H
#include <execinfo.h>
#endif

#include "str.h"
#include "id.h"
#include "file.h"
#include "malloc.h"
#include "printf.h"
#include "env.h"
#include "dir.h"
#include "ssl.h"
#include "rtr-time.h"

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

#ifdef __FreeBSD__
static __thread int g_enable_tracing = RTR_TRACE_ENABLED;
#else
static int g_enable_tracing = RTR_TRACE_ENABLED;
#endif

#ifdef __OpenBSD__
/* This is only used in OpenBSD for now */
#define RETRACE_MAX_THREAD 512

struct thread_storage {
	pthread_t thread_id;
	int value;
};

_atomic_lock_t enabled_lock = _ATOMIC_LOCK_UNLOCKED;

static struct thread_storage g_enable_tracing_array[RETRACE_MAX_THREAD];
#else
static pthread_key_t g_enable_tracing_key = -1;
#endif

static int g_init_rand;
static unsigned int g_rand_seed;

static rtr_logging_config_t g_logging_config;

/*
 * Global list of file descriptors so we can track
 * their usage across different functions
 */
SLIST_HEAD(fdlist_head, descriptor_info) g_fdlist =
	SLIST_HEAD_INITIALIZER(g_fdlist);

static pthread_mutex_t printing_lock = PTHREAD_MUTEX_INITIALIZER;

static int show_timestamp;
static int output_file_flush;
static FILE *output_file;

static int is_main_thread(void);
static void trace_set_color(char *color);

static int rtr_get_config_single_internal(const char *function, ...);

static void trace_printf_str(const char *string, int maxlength);
static void trace_dump_data(const unsigned char *buf, size_t nbytes);
static void trace_mode(mode_t mode, char *p);
static void trace_printf_backtrace(void);

static int rtr_check_logging_config(struct rtr_event_info *event_info, int stack_trace);
static void initialize_tracing_key(void);

/* Returns time as zero on the first call and in subsequents call
 * returns the time elapsed since the first called */
static double
retrace_get_time(void)
{
	static double start_time;
	double ret = 0;

#if defined(CLOCK_MONOTONIC) && defined(__x86_64__)
	struct timespec current_time = {0, 0};

	clock_gettime(CLOCK_MONOTONIC, &current_time);

	if (start_time == 0) {
		start_time = current_time.tv_sec;
		start_time += current_time.tv_nsec / 1E9; /* 1 second = 1e9 nano seconds */
	} else {
		ret = current_time.tv_sec;
		ret += current_time.tv_nsec / 1E9; /* 1 second = 1e9 nano seconds */
		ret -= start_time;
	}
#else
	struct timeval current_time = {0, 0};

	real_gettimeofday(&current_time, NULL);

	if (start_time == 0) {
		start_time = current_time.tv_sec;
		start_time += (double) current_time.tv_usec / (double) 1E6; /* 1 second = 1e6 micro seconds */
	} else {
		ret = current_time.tv_sec;
		ret += (double) current_time.tv_usec / (double) 1E6; /* 1 second = 1e6 micro seconds */
		ret -= start_time;
	}
#endif

	return ret;
}

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

		trace_printf(0, "%p", dirp);
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
	case PARAMETER_TYPE_PERM:
	{
		char perm[10];

		trace_mode(*((mode_t *) *value), perm);
		trace_printf(0, "%o", *((int *) *value));
		trace_set_color(INF);
		trace_printf(0, " [%s]", perm);
		trace_set_color(VAR);

		break;
	}

	case PARAMETER_TYPE_STRUCT_STAT:
	{
		char perm[10];

		trace_printf(1, "struct stat {\n");
		trace_printf(1, "\tst_dev = %lu\n", (*(struct stat **) *value)->st_dev);
		trace_printf(1, "\tst_ino = %i\n", (*(struct stat **) *value)->st_ino);
		trace_mode((*(struct stat **) *value)->st_mode, perm);
		trace_printf(1, "\tst_mode = %d [%s]\n", (*(struct stat **) *value)->st_mode, perm);
		trace_printf(1, "\tst_nlink = %lu\n", (*(struct stat **) *value)->st_nlink);
		trace_printf(1, "\tst_uid = %d\n", (*(struct stat **) *value)->st_uid);
		trace_printf(1, "\tst_gid = %d\n", (*(struct stat **) *value)->st_gid);
		trace_printf(1, "\tst_rdev = %r\n", (*(struct stat **) *value)->st_rdev);
		trace_printf(1, "\tst_atime = %lu\n", (*(struct stat **) *value)->st_atime);
		trace_printf(1, "\tst_mtime = %lu\n", (*(struct stat **) *value)->st_mtime);
		trace_printf(1, "\tst_ctime = %lu\n", (*(struct stat **) *value)->st_ctime);
		trace_printf(1, "\tst_size = %zu\n", (*(struct stat **) *value)->st_size);
		trace_printf(1, "\tst_blocks = %lu\n", (*(struct stat **) *value)->st_blocks);
		trace_printf(1, "\tst_blksize = %lu\n", (*(struct stat **) *value)->st_blksize);
#if __APPLE__
		trace_printf(1, "\tst_flags = %d\n", (*(struct stat **) *value)->st_flags);
		trace_printf(1, "\tst_gen = %d\n", (*(struct stat **) *value)->st_gen);
#endif
		trace_printf(1, "}\n");

		break;
	}

	case PARAMETER_TYPE_STRUCT_SOCKADDR:
		switch ((*(struct sockaddr **) *value)->sa_family) {
		case AF_INET:
			trace_printf(0, "%s:%d[AF_INET]",
						 inet_ntoa(((struct sockaddr_in *)(*(struct sockaddr **) *value))->sin_addr),
						 ntohs(((struct sockaddr_in *)(*(struct sockaddr **) *value))->sin_port));
			break;

#ifdef AF_INET6
		case AF_INET6:
			trace_printf(0, "[%s]:%d[AF_INET6]",
						 inet_ntoa(((struct sockaddr_in *)(*(struct sockaddr **) *value))->sin_addr),
						 ntohs(((struct sockaddr_in *)(*(struct sockaddr **) *value))->sin_port));
			break;
#endif

		case AF_UNIX:
			trace_printf(0, "%s[AF_UNIX|AF_LOCAL]", ((struct sockaddr_un *)(*(struct sockaddr **) *value))->sun_path);
			break;

		default:
			trace_printf(0, "unssuported sa_family: %d", (*(struct sockaddr **) *value)->sa_family);
			break;
		}
		break;

	case PARAMETER_TYPE_FD_SET:
	{
		int fd, comma = 0;

		const char *set = (*(const char **) *value);
		value++;

		int nfds = (*(int *) *value);
		value++;

		fd_set in = (*(fd_set *) *value);
		value++;

		fd_set *out = (*(fd_set **) *value);

		if (out == NULL)
			break;

		for (fd = 0; fd < nfds; fd++) {
			if (FD_ISSET(fd, &in)) {
				trace_printf(0, "%.*s%.*s%d", comma, ",",
							 FD_ISSET(fd, out) ? 1 : 0, "+", fd);

				if (comma == 0)
					comma = 1;
			}
		}
		trace_printf(0, ")");

		break;
	}

	case PARAMETER_TYPE_STRUCT_HOSTEN:
	{
		int i;
		struct hostent *hent = *(struct hostent **) *value;

		for (i = 0; hent->h_addr_list[i] != NULL; i++) {
			char ip_addr[INET6_ADDRSTRLEN];

			inet_ntop(hent->h_addrtype, hent->h_addr_list[i], ip_addr, sizeof(ip_addr));
			trace_printf(0, i > 0 ? ",%s" : "%s", ip_addr);
		}

		break;
	}

	case PARAMETER_TYPE_IP_ADDR:
	{
		char ip_addr[INET6_ADDRSTRLEN];

		const void *addr = *value;
		value++;

		int type = *(int *)value;

		inet_ntop(type, addr, ip_addr, sizeof(ip_addr));

		trace_printf(0, "%s", addr);

		break;
	}

	case PARAMETER_TYPE_STRUCT_ADDRINFO:
	{
		struct addrinfo *rp, *result = *((struct addrinfo **)*value);

		for (rp = result; rp != NULL; rp = rp->ai_next) {
			char addr[INET6_ADDRSTRLEN];

			if (rp != result) {
				trace_printf(0, ",");
			}

			switch (rp->ai_family) {
			case AF_INET:
				inet_ntop(rp->ai_family, &(((struct sockaddr_in *)rp->ai_addr)->sin_addr), addr, sizeof(addr));

				trace_printf(0, "%s", addr);
				break;

			case AF_INET6:
				inet_ntop(rp->ai_family, &(((struct sockaddr_in6 *)rp->ai_addr)->sin6_addr), addr, sizeof(addr));

				trace_printf(0, "%s", addr);
				break;

			default:
				trace_printf(0, "AI_FAMILY:%d", rp->ai_family);
				break;
			}
		}

		trace_printf(0, "]\n");

		break;
	}

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

#ifdef HAVE_OPENSSL_SSL_H
static void
retrace_print_key(const unsigned char *buf, int len)
{
	int i;
	for (i = 0; i < len; i++) {
		trace_printf(0, "%02X", buf[i]);
	}
}

static void
retrace_print_ssl_keys(void *_ssl)
{
	SSL *ssl = (SSL *) _ssl;

	SSL_SESSION *session = NULL;
	size_t master_key_length = 0;
	size_t client_random_length = 0;
#if OPENSSL_VERSION_NUMBER < 0x10100000L
	unsigned char *client_random;
	unsigned char *master_key;
#else
	unsigned char client_random[SSL3_RANDOM_SIZE];
	unsigned char master_key[SSL_MAX_MASTER_KEY_LENGTH];
#endif

	if (ssl) {
#if OPENSSL_VERSION_NUMBER < 0x10100000L
		session = ssl->session;

		if (ssl->s3) {
			client_random = ssl->s3->client_random;
			client_random_length = SSL3_RANDOM_SIZE;
		}

		if (session) {
			master_key = session->master_key;
			master_key_length = session->master_key_length;
		}
#else
		rtr_SSL_get_session_t real_SSL_get_session;
		rtr_SSL_SESSION_get_master_key_t real_SSL_SESSION_get_master_key;
		rtr_SSL_get_client_random_t real_SSL_get_client_random;

		*(void **) &real_SSL_get_client_random = dlsym(RTLD_DEFAULT, "SSL_get_client_random");
		*(void **) &real_SSL_SESSION_get_master_key = dlsym(RTLD_DEFAULT, "SSL_SESSION_get_master_key");
		*(void **) &real_SSL_get_session = dlsym(RTLD_DEFAULT, "SSL_get_session");

		if (real_SSL_get_client_random &&
		    real_SSL_SESSION_get_master_key &&
		    real_SSL_get_client_random) {
			session = real_SSL_get_session (ssl);

			if (session) {
				master_key_length = real_SSL_SESSION_get_master_key(session, master_key, SSL_MAX_MASTER_KEY_LENGTH);
				client_random_length = real_SSL_get_client_random(ssl, client_random, SSL3_RANDOM_SIZE);
			}
		}
#endif
	}

	if (master_key_length > 0 && client_random_length > 0) {
		trace_printf(0, "\tCLIENT_RANDOM ");
		retrace_print_key(client_random, client_random_length);
		trace_printf(0, " ");
		retrace_print_key(master_key, master_key_length);
		trace_printf(0, "\n");
	}
}
#endif

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

		if (size == 1) /* Special case for size == 1 */
			trace_dump_data((*(unsigned char **) *value), nmemb);
		else if (size > 0)
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
			retrace_print_ssl_keys(ssl);
#endif /* HAVE_OPENSSL_SSL */
	}

	return value + 1;
}


void
retrace_event(struct rtr_event_info *event_info)
{
	int olderrno;
	int old_trace_state;
	static char *output_file_path;
	static int loaded_config;
	FILE *out_file_tmp = NULL;

	if (!get_tracing_enabled())
		return;

	if (event_info->event_type == EVENT_TYPE_BEFORE_CALL) {
		event_info->start_time = retrace_get_time();

		/* Don't log any call on before unless explicitly asked too */
		if (!(event_info->event_flags & EVENT_FLAGS_PRINT_BEFORE))
			return;
	}

	old_trace_state = trace_disable();
	pthread_mutex_lock(&printing_lock);
	olderrno = errno;

	if (!loaded_config) {
		loaded_config = 1;
		if (rtr_get_config_single_internal("logtofile", ARGUMENT_TYPE_STRING, ARGUMENT_TYPE_INT, ARGUMENT_TYPE_END,
								  &output_file_path, &output_file_flush)) {
			if (output_file_path) {
				 out_file_tmp = real_fopen(output_file_path, "a");
			}
			output_file = out_file_tmp;
		}

		if (rtr_get_config_single_internal("showtimestamp", ARGUMENT_TYPE_END))
			show_timestamp = 1;
	}

	if (!rtr_check_logging_config(event_info, 0)) {
		pthread_mutex_unlock(&printing_lock);
		trace_restore(old_trace_state);

		return;
	}

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

		if (event_info->event_type == EVENT_TYPE_AFTER_CALL) {
			static int loaded_time_config;
			static double timestamp_limit;

			if (!loaded_time_config) {
				loaded_time_config = 1;

				if (!rtr_get_config_single_internal("showcalltime", ARGUMENT_TYPE_DOUBLE, ARGUMENT_TYPE_END,
								&timestamp_limit))
					timestamp_limit = -1;
			}

			if (timestamp_limit >= 0) {
				double elapsed_time;

				elapsed_time = retrace_get_time() - event_info->start_time;

				if (elapsed_time >= timestamp_limit) {
					trace_set_color(INF);
					trace_printf(0, " [took: %0.5f]", elapsed_time);
					trace_set_color(RST);
				}
			}
		}

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

	if ((event_info->event_flags & EVENT_FLAGS_PRINT_BACKTRACE) &&
		(rtr_check_logging_config(event_info, 1))) {
		trace_printf_backtrace();
	}

	errno = olderrno;

	pthread_mutex_unlock(&printing_lock);
	trace_restore(old_trace_state);
}

void
retrace_log_and_redirect_before(struct rtr_event_info *event_info)
{
	event_info->event_type = EVENT_TYPE_BEFORE_CALL;
	retrace_event(event_info);
}

void
retrace_log_and_redirect_after(struct rtr_event_info *event_info)
{
	event_info->event_type = EVENT_TYPE_AFTER_CALL;
	retrace_event(event_info);
}


struct config_entry {
	STAILQ_ENTRY(config_entry) next;
	char *line;  /* line with commas replaced by '\0' */
	int nargs;
};
STAILQ_HEAD(config_head, config_entry);

static void
trace_printfv(int hdr, char *color, const char *fmt, va_list arglist)
{
	int old_trace_state;
	FILE *output_file_current = stderr;
	int is_a_tty = 0;

	if (output_file)
		output_file_current = output_file;

	old_trace_state = trace_disable();

	if (hdr == 1) {
		real_fprintf(output_file_current, "(%d) ", real_getpid());

		if (!is_main_thread())
			real_fprintf(output_file_current, "(thread: %u) ", pthread_self());

		if (show_timestamp) {
			float current_time;

			current_time = retrace_get_time();
			real_fprintf(output_file_current, "(%0.5f) ", current_time);
		}
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

static void
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

	if (string == NULL || *string == '\0')
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
static void
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

	if (rtr_get_config_single_internal("disabledatadump", ARGUMENT_TYPE_INT, ARGUMENT_TYPE_END, &disable)) {
		if (disable)
			return;
	}

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
#ifndef __OpenBSD__
	pthread_key_create(&g_enable_tracing_key, NULL);
#endif
}

static void
retrace_init_tracing_key(void)
{
#ifndef __OpenBSD__
	static pthread_once_t tracing_key_once = PTHREAD_ONCE_INIT;

	pthread_once(&tracing_key_once, initialize_tracing_key);
#endif
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

static int
get_tracing_state_thread()
{
#ifdef __OpenBSD__
	int i;
	int last_zero = -1;
	int retval = -1;

	while (_atomic_lock(&enabled_lock))
		sched_yield();

	for (i = 0; i < RETRACE_MAX_THREAD; i++) {
		if (g_enable_tracing_array[i].thread_id == pthread_self()) {
			retval = g_enable_tracing_array[i].value;
			break;
		} else if (last_zero == -1 && g_enable_tracing_array[i].thread_id == 0)
			last_zero = i;
	}

	if (retval == -1 && last_zero != -1) {
		g_enable_tracing_array[last_zero].thread_id = pthread_self();
		g_enable_tracing_array[last_zero].value = RTR_TRACE_ENABLED;

		retval = g_enable_tracing_array[last_zero].value;
	}

	enabled_lock = _ATOMIC_LOCK_UNLOCKED;

	return retval == -1 ? RTR_TRACE_DISABLED : retval;

#else
	if (pthread_getspecific(g_enable_tracing_key) == NULL)
		pthread_setspecific(g_enable_tracing_key, (void *) (size_t) RTR_TRACE_ENABLED);

	return (int)(size_t) pthread_getspecific(g_enable_tracing_key);
#endif
}

static void
set_tracing_state_thread(int state)
{
#ifdef __OpenBSD__
	int i;
	int last_zero = -1;
	int found = 0;

	while (_atomic_lock(&enabled_lock))
		sched_yield();

	for (i = 0; i < RETRACE_MAX_THREAD; i++) {
		if (g_enable_tracing_array[i].thread_id == pthread_self()) {
			g_enable_tracing_array[i].value = state;
			found = 1;
			break;
		} else if (last_zero == -1 && g_enable_tracing_array[i].thread_id == 0)
			last_zero = i;
	}

	if (!found && last_zero != -1) {
		g_enable_tracing_array[last_zero].thread_id = pthread_self();
		g_enable_tracing_array[last_zero].value = state;
	}

	enabled_lock = _ATOMIC_LOCK_UNLOCKED;
#else
	pthread_setspecific(g_enable_tracing_key, (void *) (size_t) (state == RTR_TRACE_ENABLED ?
		RTR_TRACE_ENABLED : RTR_TRACE_DISABLED));
#endif
}

int
get_tracing_state()
{
#ifndef __FreeBSD__
	if (!is_main_thread()) {
		retrace_init_tracing_key();

		return get_tracing_state_thread();
	}
#endif
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
#ifdef __FreeBSD__
	g_enable_tracing = old_state;
#else
	if (is_main_thread()) {
		g_enable_tracing = old_state;
        } else {
		set_tracing_state_thread(old_state);
        }
#endif
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
	struct config_head config;

	FILE *config_file;
	char *buf = NULL, *p;
	size_t buflen = 0;
	ssize_t sz;

	static struct config_entry config_end[1];
	static const struct config_entry *pconfig;

	if (pconfig != NULL)
		return pconfig;

	memset(config_end, 0, sizeof(config_end));

	config_file = get_config_file();
	if (config_file == NULL) {
		pconfig = config_end;
		return pconfig;
	}

	STAILQ_INIT(&config);
	for (;;) {
		struct config_entry *pentry;

		sz = getline(&buf, &buflen, config_file);

		if (sz <= 0)
			break;

		if (buf[sz - 1] == '\n')
			--sz;

		if (sz == 0)
			continue;

		pentry = real_malloc(sizeof(struct config_entry) + sz + 1);
		if (pentry != NULL) {
			pentry->line = (char *)&pentry[1];
			real_strncpy(pentry->line, buf, sz);
			pentry->line[sz] = '\0';
			pentry->nargs = 0;
			p = real_strchr(pentry->line, ',');
			while (p != NULL) {
				*p = '\0';
				++pentry->nargs;
				p = real_strchr(p + 1, ',');
			}
			STAILQ_INSERT_TAIL(&config, pentry, next);
		}
	}

	real_free(buf);
	real_fclose(config_file);

	STAILQ_INSERT_TAIL(&config, config_end, next);

	pconfig = STAILQ_FIRST(&config);

	return pconfig;
}

static int
rtr_parse_config(const struct config_entry **pentry,
	const char *function, va_list arg_types)
{
	int retval, nargs;
	char *parg;
	void *pvar;
	va_list arg_values;

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
	while ((*pentry)->line != NULL && retval == 0) {
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

		*pentry = STAILQ_NEXT(*pentry, next);
	}

	va_end(arg_values);

	return retval;
}

int rtr_get_config_multiple(RTR_CONFIG_HANDLE *handle, const char *function, ...)
{
	int old_trace_state, ret = 0;
	const struct config_entry **config = (const struct config_entry **)handle;

	va_list args;

	if (!get_tracing_enabled())
		return 0;

	old_trace_state = trace_disable();

	va_start(args, function);
	ret = rtr_parse_config(config, function, args);
	va_end(args);

	trace_restore(old_trace_state);

	if (!ret)
		*config = NULL;

	return (ret);
}

int rtr_get_config_single(const char *function, ...)
{
	const struct config_entry *config = NULL;
	va_list args;
	int ret, old_trace_state;

	if (!get_tracing_enabled())
		return 0;

	old_trace_state = trace_disable();

	va_start(args, function);
	ret = rtr_parse_config(&config, function, args);
	va_end(args);

	trace_restore(old_trace_state);

	return (ret);
}

static int
rtr_get_config_single_internal(const char *function, ...)
{
	const struct config_entry *config = NULL;
	va_list args;
	int ret;

	va_start(args, function);
	ret = rtr_parse_config(&config, function, args);
	va_end(args);

	return (ret);
}

struct descriptor_info *
file_descriptor_get(int fd)
{
	struct descriptor_info *pinfo;

	SLIST_FOREACH(pinfo, &g_fdlist, next)
		if (pinfo->fd == fd)
			return pinfo;

	return NULL;
}

struct descriptor_info *
file_descriptor_update(int fd, unsigned int type, const char *location)
{
	struct descriptor_info *pinfo;

	file_descriptor_remove(fd);

	pinfo = real_malloc(sizeof(struct descriptor_info) +
	    real_strlen(location) + 1);

	if (pinfo != NULL) {
		pinfo->fd = fd;
		pinfo->type = type;
		pinfo->location = (char *)&pinfo[1];
		pinfo->http_redirect = NULL;
		real_strcpy(pinfo->location, location);

		SLIST_INSERT_HEAD(&g_fdlist, pinfo, next);
	}

	return pinfo;
}

void
file_descriptor_remove(int fd)
{
	struct descriptor_info *pinfo;

	pinfo = file_descriptor_get(fd);

	if (pinfo) {
		SLIST_REMOVE(&g_fdlist, pinfo, descriptor_info, next);
		if (pinfo->http_redirect != NULL) {
			if (pinfo->http_redirect->filefd != -1)
				real_close(pinfo->http_redirect->filefd);
			real_free(pinfo->http_redirect);
		}
		real_free(pinfo);
	}
}

/* lightweight copy of strmode() from FreeBSD for displaying mode_t in chmod */
static void
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
static void
trace_printf_backtrace()
{
	void *callstack[128];
	int old_trace_state;

	if (!rtr_get_config_single_internal("backtrace", ARGUMENT_TYPE_END))
		return;

	old_trace_state = trace_disable();

	int i, frames = backtrace(callstack, 128);
	char **strs = backtrace_symbols(callstack, frames);

	if (strs != NULL) {
		trace_set_color(INF);
		trace_printf(1, "======== begin callstack =========\n");
		for (i = 2; i < frames; ++i)
			trace_printf(1, "%s\n", strs[i]);
		trace_printf(1, "======== end callstack =========\n");
		trace_set_color(RST);

		real_free(strs);
	}

	trace_restore(old_trace_state);
}

static void
rtr_init_random(void)
{
	if (!g_init_rand) {
		if (!rtr_get_config_single_internal("fuzzingseed", ARGUMENT_TYPE_UINT, ARGUMENT_TYPE_END, &g_rand_seed))
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

/* get configuration token by separator */
int rtr_check_config_token(const char *token, char *str, const char *sep, int *reverse)
{
	char *p, *q;

	/* init reverse flag */
	*reverse = 0;

	p = real_malloc(real_strlen(str) + 1);
	if (!p)
		return 0;

	real_strcpy(p, str);
	p[real_strlen(str)] = '\0';

	q = real_strtok(p, sep);
	while (q != NULL) {
		/* check logical NOT operator '!' */
		if (q[0] == '!')
			*reverse = 1;

		if (real_strcmp(token, (*reverse == 0) ? q : q + 1) == 0)
			return 1;

		q = real_strtok(NULL, sep);
	}

	return 0;
}

/* get fuzzing values */
void *rtr_get_fuzzing_value(enum RTR_FUZZ_TYPE fuzz_type, void *param)
{
	char *ret = NULL;
	int i, len;

	switch (fuzz_type) {
	case RTR_FUZZ_TYPE_BUFOVER:
		len = *((int *) param);

		ret = real_malloc(len + 1);
		memset(ret, 'A', len);
		ret[len] = '\0';

		break;

	case RTR_FUZZ_TYPE_FMTSTR:
		len = *((int *) param);

		ret = real_malloc(len + 1);
		for (i = 0; i < len; i++) {
			char c = (i % 2) ? 's' : '%';

			ret[i] = c;
		}

		ret[len] = '\0';

		break;

	case RTR_FUZZ_TYPE_GARBAGE:
		len = *((int *) param);

		ret = real_malloc(len);
		for (i = 0; i < len; i++)
			ret[i] = (char) rand() % 0xFF;

		break;

	default:
		break;
	}

	return (void *) ret;
}

/* parse global options */
static const char *rtr_logging_groups[] = {
	"LOG_GROUP_MEM",
	"LOG_GROUP_FILE",
	"LOG_GROUP_NET",
	"LOG_GROUP_SYS",
	"LOG_GROUP_STR",
	"LOG_GROUP_SSL",
	"LOG_GROUP_PROC",
	NULL
};

static const char *rtr_logging_levels[] = {
	"LOG_LELEL_NOR",
	"LOG_LEVEL_ERR",
	"LOG_LEVEL_FUZZ",
	"LOG_LEVEL_REDIRECT",
	NULL
};

/* parse logging options */
static void parse_logging_options(int opt_type, char *opt_str)
{
	int i;
	char sep[] = "| \t";

	int reverse = 0;

	if (opt_type == RTR_LOG_OPT_GRP || opt_type == RTR_LOG_OPT_STRACE) {
		int opt_val = 0;

		for (i = 0; rtr_logging_groups[i] != NULL; i++) {
			if (rtr_check_config_token(rtr_logging_groups[i], opt_str, sep, &reverse)) {
				int bit_val = (i == 0) ? 0x01 : i * 2;

				if (!reverse)
					opt_val |= bit_val;
				else
					opt_val &= !bit_val;
			}
		}

		if (opt_type == RTR_LOG_OPT_GRP)
			g_logging_config.group_bitwise = opt_val;
		else
			g_logging_config.stracing_group_bitwise = opt_val;

		return;
	}

	if (opt_type == RTR_LOG_OPT_LEVEL) {
		for (i = 0; rtr_logging_levels[i] != NULL; i++) {
			if (rtr_check_config_token(rtr_logging_levels[i], opt_str, sep, &reverse)) {
				int bit_val = (i == 0) ? 0x01 : i * 2;

				if (!reverse)
					g_logging_config.level_bitwise |= bit_val;
				else
					g_logging_config.level_bitwise &= !bit_val;
			}
		}
	}
}

/* initialize logging configuration */
static void rtr_init_logging_config(void)
{
	char *logging_grps;
	char *logging_types;

	if (g_logging_config.init_flag)
		return;

	memset(&g_logging_config, 0, sizeof(g_logging_config));

	/* read global configuration */
	if (rtr_get_config_single_internal("logging-global", ARGUMENT_TYPE_STRING, ARGUMENT_TYPE_STRING, ARGUMENT_TYPE_END,
		&logging_grps, &logging_types)) {
		parse_logging_options(RTR_LOG_OPT_GRP, logging_grps);
		parse_logging_options(RTR_LOG_OPT_LEVEL, logging_types);
	}

	/* get allowed and excluded function list */
	rtr_get_config_single_internal("logging-allowed-funcs", ARGUMENT_TYPE_STRING,
		ARGUMENT_TYPE_END, &g_logging_config.allowed_funcs);

	rtr_get_config_single_internal("logging-excluded-funcs", ARGUMENT_TYPE_STRING,
		ARGUMENT_TYPE_END, &g_logging_config.disabled_funcs);

	if (rtr_get_config_single_internal("stacktrace-groups", ARGUMENT_TYPE_STRING, ARGUMENT_TYPE_STRING, ARGUMENT_TYPE_END,
		&logging_grps, &logging_types))
		parse_logging_options(RTR_LOG_OPT_STRACE, logging_grps);

	rtr_get_config_single_internal("stacktrace-disabled-funcs", ARGUMENT_TYPE_STRING,
		ARGUMENT_TYPE_END, &g_logging_config.stracing_disabled_funcs);

	/* if global options are missing, then set group and level to all */
	if (g_logging_config.group_bitwise == 0)
		g_logging_config.group_bitwise = RTR_FUNC_GRP_ALL;

	if (g_logging_config.level_bitwise == 0)
		g_logging_config.level_bitwise = RTR_LOG_LEVEL_ALL;

	/* if stacktracing options are missing, then set group to all */
	if (g_logging_config.stracing_group_bitwise == 0)
		g_logging_config.stracing_group_bitwise = RTR_FUNC_GRP_ALL;

	/* set init flag */
	g_logging_config.init_flag = 1;
}

/* check if event info will be logged by logging configuration */
static int rtr_check_logging_config(struct rtr_event_info *event_info, int stack_trace)
{
	int reverse = 0;
	char sep[] = "| \t";

	/* check init status of logging config and init if not */
	if (!g_logging_config.init_flag)
		rtr_init_logging_config();

	if (stack_trace) {
		/* check excluded list */
		if (g_logging_config.stracing_disabled_funcs &&
			rtr_check_config_token(event_info->function_name, g_logging_config.stracing_disabled_funcs, sep, &reverse) &&
			!reverse)
			return 0;

		if ((event_info->function_group & g_logging_config.stracing_group_bitwise) == 0x00)
			return 0;

		return 1;
	}

	/* check fuzzing or redirection is applied */
	if ((event_info->logging_level & RTR_LOG_LEVEL_FUZZ) ||
		(event_info->logging_level & RTR_LOG_LEVEL_REDIRECT))
		return 1;

	/* check the function is in allowed list */
	if (g_logging_config.allowed_funcs &&
		rtr_check_config_token(event_info->function_name, g_logging_config.allowed_funcs, sep, &reverse) &&
		!reverse)
		return 1;

	/* check the function is in excluded list */
	if (g_logging_config.disabled_funcs &&
		rtr_check_config_token(event_info->function_name, g_logging_config.disabled_funcs, sep, &reverse) &&
		!reverse)
		return 0;

	/* check global option */
	if ((event_info->function_group & g_logging_config.group_bitwise) == 0x00)
		return 0;

	/* check logging level */
	if ((event_info->logging_level & g_logging_config.level_bitwise) == 0x00)
		return 0;

	return 1;
}
