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
 * Post-intercept cleanup for ThreadContext.
 *
 * Distinct from thread_context.c's lifecycle (alloc / free / get),
 * this module owns the reset-between-uses semantic. After each
 * intercepted call completes, the engine calls clear() to:
 *   1. Free any param buffers the actions allocated (free_val=1).
 *   2. Zero the whole context so the next intercept starts clean.
 *
 * Why a separate module (ADR-0013):
 *   - Lifecycle (create once per thread, destroy on thread exit)
 *     and post-intercept reset (run after every call) are
 *     different cadences with different concerns.
 *   - Unit-testing the param-buffer free loop separately from
 *     pthread_key management is cleaner.
 *
 * The function lives here; thread_context.h still declares it for
 * backward compatibility with engine.c.
 */

#ifndef RETRACE_CORE_CLEANUP_H_
#define RETRACE_CORE_CLEANUP_H_

#include "engine.h"

/*
 * Reset the context to a pristine state. Frees any param buffers
 * flagged with free_val=1 (typically set by modify_in_param_str
 * or modify_in_param_arr). After this call, the context is ready
 * for the next intercepted call.
 *
 * Safe to call on a zeroed context (no-op).
 */
void retrace_thread_context_clear(struct ThreadContext *thread_ctx);

#endif /* RETRACE_CORE_CLEANUP_H_ */
