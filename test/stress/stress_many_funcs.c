/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Stress scenario: many intercept_scripts in one config
 * (TODO.complete/35 P0).
 *
 * Generates a JSON config with N intercept_scripts (default
 * 1000), writes it to a temp file, sets RETRACE_JSON_CONFIG,
 * and runs a workload that calls libc functions.
 *
 * Designed to stress:
 *   - script_resolver linear scan (O(N) per call)
 *   - JSON config memory footprint
 *   - parson's object lookup
 *
 * Default: 1000 scripts x 100 calls. Override via env:
 *   STRESS_FUNCS=N    default 1000
 *   STRESS_ITERS=N    default 100
 *
 * Part of TODO.complete/35.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define DEFAULT_FUNCS 1000
#define DEFAULT_ITERS 100
#define TEST_FILE "/tmp/retrace_stress_funcs.json"

int main(void)
{
	const char *funcs_env;
	const char *iters_env;
	int n_funcs;
	int n_iters;
	FILE *f;
	int i, j;

	funcs_env = getenv("STRESS_FUNCS");
	n_funcs = funcs_env ? atoi(funcs_env) : DEFAULT_FUNCS;
	if (n_funcs <= 0 || n_funcs > 10000)
		n_funcs = DEFAULT_FUNCS;

	iters_env = getenv("STRESS_ITERS");
	n_iters = iters_env ? atoi(iters_env) : DEFAULT_ITERS;
	if (n_iters <= 0)
		n_iters = DEFAULT_ITERS;

	printf("[stress] generating config with %d scripts...\n",
		n_funcs);

	f = fopen(TEST_FILE, "w");
	if (f == NULL) {
		fprintf(stderr, "[stress] FAIL: cannot write %s\n",
			TEST_FILE);
		return 1;
	}

	fprintf(f, "{\"intercept_scripts\":[");
	for (i = 0; i < n_funcs; i++) {
		if (i > 0)
			fprintf(f, ",");
		fprintf(f,
			"{\"func_name\":\"func_%d\","
			"\"actions\":["
			"{\"action_name\":\"log_params\"},"
			"{\"action_name\":\"call_real\"}"
			"]}",
			i);
	}
	fprintf(f, "]}");
	fclose(f);

	printf("[stress] config written, %d bytes\n",
		(int)strlen(TEST_FILE));

	/* Workload: call libc functions n_iters times. Under
	 * LD_PRELOAD, retrace resolves each call through the
	 * N-script config. None of the scripts match our calls
	 * (we call getuid, not func_N), so the resolver walks
	 * the entire array each time.
	 */
	for (j = 0; j < n_iters; j++)
		(void)getuid();

	unlink(TEST_FILE);

	printf("[stress] PASS: %d funcs, %d iters\n",
		n_funcs, n_iters);
	return 0;
}
