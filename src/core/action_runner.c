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

#include "action_runner.h"


#include "actions.h"
#include "logger.h"
#include "posix_compat.h"
#include "win_diag.h"

void retrace_action_runner_run(struct ThreadContext *thread_ctx,
	const char *func_name,
	const JSON_Object *i_script)
{
	const JSON_Array *i_actions;
	const JSON_Object *i_action;
	const char *i_action_name;
	int (*action_func)(struct ThreadContext *t_ctx,
		const JSON_Object *action_params);
	size_t i;

	i_actions = json_object_get_array(i_script, "actions");
	if (i_actions == NULL) {
		log_warn(
			"i_script for %s:%p does not contain actions array",
			func_name,
			thread_ctx->ret_addr);
		return;
	}

	if (!json_array_get_count(i_actions)) {
		log_warn(
			"i_script for %s:%p contains empty actions array",
			func_name,
			thread_ctx->ret_addr);
		return;
	}

	for (i = 0; i < json_array_get_count(i_actions); i++) {
		i_action = json_array_get_object(i_actions, i);
		i_action_name = json_object_get_string(i_action, "action_name");
		if (i_action_name == NULL) {
			log_err(
				"action idx: %d for %s:%p has no action_name "
				"aborting script",
				(int)i,
				func_name,
				thread_ctx->ret_addr);
			break;
		}

		retrace_win_diag("runner-get", i_action_name, 0);
		action_func = retrace_actions_get(i_action_name);
		if (action_func == NULL) {
			log_err(
				"action idx: %d for %s:%p "
				"is not supported '%s', aborting script",
				(int)i,
				func_name,
				thread_ctx->ret_addr,
				i_action_name);
			break;
		}

		retrace_win_diag("runner-info", i_action_name, (long)i);

		log_info("Running action %s, for %s:%p, tpid 0x%llx...",
			i_action_name,
			func_name,
			thread_ctx->ret_addr,
			(unsigned long long)rc_thread_self());
		retrace_win_diag("runner-info-done", i_action_name, 0);

		if (action_func(thread_ctx,
			json_object_get_object(i_action, "action_params"))) {
			log_warn(
				"action %s, for %s:%p, tpid 0x%llx aborted the script",
				i_action_name,
				func_name,
				thread_ctx->ret_addr,
				(unsigned long long)rc_thread_self());
			break;
		}
		retrace_win_diag("runner-act-done", i_action_name, 0);
	}
}
