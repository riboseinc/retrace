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
trace_printf(int hdr, char *buf, ...)
{
	if (!get_tracing_enabled())
		return;

	real_getpid = rtr_dlsym(rtr_getpid);

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

	real_strlen = rtr_dlsym(rtr_strlen);

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

	real_fopen = rtr_dlsym(rtr_fopen);

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

void *
rtr_dlsym(rtr_func_id id)
{
    static void * func_ptrs[rtr_end_func];

    if (func_ptrs[id] == NULL) {
        func_ptrs[rtr_accept] = dlsym(RTLD_NEXT, "accept");
        func_ptrs[rtr_atoi] = dlsym(RTLD_NEXT, "atoi");
        func_ptrs[rtr_bind] = dlsym(RTLD_NEXT, "bind");
        func_ptrs[rtr_chmod] = dlsym(RTLD_NEXT, "chmod");
        func_ptrs[rtr_close] = dlsym(RTLD_NEXT, "close");
        func_ptrs[rtr_closedir] = dlsym(RTLD_NEXT, "closedir");
        func_ptrs[rtr_connect] = dlsym(RTLD_NEXT, "connect");
        func_ptrs[rtr_ctime] = dlsym(RTLD_NEXT, "ctime");
        func_ptrs[rtr_ctime_r] = dlsym(RTLD_NEXT, "ctime_r");
        func_ptrs[rtr_dup2] = dlsym(RTLD_NEXT, "dup2");
        func_ptrs[rtr_dup] = dlsym(RTLD_NEXT, "dup");
        func_ptrs[rtr_execve] = dlsym(RTLD_NEXT, "execve");
        func_ptrs[rtr_exit] = dlsym(RTLD_NEXT, "exit");
        func_ptrs[rtr_fchmod] = dlsym(RTLD_NEXT, "fchmod");
        func_ptrs[rtr_fclose] = dlsym(RTLD_NEXT, "fclose");
        func_ptrs[rtr_fileno] = dlsym(RTLD_NEXT, "fileno");
        func_ptrs[rtr_fopen] = dlsym(RTLD_NEXT, "fopen");
        func_ptrs[rtr_fork] = dlsym(RTLD_NEXT, "fork");
        func_ptrs[rtr_free] = dlsym(RTLD_NEXT, "free");
        func_ptrs[rtr_fseek] = dlsym(RTLD_NEXT, "fseek");
        func_ptrs[rtr_getegid] = dlsym(RTLD_NEXT, "getegid");
        func_ptrs[rtr_getenv] = dlsym(RTLD_NEXT, "getenv");
        func_ptrs[rtr_geteuid] = dlsym(RTLD_NEXT, "geteuid");
        func_ptrs[rtr_getgid] = dlsym(RTLD_NEXT, "getgid");
        func_ptrs[rtr_getpid] = dlsym(RTLD_NEXT, "getpid");
        func_ptrs[rtr_getppid] = dlsym(RTLD_NEXT, "getppid");
        func_ptrs[rtr_getuid] = dlsym(RTLD_NEXT, "getuid");
        func_ptrs[rtr_malloc] = dlsym(RTLD_NEXT, "malloc");
        func_ptrs[rtr_opendir] = dlsym(RTLD_NEXT, "opendir");
        func_ptrs[rtr_pclose] = dlsym(RTLD_NEXT, "pclose");
        func_ptrs[rtr_perror] = dlsym(RTLD_NEXT, "perror");
        func_ptrs[rtr_pipe2] = dlsym(RTLD_NEXT, "pipe2");
        func_ptrs[rtr_pipe] = dlsym(RTLD_NEXT, "pipe");
        func_ptrs[rtr_popen] = dlsym(RTLD_NEXT, "popen");
        func_ptrs[rtr_putenv] = dlsym(RTLD_NEXT, "putenv");
        func_ptrs[rtr_read] = dlsym(RTLD_NEXT, "read");
        func_ptrs[rtr_seteuid] = dlsym(RTLD_NEXT, "seteuid");
        func_ptrs[rtr_setgid] = dlsym(RTLD_NEXT, "setgid");
        func_ptrs[rtr_setuid] = dlsym(RTLD_NEXT, "setuid");
        func_ptrs[rtr_stat] = dlsym(RTLD_NEXT, "stat");
        func_ptrs[rtr_strcat] = dlsym(RTLD_NEXT, "strcat");
        func_ptrs[rtr_strcmp] = dlsym(RTLD_NEXT, "strcmp");
        func_ptrs[rtr_strcpy] = dlsym(RTLD_NEXT, "strcpy");
        func_ptrs[rtr_strlen] = dlsym(RTLD_NEXT, "strlen");
        func_ptrs[rtr_strncat] = dlsym(RTLD_NEXT, "strncat");
        func_ptrs[rtr_strncmp] = dlsym(RTLD_NEXT, "strncmp");
        func_ptrs[rtr_strncpy] = dlsym(RTLD_NEXT, "strncpy");
        func_ptrs[rtr_strstr] = dlsym(RTLD_NEXT, "strstr");
        func_ptrs[rtr_system] = dlsym(RTLD_NEXT, "system");
        func_ptrs[rtr_tolower] = dlsym(RTLD_NEXT, "tolower");
        func_ptrs[rtr_toupper] = dlsym(RTLD_NEXT, "toupper");
        func_ptrs[rtr_unsetenv] = dlsym(RTLD_NEXT, "unsetenv");
        func_ptrs[rtr_write] = dlsym(RTLD_NEXT, "write");
    }
    return func_ptrs[id];
}
