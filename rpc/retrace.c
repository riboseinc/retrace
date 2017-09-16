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

#include "../config.h"

#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include "frontend.h"
#include "handlers.h"
#include "display.h"

#define OPT_FUNCS '0'
#define OPT_BTFUNCS '1'
#define OPT_BTDEPTH '2'
#define OPT_STRINGS '3'

static struct option options[] = {
	{"functions", required_argument, 0, OPT_FUNCS},
#if BACKTRACE
	{"backtrace-functions", required_argument, 0, OPT_BTFUNCS},
	{"backtrace-depth", required_argument, 0, OPT_BTDEPTH},
#endif
	{"show-strings", required_argument, 0, OPT_STRINGS},
	{NULL, 0, 0, 0}
};

static void set_trace_flags(int *flags, int setflag, char *funcs)
{
	char *p;
	enum retrace_function_id id;

	p = strdup(funcs);
	if (p == NULL)
		return;

	for (p = strtok(p, ","); p; p = strtok(NULL, ",")) {
		id = retrace_function_id(p);
		if (id != RPC_FUNCTION_COUNT)
			flags[id] |= setflag;
	}

	free(p);
}

int main(int argc, char **argv)
{
	struct retrace_handle *trace_handle;
	struct display_info display_info;
	int i, c, opt_funcs = 0,
	    trace_flags[RPC_FUNCTION_COUNT];
	retrace_precall_handler_t pre[RPC_FUNCTION_COUNT];
	retrace_postcall_handler_t post[RPC_FUNCTION_COUNT];

	memset(trace_flags, 0, sizeof(trace_flags));
	memset(&display_info, 0, sizeof(display_info));

#if BACKTRACE
	display_info.backtrace_depth = 10;
#endif

	while (1) {
		c = getopt_long(argc, argv, "+", options, NULL);

		if (c == -1)
			break;

		switch (c) {
		case OPT_FUNCS:
			opt_funcs = 1;
			set_trace_flags(trace_flags, RETRACE_TRACE, optarg);
			break;
#if BACKTRACE
		case OPT_BTFUNCS:
			opt_funcs = 1;
			set_trace_flags(trace_flags, RETRACE_TRACE, optarg);
			set_trace_flags(display_info.backtrace_functions, 0x01, optarg);
			break;
		case OPT_BTDEPTH:
			display_info.backtrace_depth = atoi(optarg);
			break;
#endif
		case OPT_STRINGS:
			display_info.expand_strings = atoi(optarg);
			break;
		default:
			fprintf(stderr, "got %c from optarg()", c);
			break;
		}
	}

	if (!opt_funcs)
		for (i = 0; i < RPC_FUNCTION_COUNT; i++)
			trace_flags[i] |= RETRACE_TRACE;

	trace_handle = retrace_start(&argv[optind], trace_flags);

	get_handlers(pre, post);

	retrace_set_handlers(trace_handle, pre, post);

	retrace_set_user_data(trace_handle, &display_info);

	retrace_trace(trace_handle);

	retrace_close(trace_handle);

	return 0;
}
