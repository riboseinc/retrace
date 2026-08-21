/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * One-shot capture implementation (TODO.trace-profile/03, 09).
 * POSIX: fork/exec with LD_PRELOAD / DYLD_INSERT_LIBRARIES and
 * the logger env pointed at a trace file. Windows: no preload --
 * delegate to retrace-win-run (suspended CreateProcess + DLL
 * inject, the single owner of that machinery).
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

int prof_capture_temp(char *buf, size_t bufsz, const char *prefix)
{
	int fd;

	snprintf(buf, bufsz, "/tmp/%s-XXXXXX", prefix);
	fd = mkstemp(buf);
	if (fd < 0)
		return -1;
	close(fd);
	return 0;
}

void prof_capture_setenv(const char *name, const char *value)
{
	setenv(name, value, 0);
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

#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LAUNCHER_NAME "retrace-win-run.exe"

static char g_exe_dir[MAX_PATH];

static const char *exe_dir(void)
{
	char *slash;

	if (g_exe_dir[0] != '\0')
		return g_exe_dir;
	if (GetModuleFileNameA(NULL, g_exe_dir, sizeof(g_exe_dir)) == 0)
		return NULL;
	slash = strrchr(g_exe_dir, '\\');
	if (slash == NULL)
		return NULL;
	*slash = '\0';
	return g_exe_dir;
}

static int file_readable(const char *path)
{
	return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
}

static void join_path(char *buf, size_t bufsz, const char *dir,
		      const char *name)
{
	snprintf(buf, (int)bufsz, "%s\\%s", dir, name);
}

const char *prof_capture_find_lib(void)
{
	static char buf[MAX_PATH * 2];
	const char *env = getenv("RETRACE_V2_LIB");
	const char *dir;

	if (env != NULL && env[0] != '\0')
		return env;

	/* the build + install layouts put the DLL next to the tools */
	dir = exe_dir();
	if (dir != NULL) {
		join_path(buf, sizeof(buf), dir, "retrace.dll");
		if (file_readable(buf))
			return buf;
	}
	return NULL;
}

int prof_capture_temp(char *buf, size_t bufsz, const char *prefix)
{
	char dir[MAX_PATH];
	char path[MAX_PATH];

	(void)prefix; /* GetTempFileNameA prefixes are 3 chars max */
	if (GetTempPathA(sizeof(dir), dir) == 0)
		return -1;
	if (GetTempFileNameA(dir, "rtr", 0, path) == 0)
		return -1;
	snprintf(buf, (int)bufsz, "%s", path);
	return 0;
}

void prof_capture_setenv(const char *name, const char *value)
{
	_putenv_s(name, value);
}

/*
 * No preload on Windows: spawn retrace-win-run (next to this
 * exe) with --lib, inheriting this environment (the logger +
 * config vars are already set). retrace-win-run owns the
 * suspended-create + inject + resume machinery.
 */
int prof_capture_run(char *const argv[], const char *lib,
		     const char *trace_path)
{
	const char *dir = exe_dir();
	char launcher[MAX_PATH * 2];
	char cmd[2048];
	STARTUPINFOA si;
	PROCESS_INFORMATION pi;
	DWORD rc = (DWORD)-1;
	size_t off;
	int i;

	if (dir == NULL)
		return -1;
	join_path(launcher, sizeof(launcher), dir, LAUNCHER_NAME);
	if (!file_readable(launcher))
		return -1;

	off = (size_t)snprintf(cmd, sizeof(cmd),
		"\"%s\" --lib \"%s\"", launcher, lib);
	for (i = 0; argv[i] != NULL && off < sizeof(cmd) - 4; i++)
		off += (size_t)snprintf(cmd + off, sizeof(cmd) - off,
			" \"%s\"", argv[i]);
	(void)trace_path; /* flows via RETRACE_LOGGER_DEF_FN env */

	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	ZeroMemory(&pi, sizeof(pi));
	if (!CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL,
			    NULL, &si, &pi))
		return -1;

	WaitForSingleObject(pi.hProcess, INFINITE);
	if (!GetExitCodeProcess(pi.hProcess, &rc) || rc == STILL_ACTIVE)
		rc = (DWORD)-1;
	CloseHandle(pi.hThread);
	CloseHandle(pi.hProcess);
	return rc == (DWORD)-1 ? -1 : (int)rc;
}

#endif /* !_WIN32 */
