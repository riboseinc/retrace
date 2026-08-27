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

#include <string.h>

#include "actions.h"
#include "real_impls.h"

/*
 * freeze -- suppress the real call entirely (TODO.supervisor/05).
 *
 * The "hold the specimen" verb: every function whose script
 * reaches a freeze action returns a fabricated value WITHOUT
 * the real implementation ever running. Under a wildcard
 * script the process keeps executing while making zero real
 * libc progress -- open() returns a failure, nothing is ever
 * read or written on disk.
 *
 * Deliberately silent per call. A freeze script fires on every
 * intercepted call; per-call logging or telemetry would flood
 * (and on the telemetry path, recurse). The audit trail is the
 * policy exchange itself: POLICY_SET -> swap -> POLICY_ACK,
 * journaled by the daemon.
 *
 * The fabricated value follows the sandbox deny rule: pointer
 * returns get NULL, everything else -1. Correct target code
 * must see a falsy failure, never a usable handle.
 *
 * action_params: none (presence is the instruction).
 */

/*
 * The quiet-hold exemption (the cookbook-39 lesson): a wildcard
 * freeze fabricates sleep() returns too, so frozen polling
 * loops spin hot -- the hold AMPLIFIES the load it was meant to
 * stop. Pure timeouts are inert by definition: passing them
 * through keeps the specimen quiet while everything else stays
 * frozen. nanosleep/clock_nanosleep never reach an action (no
 * prototype -> call real), so only the two wrapped time calls
 * need naming.
 */
static int is_pure_timeout(const char *name)
{
	size_t i;
	static const char *const timeouts[] = { "sleep", "usleep" };

	for (i = 0; i < sizeof(timeouts) / sizeof(timeouts[0]);
		i++) {
		if (retrace_real_impls.strcmp(name, timeouts[i]) == 0)
			return 1;
	}
	return 0;
}

static int ia_freeze(struct ThreadContext *t_ctx,
		     const JSON_Object *action_params)
{
	(void)action_params;

	if (t_ctx->prototype != NULL &&
	    is_pure_timeout(t_ctx->prototype->name))
		return 0;	/* let the real timeout run: quiet hold */

	if (t_ctx->prototype != NULL &&
	    retrace_real_impls.strcmp(
		    t_ctx->prototype->type_name, "ptr") == 0)
		t_ctx->ret_val = 0;
	else
		t_ctx->ret_val = -1;
	return -1;	/* abort the script: call_real never runs */
}

retrace_actions_define_package(freeze) = {
	{
		.name = "freeze",
		.action = ia_freeze
	}
};
