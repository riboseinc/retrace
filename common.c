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

void
trace_printf(int hdr, const char *fmt, ...)
{
	if (!get_tracing_enabled())
		return;

	char str[1024];

	if (hdr == 1) {
		rtr_getpid_t getpid_ = dlsym(RTLD_NEXT, "getpid");
		fprintf(stderr, "(%d) ", getpid_());
	}

	va_list arglist;
	va_start(arglist, fmt);
	vsnprintf(str, sizeof(str), fmt, arglist);
	va_end(arglist);

	fputs(str, stderr);
}

void
trace_printf_str(const char *string)
{
	if (!get_tracing_enabled() || *string == '\0')
		return;

	static const char CR[] = VAR "\\r" RST;
	static const char LF[] = VAR "\\n" RST;
	static const char TAB[] = VAR "\\t" RST;
	static const char SNIP[] = "[SNIP]";

	char buf[MAXLEN * (sizeof(CR)-1) + sizeof(SNIP)];
	int i;
	char *p;
	rtr_strcpy_t strcpy_ = dlsym(RTLD_NEXT, "strcpy");

	for (i = 0, p = buf; i < MAXLEN && string[i] != '\0'; i++) {
		if (string[i] == '\n')
			strcpy_(p, LF);
		else if (string[i] == '\r')
			strcpy_(p, CR);
		else if (string[i] == '\t')
			strcpy_(p, TAB);
		else {
			*(p++) = string[i];
			*p = '\0';
		}
		while (*p)
			++p;
	}
	if (string[i] != '\0')
		strcpy_(p, SNIP);
	
	trace_printf(0, buf);
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
