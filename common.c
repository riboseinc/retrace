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
trace_printf(int hdr, char *buf, ...) {
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

    if (str != NULL)
        fprintf(stderr, "%s", str);
    else
        fprintf(stderr, "%s(NULL)%s\n", VAR, RST);

    va_end(arglist);
}

void
trace_printf_str(const char *string) {
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
get_tracing_enabled() {
	return g_enable_tracing;
}

int
set_tracing_enabled(int enabled) {
	int oldvalue = g_enable_tracing;

	g_enable_tracing = enabled;

	return oldvalue;
}

int
get_redirect (const char *function, ...) {
	FILE *config_file;
	size_t line_size = 0;
	char *config_line = NULL;
	char *current_function = NULL;
	char *arg_start = NULL;
	size_t len;
	int retval = 0;

	// If we disabled tracing because we are
	// executing some internal code, don't honor
	// any redirections
	if (!get_tracing_enabled())
		return 0;

	// Disable tracing so we don't get in loops
	// when the functions we called here, call
	// Other functions that we have replaced
	int old_tracing_enabled = set_tracing_enabled (0);

	real_fopen = dlsym(RTLD_NEXT, "fopen");

	config_file = real_fopen ("/etc/retrace.conf", "r");

	if (!config_file)
		goto Cleanup;

	len = strlen (function);

	while (getline(&config_line, &line_size, config_file) != -1) {
                char *function_end = strchr (config_line, ',');

		if (function_end)
		{
			*function_end = '\0';

			if (strncmp(function, config_line, len) == 0) {
				arg_start = function_end + 1;
				retval = 1;
				break;
			}
		}

		free (config_line);
		config_line = NULL;
	}

	fclose (config_file);

	if (current_function)
		free (current_function);

	if (arg_start) {
		// Count how many arguments we have
		va_list arg_types;
		va_list arg_values;
		int current_argument;

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

			arg_end = strchr (arg_start, ',');

			if (arg_end)
				*arg_end = '\0';

			switch (current_argument) {
				case ARGUMENT_TYPE_INT:
					*((int *) current_value) = atoi (arg_start);
					break;
				case ARGUMENT_TYPE_STRING:
					*((char **) current_value) = strdup (arg_start);
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
		free (config_line);

	// Restore tracing
	set_tracing_enabled (old_tracing_enabled);

	return retval;
}

