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
#include <sys/queue.h>

#include "frontend.h"
#include "handlers.h"
#include "display.h"
#include "tracefd.h"
#if BACKTRACE
#include "backtrace.h"
#endif

static struct option options[] = {
	{"help", no_argument, 0, 'h'},
	{"version", no_argument, 0, 'v'},
	{"functions", required_argument, 0, 'f'},
#if BACKTRACE
	{"backtrace-functions", required_argument, 0, 'b'},
	{"backtrace-depth", required_argument, 0, 'd'},
#endif
	{"show-buffers", required_argument, 0, 'u'},
	{"show-strings", required_argument, 0, 's'},
	{"show-structs", no_argument, 0, 't'},
	{"trace-fds", no_argument, 0, 'F'},
	{NULL, 0, 0, 0}
};

#define STDOPTS "+f:s:u:hvFt"

#if BACKTRACE
#define BTOPTS "b:d:"
#else
#define BTOPTS
#endif

static void usage(const char *argv0, int exitval)
{
	FILE *stream = exitval == EXIT_SUCCESS ? stdout : stderr;

	fprintf(stream, "Usage: %s [options] <executable>\n\n"
	    "Retrace is a versatile security vulnerability / bug discovery "
	    "tool through monitoring and modifying the behavior of compiled "
	    "binaries on Linux, OpenBSD/FreeBSD (shared object) and macOS "
	    "(dynamic library).\n\n"
	    "Retrace can be used to assist reverse engineering / debugging "
	    "dynamically-linked ELF (Linux/OpenBSD/FreeBSD) and Mach-O "
	    "(macOS) binary executables.\n\n"
	    "Options:\n"
	    "  -f --functions=LIST	LIST is a comma separated list of "
	    "function names to trace (defaults to all supported functions)\n"
	    "  -u --show-buffers=n	Show the first n bytes of "
	    "buffer parameters\n"
	    "  -s --show-strings=n	Show the first n characters of "
	    "string parameters\n"
	    "  -t --show-structs	Show additional information for "
	    "structs\n"
	    "  -b --backtrace-functions=LIST	LIST is a comma separated "
	    "list of function names for which to show a stach trace\n"
	    "  -d --backtrace-depth=n	Show n frames when displaying stack "
	    "traces (default 4)\n"
	    "  -F --trace-fds	Show extended information for FILE * and "
	    "file descriptor parameters\n"
	    "  -h --help	Show this help\n"
	    "  -v --version	Show version information\n", argv0);
	exit(exitval);
}

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
	struct handler_info handler_info;
	int i, c, opt_funcs = 0, trace_flags[RPC_FUNCTION_COUNT],
	    tracefds = 0;
#if BACKTRACE
	int backtrace_funcs[RPC_FUNCTION_COUNT];
#endif

	memset(trace_flags, 0, sizeof(trace_flags));
	memset(&handler_info, 0, sizeof(handler_info));

#if BACKTRACE
	memset(backtrace_funcs, 0, sizeof(backtrace_funcs));
	handler_info.backtrace_depth = 4;
#endif

	while (1) {
		c = getopt_long(argc, argv, STDOPTS BTOPTS, options, NULL);

		if (c == -1)
			break;

		switch (c) {
		case 'f':
			opt_funcs = 1;
			set_trace_flags(trace_flags, RETRACE_TRACE, optarg);
			break;
#if BACKTRACE
		case 'b':
			opt_funcs = 1;
			set_trace_flags(trace_flags, RETRACE_TRACE, optarg);
			set_trace_flags(backtrace_funcs, 0x01, optarg);
			break;
		case 'd':
			handler_info.backtrace_depth = atoi(optarg);
			break;
#endif
		case 'u':
			handler_info.expand_buffers = atoi(optarg);
			break;
		case 's':
			handler_info.expand_strings = atoi(optarg);
			break;
		case 'h':
			usage(argv[0], EXIT_SUCCESS);
			break;
		case 'v':
			printf("%s version %s\n", argv[0], PACKAGE_VERSION);
			exit(EXIT_SUCCESS);
			break;
		case 'F':
			tracefds = 1;
			break;
		case 't':
			handler_info.expand_structs = 1;
			break;
		default:
			usage(argv[0], EXIT_FAILURE);
		}
	}

	if (!opt_funcs)
		for (i = 0; i < RPC_FUNCTION_COUNT; i++)
			trace_flags[i] |= RETRACE_TRACE;

	trace_handle = retrace_start(&argv[optind], trace_flags);

	retrace_set_user_data(trace_handle, &handler_info);

	if (tracefds)
		init_tracefd_handlers(trace_handle);

	#if BACKTRACE
		init_backtrace_handlers(trace_handle, backtrace_funcs);
	#endif

	set_log_handlers(trace_handle);

	retrace_trace(trace_handle);

	retrace_close(trace_handle);

	return 0;
}
