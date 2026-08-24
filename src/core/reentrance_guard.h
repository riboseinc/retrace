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
 * Reentrance guard for the v2 engine.
 *
 * One of the subtle invariants of LD_PRELOAD interposition: any libc
 * call made *inside* the engine (e.g. log_info -> fprintf -> malloc)
 * is itself intercepted, recursing back into retrace_engine_wrapper.
 * Without a guard, the recursion is unbounded.
 *
 * The guard uses thread_ctx->real_impl as the in-use marker. The
 * engine sets it on entry and clears it (via thread_context_clear)
 * on exit. A nested call sees real_impl != NULL and bails.
 *
 * Why a separate module (ADR-0013):
 *   - engine.c is the orchestrator; it should not own the in-use
 *     semantic directly. The check + enter pair is a single
 *     concept that belongs together.
 *   - Unit testing the guard in isolation is now possible.
 *   - The "what does in-use mean?" question has one canonical
 *     answer (this file), not a comment in engine.c.
 *
 * The guard does NOT clear state on exit -- that's
 * retrace_thread_context_clear's job. The two pair: enter marks
 * active, clear resets everything (including the marker).
 */

#ifndef RETRACE_CORE_REENTRANCE_GUARD_H_
#define RETRACE_CORE_REENTRANCE_GUARD_H_

#include <stddef.h>

#include "engine.h"

/*
 * Sentinel marker for "this thread is permanently in an
 * intercept" (TODO.trace-profile/31): the otlp-c exporter
 * thread holds the guard for its lifetime so any nested libc
 * call from within it (e.g. send on the socket fd) sees the
 * guard as active and bails -- no self-interposition recursion.
 */
#define RETRANCE_GUARD_PERMANENT ((void *)0x1)

/*
 * Returns 1 if the current thread is already inside an active
 * intercept (a nested libc call from within an action), else 0.
 * Also returns 1 when the guard is held permanently
 * (retrance_guard_enter_permanent).
 *
 * The check is intentionally trivial -- it reads one field. The
 * value of having it as a function is semantic: callers express
 * "is this a re-entrant call?" instead of "is real_impl set?"
 */
int retrace_reentrance_guard_active(const struct ThreadContext *ctx);

/*
 * Mark the context as actively intercepting. Captures real_impl
 * (for call_real to use later) and arch_spec_ctx (for the
 * trampoline to read back the return value).
 *
 * Calling enter on an already-active context is a programming
 * error (the engine should have checked active() first); the
 * function overwrites the prior state silently.
 */
void retrace_reentrance_guard_enter(struct ThreadContext *ctx,
				    void *real_impl,
				    void *arch_spec_ctx);

/*
 * Mark the context as PERMANENTLY in-intercept (TODO 31): the
 * guard stays active for the lifetime of the thread context.
 * Used by threads that own the otlp-c exporter's sockets --
 * any nested libc call (send, connect) sees the guard as
 * active and passes through to the real impl without
 * re-entering retrace's wrappers.
 *
 * Pair with a ThreadContext that's NEVER cleared (no
 * thread_context_clear on the exporter thread). For one-shot
 * background workers that exit, just leak the context.
 */
void retrace_reentrance_guard_enter_permanent(struct ThreadContext *ctx,
					      void *arch_spec_ctx);

#endif /* RETRACE_CORE_REENTRANCE_GUARD_H_ */
