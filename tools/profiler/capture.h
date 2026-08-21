/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

#ifndef RETRACE_TOOLS_PROFILER_CAPTURE_H_
#define RETRACE_TOOLS_PROFILER_CAPTURE_H_

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * One-shot capture (TODO.trace-profile/03, 09): run a command
 * under retrace with the right logger env, trace to trace_path.
 * POSIX: preload + fork/exec. Windows: delegate to the
 * retrace-win-run injector (found next to this executable).
 */

/*
 * Locate the retrace library: RETRACE_V2_LIB override, then the
 * build-tree layouts relative to our own executable, then the
 * install prefix. Returns a static/known buffer or NULL.
 */
const char *prof_capture_find_lib(void);

/*
 * Fill buf with a unique, creatable temp file path (the caller
 * opens/truncates it). Returns 0/-1. The prefix is a POSIX-only
 * hint (Windows temp names are 3-char prefixed).
 */
int prof_capture_temp(char *buf, size_t bufsz, const char *prefix);

/*
 * Set an env var for the child (no overwrite when already set --
 * a user-provided RETRACE_JSON_CONFIG always wins).
 */
void prof_capture_setenv(const char *name, const char *value);

/*
 * Run argv under retrace, trace to trace_path (the logger env is
 * set here). Returns the exit status of the command, or -1 on
 * launch failure.
 */
int prof_capture_run(char *const argv[], const char *lib,
		     const char *trace_path);

#ifdef __cplusplus
}
#endif

#endif /* RETRACE_TOOLS_PROFILER_CAPTURE_H_ */
