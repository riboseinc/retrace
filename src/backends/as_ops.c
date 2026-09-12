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
 * the engine and actions call are thin forwarders: they select
 * the ops table from the calling thread's context and delegate.
 * The default table is the built preload backend's
 * (retrace_as_ops_default, defined in its arch_spec_bottom.c);
 * a lane installs its table on the context around its engine
 * dispatch (the ptrace trace loop does).
 *
 * Selection is deliberately context-driven, NOT thread-local:
 * threads spawned mid-boot (the logger flusher) have broken TLV
 * under DYLD_INSERT on macOS, and the ThreadContext is already
 * per-thread. The dispatch_depth member gates nesting -- a
 * nested entry is the tracer's own interposed libc carrying a
 * trampoline frame, which must keep the default ops.
 */

#include "arch_spec.h"
#include "engine.h"

static const struct retrace_as_ops *
lane_ops_for(const struct ThreadContext *thread_ctx)
{
	if (thread_ctx != NULL && thread_ctx->lane_ops != NULL &&
	    thread_ctx->dispatch_depth <= 1)
		return thread_ctx->lane_ops;
	return &retrace_as_ops_default;
}

void
retrace_as_sched_real(struct ThreadContext *thread_ctx,
	void *arch_spec_ctx, void *real_impl)
{
	lane_ops_for(thread_ctx)->sched_real(arch_spec_ctx, real_impl);
}

void
retrace_as_cancel_sched_real(struct ThreadContext *thread_ctx,
	void *arch_spec_ctx)
{
	lane_ops_for(thread_ctx)->cancel_sched_real(arch_spec_ctx);
}

void
retrace_as_set_ret_val(struct ThreadContext *thread_ctx,
	void *arch_spec_ctx, intptr_t ret_val)
{
	const struct retrace_as_ops *ops = lane_ops_for(thread_ctx);

	/*
	 * The -1 + errno libc convention crosses to the kernel's
	 * -errno only on lanes that speak the syscall convention
	 * (an installed lane override). Translate HERE, where both
	 * the lane and the deny-time ret_errno are visible and
	 * un-clobbered; the default (preload) lane keeps the libc
	 * convention and its same-process errno.
	 */
	if (ops != &retrace_as_ops_default && ret_val == -1 &&
	    thread_ctx != NULL && thread_ctx->ret_errno > 0 &&
	    thread_ctx->ret_errno < 4096)
		ret_val = -(intptr_t) thread_ctx->ret_errno;

	ops->set_ret_val(arch_spec_ctx, ret_val);
}

int
retrace_as_setup_params(struct ThreadContext *thread_ctx,
	void *arch_spec_ctx,
	const struct FuncPrototype *proto,
	struct FuncParam params[],
	int *params_cnt)
{
	return lane_ops_for(thread_ctx)->setup_params(
		arch_spec_ctx, proto, params, params_cnt);
}

intptr_t
retrace_as_call_real(struct ThreadContext *thread_ctx,
	void *arch_spec_ctx,
	const void *real_impl,
	const struct FuncParam params[],
	int params_cnt)
{
	return lane_ops_for(thread_ctx)->call_real(
		arch_spec_ctx, real_impl, params, params_cnt);
}

void
retrace_as_ops_set(const struct retrace_as_ops *ops)
{
	struct ThreadContext *thread_ctx = retrace_thread_context_get();

	if (thread_ctx != NULL)
		thread_ctx->lane_ops = ops;
}

const struct retrace_as_ops *
retrace_as_ops_get(void)
{
	return lane_ops_for(retrace_thread_context_get());
}
