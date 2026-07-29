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

#include <stdlib.h>

#include "actions.h"
#include "logger.h"
#include "real_impls.h"

/*
 * fuzzing_seed -- make memory_fuzz deterministic.
 *
 * Port of v1's fuzzingseed. By default memory_fuzz uses rand() seeded
 * from the PID, so failures are random per run. Setting a seed makes
 * the failure sequence reproducible -- essential for debugging.
 *
 * State is process-global (matches v1): one seed per process, applied
 * to every thread's memory_fuzz invocations.
 *
 * action_params:
 *   seed  - unsigned int. Any value. Zero is a valid seed.
 *
 * The seed is applied lazily on the first memory_fuzz call AFTER this
 * action runs, via srand(). The constructor path seeds from getpid(),
 * so if fuzzing_seed is never invoked, behavior is the pre-existing
 * non-deterministic mode.
 *
 * Example JSON:
 *   {
 *     "action_name": "fuzzing_seed",
 *     "action_params": { "seed": 1498729252 }
 *   }
 */

static unsigned int g_fuzzing_seed;
static int g_fuzzing_seed_set;

static int ia_fuzzing_seed(struct ThreadContext *t_ctx,
			   const JSON_Object *action_params)
{
	double seed_value;

	(void)t_ctx;

	if (action_params == NULL) {
		log_err("action_params required for fuzzing_seed");
		return -1;
	}

	if (!json_object_has_value(action_params, "seed")) {
		log_err("'seed' (unsigned int) required for fuzzing_seed");
		return -1;
	}

	seed_value = json_object_get_number(action_params, "seed");
	g_fuzzing_seed = (unsigned int)seed_value;
	g_fuzzing_seed_set = 1;

	/* Apply immediately so subsequent rand() calls in this thread
	 * (and the next memory_fuzz call anywhere) are deterministic.
	 */
	srand(g_fuzzing_seed);

	log_info("fuzzing_seed: set to %u", g_fuzzing_seed);

	return 0;
}

/*
 * Called by memfuzz.c::ia_memory_fuzz on each invocation. If a seed
 * has been set, re-seed before the rand() call so we're deterministic
 * across the whole run. This is cheap (srand is fast) and only runs
 * if the user explicitly set a seed.
 */
void retrace_actions_fuzzing_seed_maybe_apply(void)
{
	if (g_fuzzing_seed_set)
		srand(g_fuzzing_seed);
}

retrace_actions_define_package(fuzzing_seed) = {
	{
		.name = "fuzzing_seed",
		.action = ia_fuzzing_seed
	}
};
