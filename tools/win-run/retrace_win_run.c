/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * retrace-win-run -- the Windows launcher (TODO.windows/05).
 * There is no LD_PRELOAD on Windows; this tool creates the
 * target SUSPENDED, injects retrace.dll (hooks install + engine
 * boot run inside the child), resumes, and waits.
 *
 * usage: retrace-win-run [--lib <retrace.dll>] <target.exe> [args...]
 *
 * Environment is inherited: set RETRACE_JSON_CONFIG,
 * RETRACE_LOGGER_DEF_*, and optionally RETRACE_WIN_NTDLL=1
 * (ntdll-depth hooks -- see docs/windows.md) before running.
 */

#include "inject.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(FILE *out)
{
	fprintf(out,
"Usage: retrace-win-run [--lib <retrace.dll>] <target.exe> [args...]\n"
"\n"
"The Windows equivalent of LD_PRELOAD: the target is created\n"
"suspended, retrace.dll is injected, hooks install and the engine\n"
"boots inside the child, then the target resumes under trace.\n"
"\n"
"Environment (inherited by the child):\n"
"  RETRACE_JSON_CONFIG      trace/jail config (required for actions)\n"
"  RETRACE_LOGGER_DEF_FN    JSON log output file\n"
"  RETRACE_WIN_NTDLL=1      opt in to ntdll-depth hooks\n");
}

int main(int argc, char **argv)
{
	const char *dll_path = NULL;
	char cmdline[1024];
	char dll_buf[MAX_PATH];
	size_t off = 0;
	int i;
	int first_target = 1;
	DWORD pid;
	HMODULE self;

	for (i = 1; i < argc; i++) {
		/*
		 * Flags belong to the launcher only BEFORE the
		 * target: everything after the first positional is
		 * the child's own command line (ping -n 4 is not
		 * ours to judge) -- the usage line always said so.
		 */
		if (first_target) {
			if (strcmp(argv[i], "--lib") == 0 &&
			    i + 1 < argc) {
				dll_path = argv[++i];
				continue;
			} else if (strcmp(argv[i], "-h") == 0 ||
				   strcmp(argv[i], "--help") == 0) {
				usage(stdout);
				return 0;
			} else if (argv[i][0] == '-' &&
				   argv[i][1] != '\0') {
				usage(stderr);
				return 2;
			}
		}
		if (first_target) {
			/* quote the target path (spaces in paths) */
			off += (size_t)snprintf(cmdline + off,
				sizeof(cmdline) - off, "\"%s\"", argv[i]);
			first_target = 0;
		} else {
			off += (size_t)snprintf(cmdline + off,
				sizeof(cmdline) - off, " %s", argv[i]);
		}
	}
	if (first_target) {
		usage(stderr);
		return 2;
	}

	/* DLL resolution: --lib, RETRACE_V2_LIB, then next to this
	 * exe (the installed layout puts them together).
	 */
	if (dll_path == NULL)
		dll_path = getenv("RETRACE_V2_LIB");
	if (dll_path == NULL) {
		self = GetModuleHandleA(NULL);
		if (self != NULL && GetModuleFileNameA(self, dll_buf,
						       MAX_PATH) > 0) {
			char *slash = strrchr(dll_buf, '\\');

			if (slash != NULL) {
				snprintf(slash + 1,
					sizeof(dll_buf) -
						(size_t)(slash + 1 - dll_buf),
					"retrace.dll");
				dll_path = dll_buf;
			}
		}
	}
	if (dll_path == NULL)
		dll_path = "retrace.dll";

	{
		DWORD child_exit = (DWORD)-1;

		pid = retrace_win_inject_run(cmdline, dll_path,
			&child_exit);
		if (pid == 0) {
			fprintf(stderr,
				"retrace-win-run: failed to launch '%s'\n",
				cmdline);
			return 1;
		}
		fprintf(stderr, "retrace-win-run: child exit %lu\n",
			(unsigned long)child_exit);
		/* exit WITH the child's code: honest propagation.
		 * (DWORD)-1 would collide with the caller's -1
		 * sentinel -- remap to a distinct code.
		 */
		if (child_exit == (DWORD)-1)
			return 250;
		return (int)child_exit;
	}
}
