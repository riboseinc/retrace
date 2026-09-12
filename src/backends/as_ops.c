/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * The as-ops dispatcher (ADR-0016). The retrace_as_* entry points
 * the engine calls are thin forwarders: they read a thread-local
 * current table and delegate. The default table is the built
 * preload backend's (retrace_as_ops_default, defined in its
 * arch_spec_bottom.c); the ptrace trace loop swaps in the
 * syscall-lane table around its engine dispatch.
 *
 * Cost on the preload lanes: one TLS load + an indirect call per
 * engine entry -- noise against the microseconds actions take
 * (the F2 dispatch lesson).
 */

#include "arch_spec.h"

/*
 * Lane overrides apply only to the OUTERMOST engine dispatch on
 * a thread (ADR-0016): nested entries are the tracer's own
 * interposed libc inside an active dispatch -- trampoline-lane
 * frames that must keep the default ops. Declared in engine.c.
 */
extern int retrace_engine_dispatch_depth(void);

static _Thread_local const struct retrace_as_ops *tl_as_current;

void
retrace_as_ops_set(const struct retrace_as_ops *ops)
{
	tl_as_current = ops;
}

const struct retrace_as_ops *
retrace_as_ops_get(void)
{
	/* depth 0 = direct call (no engine entry): the installed
	 * table applies. depth 1 = outermost engine dispatch: the
	 * lane's own frame. depth >= 2 = a nested entry -- the
	 * tracer's own interposed libc inside an active dispatch:
	 * trampoline frames, default ops.
	 */
	if (tl_as_current != NULL &&
	    retrace_engine_dispatch_depth() <= 1)
		return tl_as_current;
	return &retrace_as_ops_default;
}

void
retrace_as_sched_real(void *arch_spec_ctx, void *real_impl)
{
	retrace_as_ops_get()->sched_real(arch_spec_ctx, real_impl);
}

void
retrace_as_cancel_sched_real(void *arch_spec_ctx)
{
	retrace_as_ops_get()->cancel_sched_real(arch_spec_ctx);
}

void
retrace_as_set_ret_val(void *arch_spec_ctx, intptr_t ret_val)
{
	retrace_as_ops_get()->set_ret_val(arch_spec_ctx, ret_val);
}

int
retrace_as_setup_params(void			*arch_spec_ctx,
			const struct FuncPrototype *proto,
			struct FuncParam	params[],
			int			*params_cnt)
{
	return retrace_as_ops_get()->setup_params(
		arch_spec_ctx, proto, params, params_cnt);
}

intptr_t
retrace_as_call_real(void			*arch_spec_ctx,
		     const void		*real_impl,
		     const struct FuncParam params[],
		     int			params_cnt)
{
	return retrace_as_ops_get()->call_real(
		arch_spec_ctx, real_impl, params, params_cnt);
}
