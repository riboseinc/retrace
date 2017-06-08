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
RETRACE_IMPLEMENTATION(execl)(const char *path, const char *arg0, ... /*, (char *)0 */)
{
	real_execv = dlsym(RTLD_NEXT, "execv");

	trace_printf(1, "execl(\"%s\"", path);

	va_list arglist;
	va_start(arglist, arg0);

	char *p = NULL;
	int i = 1;
	while ((p = va_arg(arglist, char *)) != NULL)
		i++;

	const char **argv = malloc(i * sizeof (char *));

	i = 0;
	argv[i] = arg0;

	va_start (arglist, arg0);

	while ((p = va_arg(arglist, char *)) != NULL) {
		argv[++i] = p;

		trace_printf(0, ", \"%s\"", p);
	}

	trace_printf(0, ");\n");

	va_end(arglist);

	int retVal = real_execv(path, (char * const *)argv);

	free(argv);

	return retVal;
}

RETRACE_IMPLEMENTATION(execl)

int
RETRACE_IMPLEMENTATION(execv)(const char *path, char *const argv[])
{
	real_execv = dlsym(RTLD_NEXT, "execv");

	int i;

	trace_printf(1, "execv(\"%s\"", path);

	for (i = 0;; i++) {
		if (argv[i] == NULL)
			break;

		trace_printf(0, ", \"%s\"", argv[i]);
	}

	trace_printf(0, ");\n");

	return real_execv(path, argv);
}

RETRACE_IMPLEMENTATION(execv)

int
RETRACE_IMPLEMENTATION(execle)(const char *path, const char *arg0, ... /*, (char *)0, char *const envp[]*/)
{
	real_execve = dlsym(RTLD_NEXT, "execve");

	trace_printf(1, "execle(\"%s\"", path);

	va_list arglist;
	va_start(arglist, arg0);

	char *p = NULL;
	int i = 1;
	while ((p = va_arg(arglist, char *)) != NULL)
		i++;

	const char **argv = malloc(i * sizeof (char *));

	i = 0;
	argv[i] = arg0;

	va_start (arglist, arg0);

	while ((p = va_arg(arglist, char *)) != NULL) {
		argv[++i] = p;

		trace_printf(0, ", \"%s\"", p);
	}

	char * const *envp = va_arg(arglist, char **);

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

	va_end(arglist);

	int retVal = real_execve(path, (char * const *)argv, envp);

	free(argv);

	return retVal;
}

RETRACE_IMPLEMENTATION(execle)

int
RETRACE_IMPLEMENTATION(execve)(const char *path, char *const argv[], char *const envp[])
{
	real_execve = dlsym(RTLD_NEXT, "execve");

	int i;

	trace_printf(1, "execve(\"%s\"", path);

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
}

RETRACE_IMPLEMENTATION(execve)

int
RETRACE_IMPLEMENTATION(execlp)(const char *file, const char *arg0, ... /*, (char *)0 */)
{
	real_execvp = dlsym(RTLD_NEXT, "execvp");

	trace_printf(1, "execlp(\"%s\"", file);

	va_list arglist;
	va_start(arglist, arg0);

	char *p = NULL;
	int i = 1;
	while ((p = va_arg(arglist, char *)) != NULL)
		i++;

	const char **argv = malloc(i * sizeof (char *));

	i = 0;
	argv[i] = arg0;

	va_start (arglist, arg0);

	while ((p = va_arg(arglist, char *)) != NULL) {
		argv[++i] = p;

		trace_printf(0, ", \"%s\"", p);
	}

	trace_printf(0, ");\n");

	va_end(arglist);

	int retVal = real_execvp(file, (char * const *)argv);

	free(argv);

	return retVal;
}

RETRACE_IMPLEMENTATION(execlp)

int
RETRACE_IMPLEMENTATION(execvp)(const char *file, char *const argv[])
{
	real_execvp = dlsym(RTLD_NEXT, "execvp");

	int i;

	trace_printf(1, "execvp(\"%s\"", file);

	for (i = 0;; i++) {
		if (argv[i] == NULL)
			break;

		trace_printf(0, ", \"%s\"", argv[i]);
	}

	trace_printf(0, ");\n");

	return real_execvp(file, argv);
}

RETRACE_IMPLEMENTATION(execvp)

int
RETRACE_IMPLEMENTATION(execvpe)(const char *file, char *const argv[], char *const envp[])
{
	real_execvpe = dlsym(RTLD_NEXT, "execvpe");

	int i;

	trace_printf(1, "execvpe(\"%s\"", file);

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

	return real_execvpe(file, argv, envp);
}

RETRACE_IMPLEMENTATION(execvpe)

int
RETRACE_IMPLEMENTATION(execveat)(int dirfd, const char *pathname, char *const argv[], char *const envp[], int flags)
{
	real_execveat = dlsym(RTLD_NEXT, "execveat");

	int i;

	trace_printf(1, "execveat(%d, \"%s\"", dirfd, pathname);

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
	trace_printf(1, "%d\n", flags);

	return real_execveat(dirfd, pathname, argv, envp, flags);
}

RETRACE_IMPLEMENTATION(execveat)

int
RETRACE_IMPLEMENTATION(fexecve)(int fd, char *const argv[], char *const envp[])
{
	real_fexecve = dlsym(RTLD_NEXT, "fexecve");

	int i;

	trace_printf(1, "fexecve(%d", fd);

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


	return fexecve(fd, argv, envp);
}

RETRACE_IMPLEMENTATION(fexecve)
