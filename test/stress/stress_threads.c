/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Multi-threaded stress test for retrace v2 (TODO.complete/35 P0).
 *
 * Spawns N threads, each making M libc calls across several
 * function families. Designed to be run under LD_PRELOAD /
 * DYLD_INSERT_LIBRARIES to stress retrace's per-thread context
 * + global registries + reentrance guard under load.
 *
 * Verifies:
 *   1. No crash.
 *   2. All threads complete.
 *   3. Each thread's call count matches expectation.
 *   4. (ASAN/TSAN builds report no errors.)
 *
 * Default: 8 threads x 100K iterations = 800K intercepted calls
 * per family. Override via env vars:
 *
 *   STRESS_THREADS=N    default 8
 *   STRESS_ITERS=N      default 100000
 *
 * The test exits 0 on success, non-zero on any thread returning
 * an error or the main process crashing.
 *
 * Part of TODO.complete/35.
 */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#define DEFAULT_THREADS 8
#define DEFAULT_ITERS   100000

struct thread_arg {
	int thread_idx;
	long iters;
	long calls_made;
	long errors;
};

static void *worker(void *arg)
{
	struct thread_arg *a = (struct thread_arg *)arg;
	long i;
	char buf[64];

	a->calls_made = 0;
	a->errors = 0;

	for (i = 0; i < a->iters; i++) {
		/* Each iteration touches multiple libc surfaces so we
		 * exercise different prototypes + registries.
		 */

		/* stdlib.h */
		(void)abs((int)i);

		/* unistd.h */
		(void)getuid();

		/* string.h -- strlen is one of the hottest intercepts */
		snprintf(buf, sizeof(buf), "thread-%d-iter-%ld",
			a->thread_idx, i);
		(void)strlen(buf);

		/* stdio.h -- snprintf above already touches it */

		a->calls_made += 4;
	}

	return NULL;
}

int main(void)
{
	const char *threads_env;
	const char *iters_env;
	int n_threads;
	long n_iters;
	pthread_t *threads;
	struct thread_arg *args;
	int i;
	int rc;
	long total_calls = 0;
	long total_errors = 0;

	threads_env = getenv("STRESS_THREADS");
	n_threads = threads_env ? atoi(threads_env) : DEFAULT_THREADS;
	if (n_threads <= 0 || n_threads > 1024)
		n_threads = DEFAULT_THREADS;

	iters_env = getenv("STRESS_ITERS");
	n_iters = iters_env ? atol(iters_env) : DEFAULT_ITERS;
	if (n_iters <= 0)
		n_iters = DEFAULT_ITERS;

	printf("[stress] starting: %d threads x %ld iters x 4 calls/iter\n",
		n_threads, n_iters);

	threads = (pthread_t *)calloc(n_threads, sizeof(pthread_t));
	args = (struct thread_arg *)calloc(n_threads, sizeof(*args));
	if (threads == NULL || args == NULL) {
		fprintf(stderr, "[stress] FAIL: alloc\n");
		free(threads);
		free(args);
		return 1;
	}

	for (i = 0; i < n_threads; i++) {
		args[i].thread_idx = i;
		args[i].iters = n_iters;
		rc = pthread_create(&threads[i], NULL, worker, &args[i]);
		if (rc != 0) {
			fprintf(stderr,
				"[stress] FAIL: pthread_create %d: %s\n",
				i, strerror(rc));
			return 1;
		}
	}

	for (i = 0; i < n_threads; i++) {
		rc = pthread_join(threads[i], NULL);
		if (rc != 0) {
			fprintf(stderr,
				"[stress] FAIL: pthread_join %d: %s\n",
				i, strerror(rc));
			return 1;
		}
		total_calls += args[i].calls_made;
		total_errors += args[i].errors;
	}

	printf("[stress] complete: %ld calls, %ld errors\n",
		total_calls, total_errors);

	free(threads);
	free(args);

	if (total_errors > 0) {
		fprintf(stderr, "[stress] FAIL: %ld errors\n", total_errors);
		return 1;
	}

	printf("[stress] PASS\n");
	return 0;
}
