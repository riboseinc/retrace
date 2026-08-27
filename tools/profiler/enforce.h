/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

#ifndef RETRACE_PROFILER_ENFORCE_H_
#define RETRACE_PROFILER_ENFORCE_H_

/*
 * `retrace-profile enforce` (TODO.beyond-libc/01): compile the
 * declared-set into a kernel-enforcement spec consumed by the
 * retrace-enforce installer. One declared-set, two planes: the
 * path-precise Landlock ruleset and the coarse seccomp floor.
 *
 *   retrace-profile enforce <profile.json> [--inside d.json]
 *       [--backend landlock|seccomp|both] [--exec <path>]
 *       [-o spec.json]
 */

int enforce_mode(int argc, char **argv);

#endif /* RETRACE_PROFILER_ENFORCE_H_ */
