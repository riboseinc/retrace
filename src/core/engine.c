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

#ifndef __GNUC__
#error GNU extensions are required!
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>

#include "engine.h"
#include "real_impls.h"
#include "arch_spec.h"
#include "actions.h"
#include "conf.h"
#include "logger.h"
#include "thread_context.h"
#include "reentrance_guard.h"
#include "script_resolver.h"
#include "action_runner.h"

/*
 * The central dispatch entry point invoked by every per-arch asm
 * trampoline. Responsibilities, in strict order:
 *
 *   1. Look up the real libc implementation (no allocation here — this
 *      runs before init and during reentry).
 *   2. If init never completed, schedule the real call and return.
 *   3. Acquire the per-thread interception context.
 *   4. If real_impl is NULL, abort the call with ret_val = -1.
 *   5. Default to calling the real implementation.
 *   6. Reentrance guard: if thread_ctx already holds a real_impl,
 *      we're inside a nested call — bail.
 *   7. Mark this thread as actively intercepting.
 *   8. Look up the prototype; if missing, just call real.
 *   9. Apply the per-function log filter (issue #486).
 *  10. Parse variadic args out of the frame (or bail for FP varargs).
 *  11. Find the intercept_script; if none, call real.
 *  12. Hand off to action_runner to actually run the script.
 *  13. Write thread_ctx->ret_val back into the frame.
 *  14. Clear the per-thread context.
 *
 * Steps 3, 11, 12 live in their own modules (ADR-0013); this function
 * is the orchestrator that calls them in order.
 */
void retrace_engine_wrapper(char *func_name,
	void *arch_spec_ctx)
{
	void *real_impl;
	struct ThreadContext *thread_ctx;
	const JSON_Object *i_script;
	const JSON_Array *i_scripts;

	real_impl = retrace_as_get_real_safe(func_name);
	if (!retrace_inited) {
		retrace_as_sched_real(arch_spec_ctx, real_impl);
		return;
	}

	thread_ctx = retrace_thread_context_get();
	if (thread_ctx == NULL) {
		log_err(
			"%s() intercept failed - could not get context",
			func_name);
		return;
	}

	if (real_impl == NULL) {
		/* cannot process
		 * abort call with return val -1,
		 * -1 is chosen for the best chance of
		 * signaling an error for CRT funcs
		 * The caller will probably crash anyway...
		 */
		retrace_as_set_ret_val(arch_spec_ctx, -1);
		return;
	}

	/* set default to call real impl */
	retrace_as_sched_real(arch_spec_ctx, real_impl);

	/* Reentrance guard: nested libc calls from inside an action
	 * would recurse unbounded. Bail if the current thread is
	 * already inside an intercept (reentrance_guard.c).
	 */
	if (retrace_reentrance_guard_active(thread_ctx))
		return;

	retrace_reentrance_guard_enter(thread_ctx, real_impl,
		arch_spec_ctx);

	thread_ctx->prototype = retrace_func_get(func_name);

	/* if func is not prototyped - do not intervene */
	if (thread_ctx->prototype == NULL)
		goto clean_up;

	/*
	 * Per-function log filter (issue #486). If the user excluded
	 * this function (or didn't include it in the allowlist), skip
	 * all action processing. The real call still runs via
	 * call_real_flag (already set by retrace_as_sched_real above).
	 */
	if (!retrace_logger_func_loggable(func_name)) {
		log_dbg("func '%s' filtered -- skip actions", func_name);
		goto clean_up;
	}

	/* setup params, do not proceed if failed since
	 * it can be dangerous to call orig with partial params
	 */
	thread_ctx->params_cnt = ENGINE_MAXCOUNT_PARAMS;
	if (!retrace_as_setup_params(thread_ctx->arch_spec_ctx,
		thread_ctx->prototype,
		thread_ctx->params,
		&thread_ctx->params_cnt)) {
		log_err(
			"failed to setup params for %s(), will not proceed",
			func_name);
		goto clean_up;
	}

	/* find intercept script for the func and return addr */
	i_scripts = json_object_get_array(retrace_conf, "intercept_scripts");
	if (!i_scripts) {
		log_warn(
			"%s() config does not contain intercept_scripts",
			func_name);
		goto clean_up;
	}

	i_script = retrace_script_find(i_scripts, func_name,
		thread_ctx->ret_addr);

	if (!i_script) {
		/* no script defined for this func, call real */
		goto clean_up;
	}

	/* we have script, do not call real impl by default */
	retrace_as_cancel_sched_real(arch_spec_ctx);

	retrace_action_runner_run(thread_ctx, func_name, i_script);

	/* write back to arch spec. portion */
	retrace_as_set_ret_val(arch_spec_ctx, thread_ctx->ret_val);

clean_up:
	/* mark hi-level intercept done */
	retrace_thread_context_clear(thread_ctx);
}

int retrace_engine_init(void)
{
	return retrace_thread_context_init();
}
