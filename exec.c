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

#include "common.h"
#include "exec.h"
#include <unistd.h>

int
RETRACE_IMPLEMENTATION(system)(const char *command)
{
	real_system = dlsym(RTLD_NEXT, "system");
	trace_printf(1, "system(\"%s\");\n", command);
	return real_system(command);
}

RETRACE_REPLACE (system)

int
RETRACE_IMPLEMENTATION(execve)(const char *path, char *const argv[], char *const envp[])
{
	real_execve = dlsym(RTLD_NEXT, "execve");

	int i;

	trace_printf(1, "execve(\"%s\"", argv[0]);

	for (i = 0;; i++) {
		if (argv[i] == NULL)
			break;

		trace_printf(0, ", \"%s\"", argv[i]);
	}

	trace_printf(0, ", envp);\n");

	trace_printf(1, "char *envp[]=\n");
	trace_printf(1, "{\n");

	for (i = 0;; i++) {
		if (envp[i] == NULL)
			break;

		trace_printf(1, "\t\"%s\",\n", envp[i]);
	}
	trace_printf(1, "\t0\n");
	trace_printf(1, "}\n");

	return real_execve(path, argv, envp);

	return(0);
}

RETRACE_REPLACE (execve)

