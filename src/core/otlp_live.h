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
#ifndef RETRACE_CORE_OTLP_LIVE_H_
#define RETRACE_CORE_OTLP_LIVE_H_

#include <stdint.h>

/*
 * otlp-c live streaming (TODO.trace-profile/31).
 *
 * When RETRACE_OTLP_ENDPOINT is set at process start, a background
 * thread is spawned which:
 *   1. creates an otlp-c exporter + tracer pointed at the endpoint
 *   2. holds the per-thread reentrance guard for its lifetime so
 *      its own send/connect/recv through the hooked libc reaches
 *      the real implementation (same lesson as the NtWriteFile
 *      recursion in TODO 28)
 *   3. pumps otlp_exporter_tick() every OTLP_TICK_MS
 *   4. on retrace_otlp_live_deinit, calls shutdown + bounded
 *      flush (2s ceiling) + free
 *
 * The hot path -- every traced call -- is just otlp_exporter_emit():
 * a lock-free MPSC enqueue. Span construction + serialization
 * happens on the tick thread, off the wrapper's critical path.
 *
 * What it emits: one span per traced call (per-intercept entry),
 * with retrace-specific attributes (retrace.func, retrace.module,
 * retrace.pid, retrace.tid, retrace.call_duration_us where
 * present, retrace.ret_val). Trace ID is per-process; span ID
 * is per-call -- so a single traced binary shows as one trace
 * with N spans.
 *
 * Bounded failure: if the endpoint is down, the MPSC queue fills
 * and spans drop with otlp_exporter_stats.dropped_full counting
 * the loss. The target's behavior is never blocked or crashed.
 *
 * Endpoint parsing: the env var is taken verbatim as the OTLP/HTTP
 * base URL (e.g. http://collector:4318). otlp-c appends
 * /v1/traces automatically.
 */

int retrace_otlp_live_init(void);
void retrace_otlp_live_deinit(void);

/*
 * Push one traced call (encoded as a JSON object string -- the
 * same shape the logger writes to stdout/file) to the otlp-c
 * exporter's MPSC queue. Returns 0 on success (enqueued, dropped
 * on full, or exporter not initialized -- the latter is
 * silent). Called from the log flusher's emit callback (single
 * thread), but otlp_exporter_emit is itself thread-safe so any
 * caller works.
 *
 * `serialized_json` is the parson-serialized envelope
 * (time/pid/tid/module/severity/message.func/...). The function
 * parses the envelope, extracts the trace semantics, builds a
 * span, and emits it.
 */
int retrace_otlp_live_emit_json(const char *serialized_json);

/*
 * Drain stats from the live exporter. 0 if not initialized. Used
 * by the at-exit diagnostics line (RETRACE_OTLP_STATS=1).
 */
void retrace_otlp_live_get_stats(uint64_t *emitted, uint64_t *sent,
				 uint64_t *dropped_full,
				 uint64_t *dropped_err);

#endif /* RETRACE_CORE_OTLP_LIVE_H_ */
