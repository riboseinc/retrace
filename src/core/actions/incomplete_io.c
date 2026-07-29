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

#include "actions.h"
#include "logger.h"
#include "real_impls.h"

/*
 * incomplete_io -- partially fail I/O calls (short reads/writes).
 *
 * Port of v1's incompleteio. Useful for testing programs' handling of
 * short I/O -- the kernel / libc is allowed to return fewer bytes
 * than requested, but most programs assume the full count and break
 * when that assumption is violated.
 *
 * Run AFTER call_real so t_ctx->ret_val holds the real byte count.
 * Truncates ret_val to (real * rate). A rate of 0.0 forces every call
 * to look like EOF; 1.0 is a no-op; 0.5 returns half the bytes.
 *
 * Only meaningful for: read, write, readv, writev, fread, fwrite,
 * recv, send, recvfrom, sendto. Applying it to other functions is
 * harmless but pointless.
 *
 * action_params:
 *   rate     - float in [0.0, 1.0]. Fraction of real bytes to return.
 *
 * Example JSON:
 *   {
 *     "action_name": "incomplete_io",
 *     "action_params": { "rate": 0.5 }
 *   }
 */

static int ia_incomplete_io(struct ThreadContext *t_ctx,
			    const JSON_Object *action_params)
{
	double rate;
	long real_ret;

	if (action_params == NULL) {
		log_err("action_params required for incomplete_io");
		return -1;
	}

	if (!json_object_has_value(action_params, "rate")) {
		log_err("'rate' (0.0..1.0) required for incomplete_io");
		return -1;
	}

	rate = json_object_get_number(action_params, "rate");

	if (rate < 0.0)
		rate = 0.0;
	else if (rate > 1.0)
		rate = 1.0;

	real_ret = t_ctx->ret_val;

	/* Negative returns (errors) and zero returns (EOF) are passed
	 * through untouched. Only positive byte counts get truncated.
	 */
	if (real_ret <= 0)
		return 0;

	t_ctx->ret_val = (long)(real_ret * rate);

	log_info("incomplete_io: %ld -> %ld bytes (rate=%.3f)",
		real_ret, t_ctx->ret_val, rate);

	return 0;
}

retrace_actions_define_package(incomplete_io) = {
	{
		.name = "incomplete_io",
		.action = ia_incomplete_io
	}
};
