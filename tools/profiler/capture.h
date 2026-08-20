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
 * One-shot capture (TODO.trace-profile/03): run a command under
 * the retrace preload with the right logger env, trace to
 * trace_path. POSIX only (fork/exec + LD_PRELOAD /
 * DYLD_INSERT_LIBRARIES).
 */

/*
 * Locate the retrace library: RETRACE_V2_LIB override, then the
 * build-tree layouts relative to our own executable, then the
 * install prefix. Returns a static/known buffer or NULL.
 */
const char *prof_capture_find_lib(void);

/*
 * Run argv under the preload library, trace to trace_path.
 * Returns the exit status of the command, or -1 on
 * launch failure.
 */
int prof_capture_run(char *const argv[], const char *lib,
		     const char *trace_path);

#ifdef __cplusplus
}
#endif

#endif /* RETRACE_TOOLS_PROFILER_CAPTURE_H_ */
