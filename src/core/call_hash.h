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

#ifndef RETRACE_CORE_CALL_HASH_H_
#define RETRACE_CORE_CALL_HASH_H_

#include <stddef.h>
#include <stdint.h>

/*
 * Per-thread rolling hash of intercepted libc calls
 * (TODO.complete/24 MVP).
 *
 * Each call updates the hash with (function_id, arg_count). After
 * the run, the hash distinguishes inputs that exercised different
 * libc-call sequences -- even if the instruction-level coverage
 * map (libFuzzer/AFL) is identical. That's coverage feedback the
 * fuzzer can't get any other way.
 *
 * MVP scope: per-thread state, accessible via env-gated getters.
 * The future shmem map writer (TODO.complete/24 P1) will expose
 * this to libFuzzer's coverage map.
 *
 * Activation: set RETRACE_CALL_HASH=1 at target launch. Off by
 * default (zero overhead when not used).
 */

/*
 * Exported global: the most recent call-hash value from ANY
 * thread. Updated atomically by retrace_call_hash_observe().
 *
 * A libFuzzer custom mutator (TODO.complete/24 P1) reads this
 * to bias mutations toward inputs that produce new call
 * sequences. The mutator compares the current value against
 * the value from the previous iteration -- if different, the
 * input exercised a new libc-call path, and similar mutations
 * should be favored.
 *
 * Access: read is atomic on all supported architectures
 * (aligned 64-bit load). Write uses __sync_lock_test_and_set
 * for portability.
 */
extern uint64_t retrace_call_hash_last;

/*
 * Initialize the module. Parses RETRACE_CALL_HASH env var. Safe
 * to call regardless of whether the feature is enabled.
 *
 * Returns 0 on success, -1 on failure (logs via log_err).
 */
int retrace_call_hash_init(void);

/*
 * Returns 1 if the call-hash feature is enabled (env was set to
 * non-zero at init). The engine checks this on every intercept
 * to skip the hash update when disabled -- zero overhead path.
 */
int retrace_call_hash_enabled(void);

/*
 * Update the calling thread's rolling hash with one observation.
 * Called by the engine on every intercepted call (when enabled).
 *
 * The hash is a 64-bit FNV-1a variant. Function name + arg count
 * are folded in; specific arg values are NOT (they'd cause the
 * hash to change on every call, defeating the coverage purpose).
 *
 * Disabled when retrace_call_hash_enabled() returns 0 (no-op).
 */
void retrace_call_hash_observe(const char *func_name, int arg_count);

/*
 * Read the calling thread's current hash. Returns 0 if hashing
 * is disabled or the thread has never observed a call.
 */
uint64_t retrace_call_hash_get(void);

/*
 * Walk every thread's hash and invoke `cb(hash, ctx)` once per
 * thread that has observed at least one call. Used at deinit to
 * surface the per-thread final hashes.
 */
typedef void (*retrace_call_hash_walk_cb)(uint64_t hash, void *ctx);

void retrace_call_hash_walk(retrace_call_hash_walk_cb cb, void *ctx);

/*
 * Teardown. Frees per-thread state. Called from logger-style
 * destructor.
 */
void retrace_call_hash_deinit(void);

#endif /* RETRACE_CORE_CALL_HASH_H_ */
