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

#include "common.h"
#include "str.h"
#include "id.h"
#include "file.h"
#include "malloc.h"
#include "printf.h"
#include "env.h"

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

/*
 * Global list of file descriptors so we can track
 * their usage across different functions
 *
 */
#define DESCRIPTOR_LIST_INITIAL_SIZE 8
struct descriptor_info **g_descriptor_list;
unsigned int g_descriptor_list_size;

struct config_entry {
	SLIST_ENTRY(config_entry) next;
	char *line;  /* line with commas replaced by '\0' */
	int nargs;
};
SLIST_HEAD(config_head, config_entry);

void
trace_printf(int hdr, const char *fmt, ...)
{
	static const size_t maxlen = 1024;
	char *str;
	va_list arglist;
	int old_trace_state;
	FILE *output_file = stderr;
	char *output_file_path;

	if (!get_tracing_enabled())
		return;
	if (rtr_get_config_single("logtofile", ARGUMENT_TYPE_STRING, ARGUMENT_TYPE_END, &output_file_path)) {
		old_trace_state = trace_disable();
		if (output_file_path) {
			FILE *out_file_tmp = fopen(output_file_path, "a");

			if (out_file_tmp)
				output_file = out_file_tmp;
		}

		trace_restore(old_trace_state);
	}

	old_trace_state = trace_disable();

	str = alloca(maxlen);

	va_start(arglist, fmt);
	vsnprintf(str, maxlen, fmt, arglist);
	va_end(arglist);

	if (hdr == 1)
		fprintf(output_file, "(%d) ", getpid());

	fprintf(output_file, "%s", str);

	if (output_file != stderr)
		fclose(output_file);

	trace_restore(old_trace_state);
}

void
trace_printf_str(const char *string)
{
	static const char CR[] = VAR "\\r" RST;
	static const char LF[] = VAR "\\n" RST;
	static const char TAB[] = VAR "\\t" RST;
	static const char SNIP[] = "[SNIP]";
	char buf[MAXLEN * (sizeof(CR)-1) + sizeof(SNIP)];
	int i;
	char *p;
	int old_trace_state;

	if (!get_tracing_enabled() || *string == '\0')
		return;

	old_trace_state = trace_disable();

	for (i = 0, p = buf; i < MAXLEN && string[i] != '\0'; i++) {
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
	rtr_sprintf_t real_sprintf;
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
	real_sprintf = RETRACE_GET_REAL(sprintf);

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
		sprintf(hexp, "%*s", n * 2 + n/2, "");

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
	rtr_getpid_t real_getpid;

	real_getpid = RETRACE_GET_REAL(getpid);

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
	rtr_fopen_t real_fopen;
	rtr_malloc_t real_malloc;
	rtr_free_t real_free;
	rtr_getenv_t real_getenv;
	int olderrno;

	olderrno = errno;

	real_fopen	= RETRACE_GET_REAL(fopen);
	real_malloc	= RETRACE_GET_REAL(malloc);
	real_free	= RETRACE_GET_REAL(free);
	real_getenv	= RETRACE_GET_REAL(getenv);

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

			file_path_user = (char *)real_malloc(strlen(file_path) + strlen(file_name_user) + 2);

			if (file_path_user) {
				strcpy(file_path_user, file_path);
				strcat(file_path_user, "/");
				strcat(file_path_user, file_name_user);

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
	rtr_free_t real_free;
	rtr_malloc_t real_malloc;
	rtr_strchr_t real_strchr;
	rtr_fclose_t real_fclose;

	real_free = RETRACE_GET_REAL(free);
	real_malloc = RETRACE_GET_REAL(malloc);
	real_strchr = RETRACE_GET_REAL(strchr);
	real_fclose = RETRACE_GET_REAL(fclose);

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
		for (p = real_strchr(pentry->line, ','); p; p = strchr(++p, ',')) {
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
	rtr_strcmp_t real_strcmp;
	rtr_strlen_t real_strlen;

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

	real_strcmp = RETRACE_GET_REAL(strcmp);
	real_strlen = RETRACE_GET_REAL(strlen);

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


struct descriptor_info *
descriptor_info_new(int fd, unsigned int type, const char *location, int port)
{
	int old_trace_state;
	struct descriptor_info *di;

	old_trace_state = trace_disable();

	di = (struct descriptor_info *) malloc(sizeof(struct descriptor_info));

	if (di) {
		di->fd = fd;
		di->type = type;

		if (location)
			di->location = strdup(location);
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
		free(di->location);

	free(di);

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
		g_descriptor_list = (struct descriptor_info **)malloc(
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
			free(di->location);
		}
		di->location = strdup (location);

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
