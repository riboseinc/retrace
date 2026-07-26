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

#include <errno.h>

#include "actions.h"
#include "logger.h"
#include "real_impls.h"
#include "data_types.h"

static int initialized;

static int ia_memory_fuzz
	(struct ThreadContext *t_ctx,
		const JSON_Object *action_params)
{
	double fail_rate;
	long random_value;

	if (action_params == NULL) {
		log_err("action_params must exists for modify_return_value_int");
		return -1;
	}

	if (!initialized) {
		initialized = 1;

		if (json_object_has_value(action_params, "fuzz_seed")) {
			double fuzz_seed;

			fuzz_seed = json_object_get_number(action_params, "fuzz_seed");

			srand(fuzz_seed);
		} else {
			srand(retrace_real_impls.time(NULL));
		}
	}

	if (!json_object_has_value(action_params, "fail_rate")) {
		log_err("fail_rate must exist in action_params "
			"for memory_fuzz");
		return -1;
	}

	fail_rate = json_object_get_number(action_params, "fail_rate");

	random_value = rand();
	if (random_value <= ((double) RAND_MAX * fail_rate)) {
		errno = ENOMEM;
		t_ctx->ret_val = (long) NULL;

		log_info("Failed memory fuzz");
	} else
		log_info("Passed memory fuzz");

	/* 0 indicates successful processing */
	return 0;
}

retrace_actions_define_package(basic) = {
	{
		.name = "memory_fuzz",
		.action = ia_memory_fuzz
	}
};
