/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * One-shot capture implementation (TODO.trace-profile/03).
 * POSIX: fork/exec with LD_PRELOAD / DYLD_INSERT_LIBRARIES and
 * the logger env pointed at a trace file. Windows uses
 * retrace-win-run injection instead (docs/windows.md).
 */

#ifndef _WIN32

#include "capture.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>

#ifdef __APPLE__
static const char *const g_preload_vars[] = {
	"DYLD_INSERT_LIBRARIES", "LD_PRELOAD", NULL
};
static const char *const g_lib_exts[] = { "dylib", "so", NULL };
#else
static const char *const g_preload_vars[] = {
	"LD_PRELOAD", NULL
};
static const char *const g_lib_exts[] = { "so", NULL };
#endif

static int file_readable(const char *path)
{
	return access(path, R_OK) == 0;
}

const char *prof_capture_find_lib(void)
{
	static char buf[1024];
	const char *env = getenv("RETRACE_V2_LIB");
	const char *const dirs[] = {
		".", "../src/v2", "../../src/v2", "src/v2",
		"/usr/local/lib", NULL
	};
	size_t d, e;

	if (env != NULL && env[0] != '\0')
		return env;

	for (d = 0; dirs[d] != NULL; d++) {
		for (e = 0; g_lib_exts[e] != NULL; e++) {
			snprintf(buf, sizeof(buf), "%s/libretrace.%s",
				dirs[d], g_lib_exts[e]);
			if (file_readable(buf))
				return buf;
		}
	}
	return NULL;
}

int prof_capture_run(char *const argv[], const char *lib,
		     const char *trace_path)
{
	pid_t pid;
	int status;
	size_t v;

	pid = fork();
	if (pid < 0)
		return -1;

	if (pid == 0) {
		setenv("RETRACE_LOGGER_DEF_ENA", "1", 1);
		setenv("RETRACE_LOGGER_DEF_STDOUT_ENA", "0", 1);
		setenv("RETRACE_LOGGER_DEF_FN", trace_path, 1);
		for (v = 0; g_preload_vars[v] != NULL; v++)
			setenv(g_preload_vars[v], lib, 1);
		execvp(argv[0], argv);
		fprintf(stderr,
			"retrace-profile capture: cannot exec '%s'\n",
			argv[0]);
		_exit(127);
	}

	if (waitpid(pid, &status, 0) < 0)
		return -1;
	if (WIFEXITED(status))
		return WEXITSTATUS(status);
	return -1;
}

#else /* _WIN32 */

#include "capture.h"

const char *prof_capture_find_lib(void)
{
	return NULL;
}

int prof_capture_run(char *const argv[], const char *lib,
		     const char *trace_path)
{
	(void)argv;
	(void)lib;
	(void)trace_path;
	return -1;
}

#endif /* !_WIN32 */
