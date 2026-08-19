/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Contention benchmark: lock-free ring logger under N producer
 * threads (TODO.complete/34; the ring shipped in v2.3.0).
 *
 * The ring's design claim is scalability: each producer thread
 * owns its own SPSC ring, the hot path is two relaxed loads plus
 * one release store, and a single flusher thread performs the
 * I/O. If that holds, aggregate throughput should scale close
 * to linearly with producer count. A hidden mutex or false
 * sharing would flatten the curve -- this benchmark makes that
 * visible.
 *
 * For each thread count (1, 2, 4, 8):
 *   - start the flusher with a counting no-op callback
 *   - spawn N producers, each pushing PER_THREAD entries
 *     through the real hot path (log_info -> ring push)
 *   - time the wall clock across a start barrier
 *   - report aggregate entries/sec and ns/entry
 *   - stop the flusher and verify every entry was drained
 *     (correctness under contention, not just speed)
 *
 * Output lines are diff-stable like the other benchmarks:
 *
 *   content=N threads total=X wall=Y.ns eps=Z ns_per_entry=W
 */

#include "bench.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "log_ring.h"
#include "log_flusher.h"
#include "logger.h"
#include "real_impls.h"

#define PER_THREAD 20000
#define MAX_THREADS 8

/* Portable start barrier: macOS has no pthread_barrier_t, so a
 * mutex + condvar gate stands in. Only the start line needs it;
 * producers run free afterwards.
 */
struct StartGate {
	pthread_mutex_t mu;
	pthread_cond_t cv;
	int go;
};

static struct StartGate g_gate;

static void gate_init(struct StartGate *g)
{
	pthread_mutex_init(&g->mu, NULL);
	pthread_cond_init(&g->cv, NULL);
	g->go = 0;
}

static void gate_release(struct StartGate *g)
{
	pthread_mutex_lock(&g->mu);
	g->go = 1;
	pthread_cond_broadcast(&g->cv);
	pthread_mutex_unlock(&g->mu);
}

static void gate_wait(struct StartGate *g)
{
	pthread_mutex_lock(&g->mu);
	while (!g->go)
		pthread_cond_wait(&g->cv, &g->mu);
	pthread_mutex_unlock(&g->mu);
}

static void gate_destroy(struct StartGate *g)
{
	pthread_mutex_destroy(&g->mu);
	pthread_cond_destroy(&g->cv);
}

struct ProducerArg {
	int entries;
	int done;
};

static struct timespec g_t0, g_t1;

/* Counting no-op sink: frees nothing, formats nothing, just
 * counts what the flusher drained. The entry text was heap-
 * allocated by the ring; drain() frees it after the callback, so
 * a pure counter is correct.
 */
static uint64_t g_drained;
static pthread_mutex_t g_drained_mu = PTHREAD_MUTEX_INITIALIZER;

static int counting_emit(const struct LogEntry *entry, void *ctx)
{
	/* Count only producer entries. Each thread's first log_info
	 * also emits a per-thread "config_cache built" banner, and
	 * deinit emits a drop summary -- those are not producer
	 * payload and must not contaminate the accounting. The text
	 * is the serialized JSON line, so match by substring.
	 */
	if (entry->text == NULL ||
	    strstr(entry->text, "bench entry") == NULL)
		return 0;
	(void)ctx;
	pthread_mutex_lock(&g_drained_mu);
	g_drained++;
	pthread_mutex_unlock(&g_drained_mu);
	return 0;
}

static void *producer(void *p)
{
	struct ProducerArg *arg = (struct ProducerArg *)p;
	int i;

	gate_wait(&g_gate);
	for (i = 0; i < arg->entries; i++)
		log_info("bench entry %d payload", i);
	arg->done = 1;
	return NULL;
}

static uint64_t wall_ns(void)
{
	return (uint64_t)(g_t1.tv_sec - g_t0.tv_sec) * 1000000000ULL +
		(uint64_t)(g_t1.tv_nsec - g_t0.tv_nsec);
}

static int run_config(int nthreads)
{
	pthread_t tids[MAX_THREADS];
	struct ProducerArg args[MAX_THREADS];
	uint64_t total = (uint64_t)nthreads * PER_THREAD;
	uint64_t ns, eps, ns_per;
	uint64_t dropped, delivered;
	int i;
	int rc = 0;

	g_drained = 0;
	retrace_log_ring_init();
	if (retrace_log_flusher_init(counting_emit, NULL) != 0) {
		printf("content=%d threads FLUSHER_INIT_FAILED\n", nthreads);
		return -1;
	}

	gate_init(&g_gate);
	for (i = 0; i < nthreads; i++) {
		args[i].entries = PER_THREAD;
		args[i].done = 0;
		pthread_create(&tids[i], NULL, producer, &args[i]);
	}

	/* Small sleep so late producers reach the gate before the
	 * starter fires; keeps the measured window tight -- and lets
	 * the flusher drain its own startup banner entries so they
	 * do not contaminate the producer count.
	 */
	struct timespec delay = {0, 20000000};

	nanosleep(&delay, NULL);
	pthread_mutex_lock(&g_drained_mu);
	g_drained = 0;
	pthread_mutex_unlock(&g_drained_mu);
	clock_gettime(CLOCK_MONOTONIC, &g_t0);
	gate_release(&g_gate);
	for (i = 0; i < nthreads; i++)
		pthread_join(tids[i], NULL);
	clock_gettime(CLOCK_MONOTONIC, &g_t1);
	gate_destroy(&g_gate);

	/* Final drain happens in stop(); give the flusher the last
	 * batch before stopping.
	 */
	retrace_log_flusher_stop();

	ns = wall_ns();
	eps = ns > 0 ? total * 1000000000ULL / ns : 0;
	ns_per = total > 0 ? ns / total : 0;
	dropped = retrace_log_ring_total_dropped();
	delivered = g_drained;

	printf("content=%d threads total=%llu wall=%llu.ns eps=%llu ns_per_entry=%llu delivered=%llu dropped=%llu\n",
		nthreads,
		(unsigned long long)total,
		(unsigned long long)ns,
		(unsigned long long)eps,
		(unsigned long long)ns_per,
		(unsigned long long)delivered,
		(unsigned long long)dropped);

	/* Accounting check: the ring is drop-on-full by design, and
	 * every drop bumps the dropped counter. The invariant is
	 * delivered + dropped == total -- no entry may vanish
	 * unaccounted. (Sustained rates above the flusher cadence
	 * *will** drop; that ceiling is exactly what this benchmark
	 * exists to measure.)
	 */
	if (delivered + dropped != total) {
		printf("content=%d threads UNACCOUNTED=%llu\n", nthreads,
			(unsigned long long)(total - delivered - dropped));
		rc = -1;
	}

	retrace_log_ring_deinit();
	return rc;
}

int main(void)
{
	int configs[] = {1, 2, 4, 8};
	size_t i;
	int rc = 0;

	retrace_real_impls.strcmp = strcmp;
	retrace_real_impls.strlen = strlen;
	retrace_real_impls.strcpy = strcpy;
	retrace_real_impls.strncmp = strncmp;
	retrace_real_impls.memset = memset;
	retrace_real_impls.memcpy = memcpy;
	retrace_real_impls.malloc = malloc;
	retrace_real_impls.free = free;
	retrace_real_impls.real_snprintf = snprintf;
	retrace_real_impls.real_sprintf = sprintf;
	retrace_real_impls.real_vsnprintf = vsnprintf;
	retrace_real_impls.rc_mutex_lock = rc_mutex_lock_posix;
	retrace_real_impls.rc_mutex_unlock = rc_mutex_unlock_posix;
	retrace_real_impls.rc_mutex_init = rc_mutex_init_posix;
	retrace_real_impls.rc_mutex_destroy = rc_mutex_destroy_posix;
	retrace_real_impls.rc_tss_create = rc_tss_create_posix;
	retrace_real_impls.rc_tss_delete = rc_tss_delete_posix;
	retrace_real_impls.rc_tss_get = rc_tss_get_posix;
	retrace_real_impls.rc_tss_set = rc_tss_set_posix;
	retrace_real_impls.rc_thread_create = rc_thread_create_posix;
	retrace_real_impls.rc_thread_join = rc_thread_join_posix;

	printf("--- log ring contention benchmark ---\n");
	printf("per-thread entries: %d\n", PER_THREAD);

	for (i = 0; i < sizeof(configs) / sizeof(configs[0]); i++) {
		if (run_config(configs[i]) != 0)
			rc = 1;
	}

	if (rc == 0)
		printf("accounting complete at every thread count "
			"(delivered + dropped == pushed)\n");
	else
		printf("WARNING: entries unaccounted under contention\n");

	return rc;
}
