/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

#ifndef RETRACE_TOOLS_PROFILER_VALIDATE_H_
#define RETRACE_TOOLS_PROFILER_VALIDATE_H_

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Profile contract validation (TODO.trace-profile/06). Enforces
 * share/profile-schema.json without a schema engine (no new
 * dependencies; the schema file documents the same contract for
 * external tooling). Returns the number of violations found, or
 * -1 when the file cannot be parsed. err receives a
 * human-readable list of violations.
 */
int prof_validate_file(const char *path, char *err, size_t errsz);

#ifdef __cplusplus
}
#endif

#endif /* RETRACE_TOOLS_PROFILER_VALIDATE_H_ */
