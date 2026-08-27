/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

#ifndef RETRACE_ENFORCE_SECCOMP_H_
#define RETRACE_ENFORCE_SECCOMP_H_

#include "enforce_spec.h"

/*
 * Install the spec's seccomp floor: classic BPF, deny-list
 * style -- listed syscalls return EPERM, everything else is
 * allowed (P0; the coarse floor under the path-precise Landlock
 * plane). Returns 0 applied, 1 unsupported kernel, -1 error.
 */
int enforce_seccomp_apply(const struct enforce_spec *spec);

#endif /* RETRACE_ENFORCE_SECCOMP_H_ */
