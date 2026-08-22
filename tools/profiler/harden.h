/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

#ifndef RETRACE_PROFILER_HARDEN_H_
#define RETRACE_PROFILER_HARDEN_H_

#include "aggregate.h"

#include <stdio.h>

/*
 * Emit the docker-compose hardening fragment for a profile
 * (TODO.trace-profile/19): read_only root, cap_drop ALL,
 * write-class paths as rw binds, read-class as ro, network off
 * when the profile shows none, env whitelist template.
 */
int prof_harden_compose(const struct Profile *p, FILE *out);

#endif /* RETRACE_PROFILER_HARDEN_H_ */
