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
#include <string.h>

#include "actions.h"
#include "logger.h"
#include "real_impls.h"

/*
 * sandbox -- deny access to specific paths.
 *
 * A runtime security policy for file access. When applied to open,
 * openat, fopen, etc., the action checks the path argument against
 * a deny list and blocks access if matched.
 *
 * Run BEFORE call_real (or as the only action) so the real call
 * never executes for denied paths. The action sets ret_val to -1
 * and aborts the script.
 *
 * action_params:
 *   deny_paths - JSON array of path strings to block. Exact match
 *                or prefix match (if the entry ends with '/').
 *
 * Example JSON (block access to /etc/shadow and /root/):
 *   {
 *     "action_name": "sandbox",
 *     "action_params": {
 *       "deny_paths": ["/etc/shadow", "/etc/sudoers", "/root/"]
 *     }
 *   }
 *
 * Recipe (sandbox a binary so it can't read credentials):
 *   {
 *     "func_name": "open",
 *     "actions": [
 *       { "action_name": "sandbox",
 *         "action_params": {
 *           "deny_paths": ["/etc/shadow", "/etc/gshadow"]
 *         }
 *       },
 *       { "action_name": "call_real" }
 *     ]
 *   }
 */

static int ia_sandbox(struct ThreadContext *t_ctx,
		      const JSON_Object *action_params)
{
	JSON_Array *deny_paths;
	size_t i, n;
	const char *path_arg = NULL;
	int arg_idx;

	if (action_params == NULL) {
		log_err("sandbox: action_params required");
		return -1;
	}

	deny_paths = json_object_get_array(action_params, "deny_paths");
	if (deny_paths == NULL) {
		log_err("sandbox: 'deny_paths' array required");
		return -1;
	}

	/*
	 * Find the path argument. For open(), it's params[0].val.
	 * For fopen(), same. For openat(), it's params[1].val (dirfd,
	 * path). We check param[0] first, then param[1].
	 */
	for (arg_idx = 0; arg_idx < t_ctx->params_cnt && arg_idx < 2; arg_idx++) {
		if (t_ctx->params[arg_idx].val != 0) {
			path_arg = (const char *)t_ctx->params[arg_idx].val;
			break;
		}
	}

	if (path_arg == NULL)
		return 0;

	n = json_array_get_count(deny_paths);
	for (i = 0; i < n; i++) {
		const char *denied = json_array_get_string(deny_paths, i);
		size_t dlen;

		if (denied == NULL)
			continue;

		dlen = retrace_real_impls.strlen(denied);

		if (denied[dlen - 1] == '/') {
			/* Prefix match: block everything under this dir */
			if (retrace_real_impls.strncmp(path_arg, denied,
				dlen) == 0) {
				log_warn("sandbox: DENIED '%s' (matches prefix '%s')",
					path_arg, denied);
				t_ctx->ret_val = -1;
				return -1;
			}
		} else {
			/* Exact match */
			if (retrace_real_impls.strcmp(path_arg, denied) == 0) {
				log_warn("sandbox: DENIED '%s'", path_arg);
				t_ctx->ret_val = -1;
				return -1;
			}
		}
	}

	return 0;
}

retrace_actions_define_package(sandbox) = {
	{
		.name = "sandbox",
		.action = ia_sandbox
	}
};
