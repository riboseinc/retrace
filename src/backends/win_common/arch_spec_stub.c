/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * TODO.windows/04 staging: the retrace_as_* entry points are the
 * trampoline contract (POSIX backends implement them in
 * arch_spec_bottom.c). On Windows nothing is hooked yet, so the
 * engine links against these no-ops; item 05 (first wrapper)
 * replaces them with the real x64/arm64 implementation.
 */

#include "arch_spec.h"

#include <stddef.h>

int retrace_as_init(void)
{
	return 0;
}

int retrace_as_init_late(void)
{
	return 0;
}

void retrace_as_sched_real(void *arch_spec_ctx, void *real_impl)
{
	(void)arch_spec_ctx;
	(void)real_impl;
}

void retrace_as_cancel_sched_real(void *arch_spec_ctx)
{
	(void)arch_spec_ctx;
}

void retrace_as_set_ret_val(void *arch_spec_ctx, long ret_val)
{
	(void)arch_spec_ctx;
	(void)ret_val;
}

void *retrace_as_get_real_safe(const char *real_impl)
{
	(void)real_impl;
	return NULL;
}

int retrace_as_setup_params(void *arch_spec_ctx,
	const struct FuncPrototype *proto, struct FuncParam params[],
	int *params_cnt)
{
	(void)arch_spec_ctx;
	(void)proto;
	(void)params;
	if (params_cnt != NULL)
		*params_cnt = 0;
	return 0;
}

long retrace_as_call_real(const void *real_impl,
	const struct FuncParam params[], int params_cnt)
{
	(void)real_impl;
	(void)params;
	(void)params_cnt;
	return -1;
}
