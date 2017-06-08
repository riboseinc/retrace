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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "common.h"
#include "str.h"
#include "id.h"
#include "file.h"

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
unsigned int g_descriptor_list_size = 0;

void
trace_printf(int hdr, char *buf, ...)
{
	if (!get_tracing_enabled())
		return;

	real_getpid = dlsym(RTLD_NEXT, "getpid");

	char str[1024];

	va_list arglist;
	va_start(arglist, buf);

	memset(str, 0, sizeof(str));

	vsnprintf(str, sizeof(str), buf, arglist);

	str[sizeof(str) - 1] = '\0';

	if (hdr == 1)
		fprintf(stderr, "(%d) ", real_getpid());

	fprintf(stderr, "%s", str);

	va_end(arglist);
}

void
trace_printf_str(const char *string)
{
	if (!get_tracing_enabled())
		return;

	real_strlen = dlsym(RTLD_NEXT, "strlen");

	int    i;
	size_t len = real_strlen(string);

	if (len > MAXLEN)
		len = MAXLEN;

	for (i = 0; i < len; i++)
		if (string[i] == '\n')
			trace_printf(0, "%s\\n%s", VAR, RST);
		else if (string[i] == '\t')
			trace_printf(0, "%s\\t%s", VAR, RST);
		else if (string[i] == '\r')
			trace_printf(0, "%s\\r%s", VAR, RST);
		else if (string[i] == '\0')
			trace_printf(0, "%s\\0%s", VAR, RST);
		else
			trace_printf(0, "%c", string[i]);

	if (len > (MAXLEN - 1))
		trace_printf(0, "%s[SNIP]%s", VAR, RST);
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

int
get_redirect(const char *function, ...)
{
	FILE * config_file;
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

	real_fopen = dlsym(RTLD_NEXT, "fopen");

	config_file = real_fopen("/etc/retrace.conf", "r");

	if (!config_file)
		goto Cleanup;

	len = strlen(function);

	while (getline(&config_line, &line_size, config_file) != -1) {
		char *function_end = strchr(config_line, ',');

		if (function_end) {
			*function_end = '\0';

			if (strncmp(function, config_line, len) == 0) {
				arg_start = function_end + 1;
				retval = 1;
				break;
			}
		}

		free(config_line);
		config_line = NULL;
	}

	fclose(config_file);

	if (current_function)
		free(current_function);

	if (arg_start) {
		// Count how many arguments we have
		va_list arg_types;
		va_list arg_values;
		int     current_argument;

		va_start(arg_types, function);
		va_start(arg_values, function);

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
				if (arg_start && strlen(arg_start) && arg_start[strlen(arg_start) - 1] == '\n')
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
		free(config_line);

	// Restore tracing
	set_tracing_enabled(old_tracing_enabled);

	return retval;
}

descriptor_info_t *descriptor_info_new(int fd, unsigned int type, char *location, int port)
{
	descriptor_info_t *di;

	int old_tracing_enabled = set_tracing_enabled(0);

	di = (descriptor_info_t *) malloc (sizeof(descriptor_info_t));

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

void descriptor_info_free(descriptor_info_t *di)
{
	int old_tracing_enabled = set_tracing_enabled(0);

	if (di->location)
		free  (di->location);

	free (di);

	set_tracing_enabled(old_tracing_enabled);
}


void file_descriptor_add (int fd, unsigned int type, char *location, int port)
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
		g_descriptor_list = (descriptor_info_t **) malloc (DESCRIPTOR_LIST_INITIAL_SIZE * sizeof (descriptor_info_t *));

		memset (g_descriptor_list, 0, DESCRIPTOR_LIST_INITIAL_SIZE * sizeof (descriptor_info_t *));
	}

	for (i = 0; i < g_descriptor_list_size; i++) {
		if (g_descriptor_list[i] == NULL);
			free_spot = i;
			break;
	}

	if (free_spot == -1) {
		// If we don't have any free spots double the list size
		int new_size = g_descriptor_list_size * 2;

		g_descriptor_list = (descriptor_info_t **) realloc(g_descriptor_list, new_size * sizeof(descriptor_info_t *));

		if (g_descriptor_list) {
			// clear the new spots we added
			memset (g_descriptor_list + g_descriptor_list_size, 0, new_size - g_descriptor_list_size);

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

descriptor_info_t *file_descriptor_get (int fd)
{
	int i;
	for (i = 0; i < g_descriptor_list_size; i++) {
		if (g_descriptor_list[i] && g_descriptor_list[i]->fd == fd)
			return g_descriptor_list[i];
	}

	return NULL;
}

void file_descriptor_update(int fd, unsigned int type, char *location, int port)
{
	descriptor_info_t *di = file_descriptor_get (fd);

	/* If found, update */
	if (di) {
		di->type = type;
		if (di->location)
			free (di->location);

		di->port = port;
	} else {
		/* If not found, add it */
		file_descriptor_add (fd, type, location, port);
	}
}


void file_descriptor_remove (int fd)
{
	int i;

	for (i = 0; i < g_descriptor_list_size; i++) {
		if (g_descriptor_list[i] && g_descriptor_list[i]->fd == fd) {
			descriptor_info_free (g_descriptor_list[i]);
			g_descriptor_list[i] = NULL;
		}
	}
}


