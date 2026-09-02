/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

#include <stddef.h>

#include "actions.h"
#include "logger.h"
#include "real_impls.h"

/*
 * fail_first -- fail the first N invocations with a chosen
 * return value, then let every later call through untouched.
 *
 * The mirror of call_count_limit: that one simulates resource
 * EXHAUSTION (calls 1..N pass, N+1.. fail); this one simulates
 * the TRANSIENT fault (calls 1..N fail, the rest recover) --
 * the retry-path primitive. A library that retries an EAGAIN
 * can now be proven correct, not just broken: fail_first two
 * EAGAINs and watch the third call succeed -- or watch the
 * untested infinite retry loop hang.
 *
 * Counts are PER FUNCTION NAME across the whole process, first
 * action_params to fire for a name claims its counter (same
 * ownership rule as call_count_limit; the two share no state,
 so a function may carry both -- transient fault on the way in,
 exhaustion eventually).
 *
 * Run BEFORE call_real: inside the failing window the action
 * sets ret_val and aborts the script, so the real libc call is
 * never made (the engine already cancelled it when the script
 * matched); outside the window it returns 0 and the script
 * proceeds -- put call_real after it and the call recovers.
 *
 * action_params:
 *   fails      - int. How many leading invocations fail.
 *                Required. 0 = pass everything through (the
 *                action is inert; useful for flipping a test
 *                from fault to baseline without editing the
 *                script shape).
 *   retval_int - int. The return value the failing calls
 *                synthesize (an errno-like int for the
 *                libc-level contract). Required.
 *
 * Example JSON (open: two EAGAINs, then reality):
 *   {
 *     "func_name": "open",
 *     "actions": [
 *       { "action_name": "fail_first",
 *         "action_params": { "fails": 2, "retval_int": -11 } },
 *       { "action_name": "call_real" },
 *       { "action_name": "log_params" }
 *     ]
 *   }
 */

#define MAX_TRACKED_FUNCS 64

struct fail_entry {
	char name[MAXLEN_FUNC_NAME + 1];
	long count;
	long fails;
	long retval;
};

static struct fail_entry g_entries[MAX_TRACKED_FUNCS];

static struct fail_entry *find_or_claim(const char *name,
	long fails)
{
	int i;
	int first_empty = -1;

	for (i = 0; i < MAX_TRACKED_FUNCS; i++) {
		if (g_entries[i].name[0] != '\0' &&
			retrace_real_impls.strcmp(g_entries[i].name,
				name) == 0) {
			if (g_entries[i].fails != fails) {
				log_warn(
					"fail_first: '%s' already tracked with fails %ld; ignoring new %ld",
					name, g_entries[i].fails, fails);
			}
			return &g_entries[i];
		}
		if (first_empty < 0 && g_entries[i].name[0] == '\0')
			first_empty = i;
	}

	if (first_empty < 0) {
		log_err("fail_first: tracking table full (%d entries)",
			MAX_TRACKED_FUNCS);
		return NULL;
	}

	/* same benign-claim race note as call_count_limit: worst
	 * case the fault window silently doesn't apply for one
	 * function -- acceptable for a fault-injection tool
	 */
	retrace_real_impls.strcpy(g_entries[first_empty].name, name);
	g_entries[first_empty].count = 0;
	g_entries[first_empty].fails = fails;
	return &g_entries[first_empty];
}

static int ia_fail_first(struct ThreadContext *t_ctx,
	const JSON_Object *action_params)
{
	double d;
	long fails, retval;
	struct fail_entry *entry;
	const char *func_name;

	if (action_params == NULL) {
		log_err("action_params required for fail_first");
		return -1;
	}
	if (!json_object_has_value(action_params, "fails") ||
	    !json_object_has_value(action_params, "retval_int")) {
		log_err("'fails' and 'retval_int' required for fail_first");
		return -1;
	}

	d = json_object_get_number(action_params, "fails");
	fails = (long)d;
	d = json_object_get_number(action_params, "retval_int");
	retval = (long)d;

	if (t_ctx->prototype == NULL || t_ctx->prototype->name[0] == '\0') {
		log_err("fail_first: no prototype name on thread ctx");
		return -1;
	}
	func_name = t_ctx->prototype->name;

	entry = find_or_claim(func_name, fails);
	if (entry == NULL)
		return 0;	/* table full; let the call through */

	entry->count++;
	entry->retval = retval;

	if (entry->count <= entry->fails) {
		t_ctx->ret_val = entry->retval;
		log_info("fail_first: '%s' failing call %ld/%ld with %ld -- real not called",
			func_name, entry->count, entry->fails,
			entry->retval);
		/* abort the script: call_real never runs, ret_val is
		 * the transient fault the caller must retry through
		 */
		return -1;
	}

	log_dbg("fail_first: '%s' recovered at call %ld",
		func_name, entry->count);
	return 0;
}

retrace_actions_define_package(fail_first) = {
	{
		.name = "fail_first",
		.action = ia_fail_first
	}
};
