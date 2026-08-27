/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

#ifndef RETRACE_TOOLS_ENFORCE_APPCONTAINER_APPLY_H_
#define RETRACE_TOOLS_ENFORCE_APPCONTAINER_APPLY_H_

#include "enforce_spec.h"

/*
 * AppContainer application (TODO.beyond-libc/01 P1, Windows).
 * Creates (or reuses) the container profile named by the spec,
 * grants the declared read/write paths to the container SID,
 * and launches the command INSIDE the container. Returns the
 * child's exit code (the caller exits with it -- there is no
 * exec-after: the container is the process boundary).
 *
 * Returns:
 *   >= 0  child exit code (Windows only)
 *   -1    launch failed (the caller must not run the command)
 *   1     the plane is missing on this host (POSIX: always)
 */
int enforce_appcontainer_apply(const struct enforce_spec *spec,
	char *const argv[], char *const envp[]);

#endif /* RETRACE_TOOLS_ENFORCE_APPCONTAINER_APPLY_H_ */
