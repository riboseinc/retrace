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

#include "common.h"
#include "str.h"
#include "id.h"
#include "file.h"
#include "malloc.h"
#include "printf.h"

/*************************************************
 *  Global setting, we set this to disable all our
 *  internal tracing when we are doing some internal
 *  operations like reading config files, so we don't
 *  confuse the program with calls that we ourselves
 *  introduced. Default to enable
 *
 * This also fixes some looping issues. Image we are
 * faking a call to getuid, this calls get_redirect to
 * check if theres a redirect active. get_redirect calls
 * fopen, which in its internal implementation also
 * calls into your tapped version of getuid and we
 * get into an infinite loop.
 *
 **************************************************/
int g_enable_tracing = 1;

/*************************************************
 * Global list of file descriptors so we can track
 * their usage across different functions
 *
 **************************************************/
#define DESCRIPTOR_LIST_INITIAL_SIZE 8
descriptor_info_t **g_descriptor_list = NULL;
unsigned int	g_descriptor_list_size = 0;

void
trace_printf(int hdr, const char *fmt, ...)
{
	if (!get_tracing_enabled())
		return;

	int old_tracing_enabled = set_tracing_enabled(0);

	real_vsnprintf = RETRACE_GET_REAL(vsnprintf);
	real_fprintf = RETRACE_GET_REAL(fprintf);
	real_getpid = RETRACE_GET_REAL(getpid);

	char str[1024];

	va_list arglist;
	va_start(arglist, buf);

	memset(str, 0, sizeof(str));

	real_vsnprintf(str, sizeof(str), buf, arglist);

	str[sizeof(str) - 1] = '\0';

	if (hdr == 1)
		real_fprintf(stderr, "(%d) ", real_getpid());

	real_fprintf(stderr, "%s", str);
	va_list arglist;
	va_start(arglist, fmt);
	rtr_vsnprintf_t vsnprintf_ = RETRACE_GET_REAL(vsnprintf);
	vsnprintf_(str, sizeof(str), fmt, arglist);
	va_end(arglist);

	fputs(str, stderr);

	set_tracing_enabled(old_tracing_enabled);
}

void
trace_printf_str(const char *string)
{
	static const char CR[] = VAR "\\r" RST;
	static const char LF[] = VAR "\\n" RST;
	static const char TAB[] = VAR "\\t" RST;
	static const char SNIP[] = "[SNIP]";

	if (!get_tracing_enabled() || *string == '\0')
		return;

	int old_tracing_enabled = set_tracing_enabled(0);

	char buf[MAXLEN * (sizeof(CR)-1) + sizeof(SNIP)];
	int i;
	char *p;
	rtr_strcpy_t strcpy_ = RETRACE_GET_REAL(strcpy);

	for (i = 0, p = buf; i < MAXLEN && string[i] != '\0'; i++) {
		if (string[i] == '\n')
			strcpy_(p, LF);
		else if (string[i] == '\r')
			strcpy_(p, CR);
		else if (string[i] == '\t')
			strcpy_(p, TAB);
		else if (string[i] == '%')
			strcpy_(p, "%%");
		else {
			*(p++) = string[i];
			*p = '\0';
		}
		while (*p)
			++p;
	}
	if (string[i] != '\0')
		strcpy_(p, SNIP);

	set_tracing_enabled(old_tracing_enabled);

	trace_printf(0, buf);
}

void
trace_dump_data(const void *buf, size_t nbytes)
{
#define DUMP_LINE_SIZE 20
	int  i;
	int  print_newline = 0;
	char current_string[DUMP_LINE_SIZE + 1];

	for (i = 0; i < nbytes; i++) {
		if (i % DUMP_LINE_SIZE == 0) {
			if (print_newline) {
				trace_printf(0, " | %s\n", current_string);
			}

			memset(current_string, '\0', DUMP_LINE_SIZE);
			trace_printf(0, "\t%07u\t", i / 2);
		}

		if (i % 2 == 0) {
			trace_printf(0, " ");
		}

		unsigned char c = ((unsigned char *) buf)[i];

		print_newline = 1;
		trace_printf(0, "%02x", c);

		// Only print ASCII characters
		if (c > 31 && c < 128)
			current_string[i % 20] = c;
		else
			current_string[i % 20] = '.';
	}

	if (print_newline) {
		trace_printf(0, " | %s\n", current_string);
	}
}

int
get_tracing_enabled()
{
	return g_enable_tracing;
}

int
set_tracing_enabled(int enabled)
{
	int oldvalue = g_enable_tracing;

	g_enable_tracing = enabled;

	return oldvalue;
}

static FILE *
get_config_file ()
{
	FILE *config_file = NULL;

	if (!get_tracing_enabled())
        	return NULL;

	int old_tracing_enabled = set_tracing_enabled(0);

	real_fopen = RETRACE_GET_REAL(fopen);

  	// If we have a RETRACE_CONFIG env var, try to open the config file
	// from there
	char *file_path = getenv("RETRACE_CONFIG");

	if (file_path)
		config_file = real_fopen(file_path, "r");

	// If we couldn't open the file from the env var try to home it from ~/.retrace.conf
	if (!config_file) {
		struct passwd *pw = getpwuid(getuid());

		if (pw && pw->pw_dir) {
			char *file_name_user = ".retrace.conf";
			char *file_path_user =
			  (char *) malloc(strlen(pw->pw_dir) + strlen(file_name_user) + 2);

			if (file_path_user) {
				strcpy(file_path_user, pw->pw_dir);
				strcat(file_path_user, "/");
				strcat(file_path_user, file_name_user);

				config_file = real_fopen(file_path_user, "r");

				free(file_path_user);
			}
		}
	}

	// Finally if the above failed try to open /etc/retrace.conf
	if (!config_file) {
		config_file = real_fopen("/etc/retrace.conf", "r");
	}

	set_tracing_enabled(old_tracing_enabled);

	return config_file;
}



static int
rtr_parse_config_file(rtr_config config_file, const char *function, va_list arg_types)
{
	size_t line_size = 0;
	char * config_line = NULL;
	char * current_function = NULL;
	char * arg_start = NULL;
	size_t len;
	int    retval = 0;

	// If we disabled tracing because we are
	// executing some internal code, don't honor
	// any redirections
	if (!get_tracing_enabled())
		return 0;

	// Disable tracing so we don't get in loops
	// when the functions we called here, call
	// Other functions that we have replaced
	int old_tracing_enabled = set_tracing_enabled(0);

	real_strncmp = RETRACE_GET_REAL(strncmp);
	real_free = RETRACE_GET_REAL(free);

	if (!config_file)
		goto Cleanup;

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
		free(current_function);

	if (arg_start) {
		// Count how many arguments we have
		va_list arg_values;
		int     current_argument;

		va_copy (arg_values, arg_types);

		// Advance past the types until we find the values
		do {
			current_argument = va_arg(arg_values, int);
		} while (current_argument != ARGUMENT_TYPE_END);

		// Now start filling the requests
		for (current_argument = va_arg(arg_types, int);
		     current_argument != ARGUMENT_TYPE_END;
		     current_argument = va_arg(arg_types, int)) {
			void *current_value = va_arg(arg_values, void *);
			char *arg_end;

			arg_end = strchr(arg_start, ',');

			if (arg_end)
				*arg_end = '\0';
			else {
				// skip the newline for the last string argument
				if (arg_start && strlen(arg_start) &&
				    arg_start[strlen(arg_start) - 1] == '\n')
					arg_start[strlen(arg_start) - 1] = '\0';
			}

			switch (current_argument) {
			case ARGUMENT_TYPE_INT:
				*((int *) current_value) = atoi(arg_start);
				break;
			case ARGUMENT_TYPE_STRING:
				*((char **) current_value) = strdup(arg_start);
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

Cleanup:
	if (config_line)
		real_free(config_line);

	// Restore tracing
	set_tracing_enabled(old_tracing_enabled);

	return retval;
}

void
rtr_confing_close (rtr_config config)
{
	fclose ((FILE *) config); 
}

int rtr_get_config_multiple(rtr_config *config, const char *function, ...)
{
	int ret = 0;

	if (*config == NULL)
		*config = get_config_file ();

	if (*config) {
		va_list args;
		va_start (args, function);
		ret = rtr_parse_config_file (*config, function, args);

		if (!ret) {
			rtr_confing_close (*config);
			*config = NULL;
		}
        }

	return ret;
}

int rtr_get_config_single(const char *function, ...)
{
	rtr_config config_file = get_config_file ();
	int ret = 0;

	if (config_file) {
		va_list args;
                va_start (args, function);
		ret = rtr_parse_config_file (config_file, function, args);

		rtr_confing_close (config_file);
	}

	return ret;
}


descriptor_info_t *
descriptor_info_new(int fd, unsigned int type, const char *location, int port)
{
	descriptor_info_t *di;

	int old_tracing_enabled = set_tracing_enabled(0);

	di = (descriptor_info_t *) malloc(sizeof(descriptor_info_t));

	if (di) {
		di->fd = fd;
		di->type = type;

		if (location)
			di->location = strdup(location);
		else
			di->location = NULL;

		di->port = port;
	}

	set_tracing_enabled(old_tracing_enabled);

	return di;
}

void
descriptor_info_free(descriptor_info_t *di)
{
	int old_tracing_enabled = set_tracing_enabled(0);

	if (di->location)
		free(di->location);

	free(di);

	set_tracing_enabled(old_tracing_enabled);
}

void
file_descriptor_add(int fd, unsigned int type, const char *location, int port)
{
	int free_spot = -1;
	int i = 0;

	int old_tracing_enabled = set_tracing_enabled(0);

	descriptor_info_t *di = descriptor_info_new(fd, type, location, port);

	if (!di) {
		return;
		set_tracing_enabled(old_tracing_enabled);
	}

	if (g_descriptor_list == NULL) {
		g_descriptor_list_size = DESCRIPTOR_LIST_INITIAL_SIZE;
		g_descriptor_list = (descriptor_info_t **) malloc(
		  DESCRIPTOR_LIST_INITIAL_SIZE * sizeof(descriptor_info_t *));

		memset(g_descriptor_list,
		       0,
		       DESCRIPTOR_LIST_INITIAL_SIZE * sizeof(descriptor_info_t *));
	}

	for (i = 0; i < g_descriptor_list_size; i++) {
		if (g_descriptor_list[i] == NULL) {
			free_spot = i;
			break;
		}
	}

	if (free_spot == -1) {
		// If we don't have any free spots double the list size
		int new_size = g_descriptor_list_size * 2;

		g_descriptor_list = (descriptor_info_t **) realloc(
		  g_descriptor_list, new_size * sizeof(descriptor_info_t *));

		if (g_descriptor_list) {
			// clear the new spots we added
			memset(g_descriptor_list + g_descriptor_list_size,
			       0,
			       new_size - g_descriptor_list_size);

			// Insert at the end of old list
			free_spot = g_descriptor_list_size;

			g_descriptor_list_size = new_size;
		}
	}

	if (free_spot != -1) {
		g_descriptor_list[free_spot] = di;
	}

	set_tracing_enabled(old_tracing_enabled);
}

descriptor_info_t *
file_descriptor_get(int fd)
{
	int i;
	for (i = 0; i < g_descriptor_list_size; i++) {
		if (g_descriptor_list[i] && g_descriptor_list[i]->fd == fd)
			return g_descriptor_list[i];
	}

	return NULL;
}

void
file_descriptor_update(int fd, unsigned int type, const char *location, int port)
{
	descriptor_info_t *di = file_descriptor_get(fd);

	/* If found, update */
	if (di) {
		di->type = type;
		if (di->location)
			free(di->location);

		di->port = port;
	} else {
		/* If not found, add it */
		file_descriptor_add(fd, type, location, port);
	}
}

void
file_descriptor_remove(int fd)
{
	int i;

	for (i = 0; i < g_descriptor_list_size; i++) {
		if (g_descriptor_list[i] && g_descriptor_list[i]->fd == fd) {
			descriptor_info_free(g_descriptor_list[i]);
			g_descriptor_list[i] = NULL;
		}
	}
}

/* lightweight copy of strmode() from FreeBSD for displaying mode_t in chmod
 */
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
