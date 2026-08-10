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

#include "log_flusher.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stddef.h>
#include <time.h>

#include "real_impls.h"
#include "logger.h"
#include "log_ring.h"

/*
 * Flusher cadence. 1 ms keeps latency low while keeping CPU use
 * bounded: the thread does O(N_rings) work per iteration, where
 * N_rings is bounded by the thread count of the target.
 *
 * A future P2 task can make this configurable via
 * RETRACE_LOGGER_FLUSH_MS. For now, 1 ms is the right default
 * for the latency-vs-throughput tradeoff in tracing workloads.
 */
#define FLUSHER_INTERVAL_NS 1000000  /* 1 ms */

struct FlusherState {
	retrace_log_flusher_emit_cb cb;

	void *ctx;

	pthread_t tid;

	_Atomic int running;

	_Atomic int stop_signal;
};

static struct FlusherState g_flusher;

/*
 * Adapter to bridge retrace_log_ring_drain's per-entry callback
 * signature to our emit callback. We pass the emit callback pair
 * via a thread-local context struct.
 */
struct DrainAdapterCtx {
	retrace_log_flusher_emit_cb cb;
	void *user_ctx;
	size_t emitted;
	int stop;
};

static int drain_adapter(const struct LogEntry *e, void *p)
{
	struct DrainAdapterCtx *c = (struct DrainAdapterCtx *)p;
	int rc;

	if (c->stop)
		return 1;
	rc = c->cb(e, c->user_ctx);
	if (rc != 0)
		c->stop = 1;
	else
		c->emitted++;
	return rc;
}

/* Walk callback: drain each ring into the adapter. */
static void ring_walk_drain(struct LogRing *r, void *p)
{
	struct DrainAdapterCtx *c = (struct DrainAdapterCtx *)p;

	if (c->stop)
		return;
	retrace_log_ring_drain(r, drain_adapter, p);
}

size_t retrace_log_flusher_drain_all(retrace_log_flusher_emit_cb cb,
				     void *ctx)
{
	struct DrainAdapterCtx c = {.cb = cb, .user_ctx = ctx,
				    .emitted = 0, .stop = 0};

	if (cb == NULL)
		return 0;
	retrace_log_ring_walk(ring_walk_drain, &c);
	return c.emitted;
}

static void *flusher_main(void *arg)
{
	struct timespec ts = {
		.tv_sec = 0,
		.tv_nsec = FLUSHER_INTERVAL_NS,
	};
	struct FlusherState *s = (struct FlusherState *)arg;

	/*
	 * Mark this thread as "logging disabled" so any libc call
	 * it makes (which would otherwise trigger a retrace
	 * intercept, create a ThreadContext, run log_params, push
	 * to THIS thread's ring, drain, call emit, call printf,
	 * intercept again... infinite recursion) short-circuits
	 * at the log_json gate. See logger.c.
	 */
	retrace_logger_disable_for_this_thread();

	while (atomic_load_explicit(&s->stop_signal,
		memory_order_relaxed) == 0) {
		retrace_log_flusher_drain_all(s->cb, s->ctx);
		nanosleep(&ts, NULL);
	}

	/* Final drain after the stop signal to capture in-flight. */
	retrace_log_flusher_drain_all(s->cb, s->ctx);

	/* Mark thread as exited so flusher_stop's spin-wait can
	 * observe exit without pthread_join. pthread_join from a
	 * dyld destructor hangs on macOS; the spin-wait + this
	 * release-store sidesteps that.
	 */
	atomic_store_explicit(&s->running, 0, memory_order_release);
	return NULL;
}

int retrace_log_flusher_init(retrace_log_flusher_emit_cb cb, void *ctx)
{
	int rc;

	if (cb == NULL) {
		log_err("log_flusher: callback required");
		return -1;
	}

	if (atomic_load_explicit(&g_flusher.running,
		memory_order_relaxed) == 1) {
		log_err("log_flusher: already running");
		return -1;
	}

	g_flusher.cb = cb;
	g_flusher.ctx = ctx;
	atomic_store_explicit(&g_flusher.stop_signal, 0,
		memory_order_relaxed);

	rc = retrace_real_impls.pthread_create(&g_flusher.tid, NULL,
		flusher_main, &g_flusher);
	if (rc != 0) {
		log_err("log_flusher: pthread_create failed: %d", rc);
		return -1;
	}

	atomic_store_explicit(&g_flusher.running, 1,
		memory_order_relaxed);
	return 0;
}

void retrace_log_flusher_stop(void)
{
	if (atomic_load_explicit(&g_flusher.running,
		memory_order_relaxed) != 1)
		return;

	atomic_store_explicit(&g_flusher.stop_signal, 1,
		memory_order_relaxed);

#if defined(__APPLE__)
	/* macOS: pthread_join from a dyld __attribute__((destructor))
	 * hangs because the threading library is mid-teardown. Use
	 * a bounded spin-wait instead, plus a grace period for the
	 * thread to fully terminate before the destructor touches
	 * stdout/file.
	 */
	{
		struct timespec poll = {.tv_sec = 0, .tv_nsec = 100000};
		struct timespec grace = {.tv_sec = 0, .tv_nsec = 10000000};
		int waits = 0;

		while (atomic_load_explicit(&g_flusher.running,
			memory_order_acquire) == 1 && waits < 10000) {
			nanosleep(&poll, NULL);
			waits++;
		}
		if (waits < 10000)
			nanosleep(&grace, NULL);
	}
#else
	/* Linux / BSD: pthread_join in a destructor works correctly.
	 * This is the safe path: the thread is fully terminated
	 * before we return.
	 */
	retrace_real_impls.pthread_join(g_flusher.tid, NULL);
#endif
}
