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

/*
 * filter -- conditional guard for intercept scripts (TODO.complete/20).
 *
 * The simplest possible filter DSL: one action that evaluates a
 * single comparison on a named param. If the comparison is false,
 * the action returns -1, aborting the rest of the script (no
 * logging, no modification, no call_real).
 *
 * This is NOT a full expression language. It's a building block.
 * Users compose multiple filter actions for AND semantics:
 *
 *   "actions": [
 *     { "action_name": "filter",
 *       "action_params": { "param_name": "flags", "op": "==", "value": 0 } },
 *     { "action_name": "filter",
 *       "action_params": { "param_name": "mode",  "op": ">",  "value": 0 } },
 *     { "action_name": "log_params" },
 *     { "action_name": "call_real" }
 *   ]
 *
 * If flags != 0, the first filter aborts; log_params never runs.
 * If mode <= 0, the second filter aborts. Both must pass for
 * log_params + call_real to execute.
 *
 * For OR semantics, use multiple intercept_scripts with the same
 * func_name but different filters (first match wins).
 *
 * action_params:
 *   param_name  - string, required. Name of the param to compare.
 *   op          - string, required. One of: ==, !=, >, <, >=, <=
 *   value       - number, required. The comparison value.
 *
 * Returns 0 if the comparison is true (script continues).
 * Returns -1 if false or on error (script aborts).
 *
 * Part of TODO.complete/20.
 */

#include <string.h>

#include "actions.h"
#include "logger.h"
#include "real_impls.h"
#include "action_utils.h"

static int eval_op(long actual, const char *op, long expected)
{
	if (retrace_real_impls.strcmp(op, "==") == 0)
		return actual == expected;
	if (retrace_real_impls.strcmp(op, "!=") == 0)
		return actual != expected;
	if (retrace_real_impls.strcmp(op, ">") == 0)
		return actual > expected;
	if (retrace_real_impls.strcmp(op, "<") == 0)
		return actual < expected;
	if (retrace_real_impls.strcmp(op, ">=") == 0)
		return actual >= expected;
	if (retrace_real_impls.strcmp(op, "<=") == 0)
		return actual <= expected;

	log_err("filter: unknown operator '%s'", op);
	return -1;
}

static int ia_filter(struct ThreadContext *t_ctx,
		     const JSON_Object *action_params)
{
	const char *param_name;
	const char *op;
	double value;
	long expected;
	long actual;
	int param_idx;
	int result;

	if (action_params == NULL) {
		log_err("filter: action_params required");
		return -1;
	}

	param_name = json_object_get_string(action_params, "param_name");
	if (param_name == NULL) {
		log_err("filter: param_name required");
		return -1;
	}

	op = json_object_get_string(action_params, "op");
	if (op == NULL) {
		log_err("filter: op required (==, !=, >, <, >=, <=)");
		return -1;
	}

	if (!json_object_has_value(action_params, "value")) {
		log_err("filter: value required");
		return -1;
	}

	value = json_object_get_number(action_params, "value");
	expected = (long)value;

	param_idx = retrace_action_find_param(t_ctx, param_name);
	if (param_idx < 0) {
		log_dbg("filter: param '%s' not in frame -- aborting",
			param_name);
		return -1;
	}

	actual = t_ctx->params[param_idx].val;

	result = eval_op(actual, op, expected);
	if (result < 0)
		return -1;

	if (result) {
		log_dbg("filter: %s(%ld) %s %ld -- PASS",
			param_name, actual, op, expected);
		return 0;
	}

	log_dbg("filter: %s(%ld) %s %ld -- FAIL (aborting script)",
		param_name, actual, op, expected);
	return -1;
}

retrace_actions_define_package(filter) = {
	{
		.name = "filter",
		.action = ia_filter
	}
};
