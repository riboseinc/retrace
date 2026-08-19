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

#include <time.h>

#include "actions.h"
#include "posix_compat.h"
#include "logger.h"

/*
 * delay -- inject latency before the real libc call returns.
 *
 * Useful for distributed-systems / network testing: simulate slow
 * disk I/O, slow network, slow database. Catches race conditions,
 * timeout bugs, retry logic gaps.
 *
 * Run AFTER call_real so the delay applies to the call's apparent
 * wall-clock duration. Works on any function — the delay is added
 * regardless of which libc call it is.
 *
 * action_params:
 *   ms       - milliseconds to sleep. Default 0 (no-op).
 *
 * Example JSON:
 *   {
 *     "action_name": "delay",
 *     "action_params": { "ms": 100 }
 *   }
 *
 * Recipe (slow-down every open()):
 *   {
 *     "func_name": "open",
 *     "actions": [
 *       { "action_name": "call_real" },
 *       { "action_name": "delay", "action_params": { "ms": 50 } }
 *     ]
 *   }
 */

static int ia_delay(struct ThreadContext *t_ctx,
		    const JSON_Object *action_params)
{
	double ms;
	struct timespec ts;

	(void)t_ctx;

	if (action_params == NULL)
		return 0;  /* no params = no delay */

	if (!json_object_has_value(action_params, "ms"))
		return 0;  /* no "ms" = no delay */

	ms = json_object_get_number(action_params, "ms");
	if (ms <= 0.0)
		return 0;

	ts.tv_sec = (time_t)(ms / 1000.0);
	ts.tv_nsec = (long)((ms - (double)ts.tv_sec * 1000.0) * 1000000.0);

	/*
	 * Use nanosleep directly. nanosleep is NOT in funcs_symbols.S
	 * so retrace doesn't intercept it; the call goes straight to
	 * libc without re-entering the engine.
	 */
	rc_nanosleep(&ts);

	return 0;
}

retrace_actions_define_package(delay) = {
	{
		.name = "delay",
		.action = ia_delay
	}
};
