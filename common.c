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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#ifdef __linux__
#include <syscall.h>
#endif
#include <stdarg.h>

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

void
trace_printf(int hdr, const char *fmt, ...)
{
	static const size_t maxlen = 1024;
	char *str;
	va_list arglist;
	int old_trace_state;

	str = alloca(maxlen);

	if (!get_tracing_enabled())
		return;

	old_trace_state = trace_disable();

	va_start(arglist, fmt);
	vsnprintf(str, maxlen, fmt, arglist);
	va_end(arglist);

	if (hdr == 1)
		fprintf(stderr, "(%d) ", getpid());

	fprintf(stderr, "%s", str);

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

	hex_str = alloca(hex_len);
	asc_str = alloca(asc_len);
	real_sprintf = RETRACE_GET_REAL(sprintf);

	for (i = 0; i < nbytes; i++) {
		if (i % DUMP_LINE_SIZE == 0) {
			if (i)
				trace_printf(0, fmt, i - DUMP_LINE_SIZE, hex_str, asc_str);
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
		trace_printf(0, fmt, i - DUMP_LINE_SIZE + n, hex_str, asc_str);
	}
}

static void
initialize_tracing_key(void)
{
	pthread_key_create(&g_enable_tracing_key, NULL);
}

static int
is_main_thread(void)
{
#if defined(__APPLE__) || defined(__FreeBSD__)
	return pthread_main_np();
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
	int old_trace_state;
	FILE *config_file = NULL;
	char *file_path;
	rtr_fopen_t real_fopen;
	rtr_malloc_t real_malloc;
	rtr_free_t real_free;
	rtr_getenv_t real_getenv;

	if (!get_tracing_enabled())
		return NULL;

	old_trace_state = trace_disable();

	real_fopen	= RETRACE_GET_REAL(fopen);
	real_malloc	= RETRACE_GET_REAL(malloc);
	real_free	= RETRACE_GET_REAL(free);
	real_getenv = RETRACE_GET_REAL(getenv);


	/* If we have a RETRACE_CONFIG env var, try to open the config file from there. */
	file_path = getenv("RETRACE_CONFIG");

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

	trace_restore(old_trace_state);

	return config_file;
}

static int
rtr_parse_config_file(FILE *config_file, const char *function, va_list arg_types)
{
	int retval = 0;
	int old_trace_state;
	size_t len;
	size_t line_size = 0;
	char *config_line = NULL;
	char *current_function = NULL;
	char *arg_start = NULL;
	rtr_strncmp_t real_strncmp;
	rtr_free_t real_free;

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

	real_strncmp = RETRACE_GET_REAL(strncmp);
	real_free = RETRACE_GET_REAL(free);

	if (!config_file)
		goto cleanup;

	len = strlen(function);

	while (getline(&config_line, &line_size, config_file) != -1) {
		char *function_end = strchr(config_line, ',');

		if (function_end) {
			*function_end = '\0';

			if (real_strncmp(function, config_line, len) == 0) {
				arg_start = function_end + 1;
				retval = 1;
				break;
			}
		}

		real_free(config_line);
		config_line = NULL;
	}

	if (current_function)
		real_free(current_function);

	if (arg_start) {
		/* Count how many arguments we have */
		va_list arg_values;
		int     current_argument;

		__va_copy(arg_values, arg_types);

		/* Advance past the types until we find the values */
		do {
			current_argument = va_arg(arg_values, int);
		} while (current_argument != ARGUMENT_TYPE_END);

		/* Now start filling the requests */
		for (current_argument = va_arg(arg_types, int);
			 current_argument != ARGUMENT_TYPE_END;
			 current_argument = va_arg(arg_types, int)) {
			void *current_value = va_arg(arg_values, void *);
			char *arg_end;

			arg_end = strchr(arg_start, ',');

			if (arg_end)
				*arg_end = '\0';
			else {
				/* skip the newline for the last string argument */
				if (arg_start && strlen(arg_start) &&
					arg_start[strlen(arg_start) - 1] == '\n')
					arg_start[strlen(arg_start) - 1] = '\0';
			}

			switch (current_argument) {
			case ARGUMENT_TYPE_INT:
				*((int *)current_value) = atoi(arg_start);
				break;
			case ARGUMENT_TYPE_STRING:
				*((char **)current_value) = strdup(arg_start);
				break;
			}

			if (arg_end != NULL)
				arg_start = arg_end + 1;
			else
				break;
		}

		va_end(arg_types);
		va_end(arg_values);
	}

cleanup:
	if (config_line)
		real_free(config_line);

	/* Restore tracing */
	trace_restore(old_trace_state);

	return retval;
}

void
rtr_confing_close(FILE *config)
{
	fclose(config);
}

int rtr_get_config_multiple(FILE **config, const char *function, ...)
{
	int ret = 0;

	if (*config == NULL)
		*config = get_config_file();

	if (*config) {
		va_list args;

		va_start(args, function);

		ret = rtr_parse_config_file(*config, function, args);

		if (!ret) {
			rtr_confing_close(*config);
			*config = NULL;
		}
	}

	return ret;
}

int rtr_get_config_single(const char *function, ...)
{
	int ret = 0;
	FILE *config_file;

	config_file = get_config_file();

	if (config_file) {
		va_list args;

		va_start(args, function);

		ret = rtr_parse_config_file(config_file, function, args);

		rtr_confing_close(config_file);
	}

	return ret;
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
