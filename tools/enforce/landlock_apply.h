/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

#ifndef RETRACE_ENFORCE_LANDLOCK_H_
#define RETRACE_ENFORCE_LANDLOCK_H_

#include "enforce_spec.h"

/*
 * Apply the spec's Landlock ruleset to THIS process (children
 * inherit). Returns:
 *   0  applied
 *   1  kernel lacks Landlock (ENOSYS/EPERM) -- caller decides
 *      whether that is fatal (fail-closed callers treat it so)
 *  -1  spec application error
 */
int enforce_landlock_apply(const struct enforce_spec *spec);

#endif /* RETRACE_ENFORCE_LANDLOCK_H_ */
