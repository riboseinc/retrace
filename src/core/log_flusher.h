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

#ifndef RETRACE_CORE_LOG_FLUSHER_H_
#define RETRACE_CORE_LOG_FLUSHER_H_

#include <stdint.h>
#include <stddef.h>

#include "log_ring.h"

/*
 * Background flusher thread (TODO.complete/19 PR B).
 *
 * Walks every registered LogRing at a fixed cadence (default 1 ms),
 * drains each into the user-supplied emit callback, and sleeps.
 *
 * The flusher is the SINGLE CONSUMER in the SPSC contract -- only
 * it may call retrace_log_ring_drain(). Producer threads never
 * block on I/O; they only push to their own ring.
 *
 * Lifecycle:
 *   1. retrace_log_ring_init()         (PR A -- module)
 *   2. retrace_log_flusher_init(cb)    (PR B -- this module)
 *   3. <target runs, threads push>
 *   4. retrace_log_flusher_stop()      -- signals stop, joins,
 *                                         does one final drain
 *   5. retrace_log_ring_deinit()       (PR A -- module)
 *
 * The flusher thread runs at normal priority. A future P2 task
 * can drop it to priority 0 via nice()/setpriority() on POSIX,
 * but that's portability overhead for marginal gain right now.
 */

/*
 * Emit callback. Called once per drained entry, on the flusher
 * thread. The entry pointer is valid only for the duration of
 * the call. Returns 0 on success, non-zero to signal "stop
 * emitting this batch" (the flusher will skip remaining entries
 * in the current drain but continue running).
 */
typedef int (*retrace_log_flusher_emit_cb)(const struct LogEntry *entry,
					   void *ctx);

/*
 * Spawn the flusher thread. Returns 0 on success, -1 on failure
 * (thread creation failure, or already running).
 *
 * The thread runs until retrace_log_flusher_stop() is called.
 * The callback is invoked on the flusher thread, so it must be
 * thread-safe with respect to anything it touches. In particular,
 * stdio writes via the real libc functions are NOT safe unless
 * routed through retrace_real_impls (which is the integration
 * path PR C will take).
 */
int retrace_log_flusher_init(retrace_log_flusher_emit_cb cb, void *ctx);

/*
 * Signal the flusher to stop, then join. Performs one final
 * drain before returning so all in-flight entries are emitted.
 *
 * Safe to call from the same thread that called init(). Not
 * safe to call from the flusher thread itself (would deadlock).
 *
 * After stop returns, the flusher is idle and init() can be
 * called again if desired.
 */
void retrace_log_flusher_stop(void);

/*
 * Drain-on-demand: walk all rings once and emit current contents.
 * Used by retrace_log_flusher_stop() for the final drain, and
 * exposed for tests that don't want to spawn the thread.
 *
 * Returns the total number of entries emitted across all rings.
 */
size_t retrace_log_flusher_drain_all(retrace_log_flusher_emit_cb cb,
				     void *ctx);

#endif /* RETRACE_CORE_LOG_FLUSHER_H_ */
