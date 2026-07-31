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
 * call_count_limit -- fail the call once the per-function invocation
 * count crosses a threshold.
 *
 * Pairs naturally with modify_return_value_int to simulate resource
 * exhaustion: "let the first N opens succeed, then fail every open
 * after that with -ENOENT."
 *
 * Counts are PER FUNCTION NAME across the whole process. The first
 * action_params to fire for a given function name claims that
 * function's counter; subsequent invocations with different limits
 * on the same function are ignored (logged as a warning).
 *
 * Run BEFORE call_real so the limit fires before the real libc call
 * — pair with modify_return_value_int to actually fail the call.
 *
 * action_params:
 *   limit    - int. After this many calls, the action starts
 *              returning -1 (which aborts the rest of the script).
 *              Required.
 *
 * Example JSON (allow 5 mallocs, then fail):
 *   {
 *     "func_name": "malloc",
 *     "actions": [
 *       { "action_name": "call_count_limit",
 *         "action_params": { "limit": 5 } },
 *       { "action_name": "call_real" },
 *       { "action_name": "modify_return_value_int",
 *         "action_params": { "retval_int": 0 } }
 *     ]
 *   }
 *
 * The first 5 calls run call_real then modify_return_value_int
 * (which sets ret to 0, simulating malloc failure). The 6th call
 * hits call_count_limit first, which aborts the script —
 * modify_return_value_int never runs, so the real malloc result
 * passes through unchanged.
 *
 * To actually fail the call with a specific error code, swap the
 * order so call_count_limit is BEFORE modify_return_value_int,
 * and put the modify_return_value_int before call_real:
 *
 *   "actions": [
 *     { "action_name": "modify_return_value_int",
 *       "action_params": { "retval_int": 0 } },
 *     { "action_name": "call_count_limit",
 *       "action_params": { "limit": 5 } }
 *   ]
 *
 * First 5 calls: modify_return_value_int sets ret to 0; limit
 * doesn't fire (under threshold). 6th call: modify_return_value_int
 * still sets ret to 0; limit fires and aborts — call_real is
 * skipped, so the caller sees the 0 return value without the real
 * malloc actually running. Effective behavior: first 5 mallocs
 * return real pointer; 6th+ return NULL.
 *
 * If this is confusing, prefer pairing with memory_fuzz which has
 * clearer semantics for "fail N% of the time."
 */

#define MAX_TRACKED_FUNCS 64

struct count_entry {
	char name[MAXLEN_FUNC_NAME + 1];
	long count;
	long limit;
};

static struct count_entry g_entries[MAX_TRACKED_FUNCS];

static struct count_entry *find_or_claim(const char *name, long limit)
{
	int i;
	int first_empty = -1;

	for (i = 0; i < MAX_TRACKED_FUNCS; i++) {
		if (g_entries[i].name[0] != '\0' &&
			retrace_real_impls.strcmp(g_entries[i].name, name) == 0) {
			/* Already claimed. If the new limit differs, warn once. */
			if (g_entries[i].limit != limit) {
				log_warn(
					"call_count_limit: '%s' already tracked with limit %ld; ignoring new limit %ld",
					name, g_entries[i].limit, limit);
			}
			return &g_entries[i];
		}
		if (first_empty < 0 && g_entries[i].name[0] == '\0')
			first_empty = i;
	}

	if (first_empty < 0) {
		log_err("call_count_limit: tracking table full (%d entries)",
			MAX_TRACKED_FUNCS);
		return NULL;
	}

	/* Claim the slot. The race here (two threads claiming the same
	 * slot for different functions) is benign for the testing use
	 * case — worst case the limit silently doesn't apply for one
	 * function. Acceptable for a fault-injection tool.
	 */
	retrace_real_impls.strcpy(g_entries[first_empty].name, name);
	g_entries[first_empty].count = 0;
	g_entries[first_empty].limit = limit;
	return &g_entries[first_empty];
}

static int ia_call_count_limit(struct ThreadContext *t_ctx,
			       const JSON_Object *action_params)
{
	double limit_d;
	long limit;
	struct count_entry *entry;
	const char *func_name;

	if (action_params == NULL) {
		log_err("action_params required for call_count_limit");
		return -1;
	}

	if (!json_object_has_value(action_params, "limit")) {
		log_err("'limit' (int) required for call_count_limit");
		return -1;
	}

	limit_d = json_object_get_number(action_params, "limit");
	limit = (long)limit_d;

	if (t_ctx->prototype == NULL || t_ctx->prototype->name[0] == '\0') {
		log_err("call_count_limit: no prototype name on thread ctx");
		return -1;
	}
	func_name = t_ctx->prototype->name;

	entry = find_or_claim(func_name, limit);
	if (entry == NULL)
		return 0;  /* table full; let the call through */

	entry->count++;

	if (entry->count > entry->limit) {
		log_info("call_count_limit: '%s' hit limit %ld (count=%ld) -- aborting script",
			func_name, entry->limit, entry->count);
		return -1;
	}

	return 0;
}

retrace_actions_define_package(call_count_limit) = {
	{
		.name = "call_count_limit",
		.action = ia_call_count_limit
	}
};
