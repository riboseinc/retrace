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
 * sandbox -- jail file access behind an explicit policy.
 *
 * A runtime security policy for file access. When applied to open,
 * openat, fopen, etc., the action checks the path argument against
 * the configured lists and blocks access on a match (or, in
 * allow mode, on a non-match).
 *
 * Run BEFORE call_real (or as the only action) so the real call
 * never executes for denied paths. The action sets ret_val to -1
 * and aborts the script.
 *
 * action_params (exactly one of the two):
 *   deny_paths  - JSON array of path strings to block. Exact match
 *                 or prefix match (if the entry ends with '/' or
 *                 '\'). Allow-by-default.
 *   allow_paths - JSON array of paths that MAY be touched.
 *                 Deny-by-default: anything not on the list is
 *                 blocked. This is the jail mode retrace-profile
 *                 emits (--jail-out).
 * When both are present a path must pass both checks.
 *
 * Example JSON (block access to /etc/shadow and /root/):
 *   {
 *     "func_name": "open",
 *     "actions": [
 *       { "action_name": "sandbox",
 *         "action_params": {
 *           "deny_paths": ["/etc/shadow", "/etc/sudoers", "/root/"]
 *         }
 *       },
 *       { "action_name": "call_real" }
 *     ]
 *   }
 */

/*
 * One list check. Returns 1 when path_arg matches entry (exact, or
 * prefix when the entry ends with a separator).
 */
static int path_matches(const char *path_arg, const char *entry)
{
	size_t elen = retrace_real_impls.strlen(entry);
	char last;

	if (elen == 0)
		return 0;
	last = entry[elen - 1];
	if (last == '/' || last == '\\') {
		/* Prefix match: everything under this directory */
		return retrace_real_impls.strncmp(path_arg, entry,
			elen) == 0;
	}
	return retrace_real_impls.strcmp(path_arg, entry) == 0;
}

static int list_contains(const char *path_arg, JSON_Array *list)
{
	size_t i, n = json_array_get_count(list);

	for (i = 0; i < n; i++) {
		const char *entry = json_array_get_string(list, i);

		if (entry != NULL && path_matches(path_arg, entry))
			return 1;
	}
	return 0;
}

static intptr_t deny_ret(const struct ThreadContext *t_ctx)
{
	if (t_ctx->prototype != NULL &&
	    t_ctx->prototype->type_name != NULL &&
	    retrace_real_impls.strcmp(
		    t_ctx->prototype->type_name, "ptr") == 0)
		return 0;
	return -1;
}

static int ia_sandbox(struct ThreadContext *t_ctx,
		      const JSON_Object *action_params)
{
	JSON_Array *deny_paths, *allow_paths;
	const char *path_arg = NULL;
	int arg_idx;

	if (action_params == NULL) {
		log_err("sandbox: action_params required");
		t_ctx->ret_val = -1;
		return -1;
	}

	deny_paths = json_object_get_array(action_params, "deny_paths");
	allow_paths = json_object_get_array(action_params, "allow_paths");
	if (deny_paths == NULL && allow_paths == NULL) {
		log_err("sandbox: 'deny_paths' or 'allow_paths' array required");
		t_ctx->ret_val = -1;
		return -1;
	}

	/*
	 * Find the path argument via prototype metadata: the first
	 * string param among the first two args (open: path; openat:
	 * dirfd, path). Non-pointer params (close(fd)) are skipped --
	 * comparing those as strings would dereference garbage.
	 */
	for (arg_idx = 0; arg_idx < t_ctx->params_cnt && arg_idx < 2; arg_idx++) {
		const struct ParamMeta *pm =
			&t_ctx->params[arg_idx].param_meta;

		if ((pm->modifiers & CDM_POINTER) &&
		    t_ctx->params[arg_idx].val != 0 &&
		    retrace_real_impls.strncmp(pm->ref_type_name,
			"sz", 3) == 0) {
			path_arg = (const char *)t_ctx->params[arg_idx].val;
			break;
		}
	}

	/*
	 * No path argument (non-file function under a wildcard
	 * script): nothing to police, let the call through.
	 */
	if (path_arg == NULL)
		return 0;

	/*
	 * Denial return value is PROTOTYPE-DRIVEN (TODO 17): a
	 * synthesized -1 for a pointer-returning function hands the
	 * caller (FILE *)-1 -- != NULL, so correct code USES it and
	 * crashes. Pointer returns deny with NULL; int returns with
	 * -1 (POSIX open-family error).
	 */
	if (allow_paths != NULL && !list_contains(path_arg, allow_paths)) {
		log_warn("sandbox: DENIED '%s' (not in allow_paths)",
			path_arg);
		errno = EACCES;
		t_ctx->ret_val = deny_ret(t_ctx);
		return -1;
	}
	if (deny_paths != NULL && list_contains(path_arg, deny_paths)) {
		log_warn("sandbox: DENIED '%s' (matches deny_paths)",
			path_arg);
		errno = EACCES;
		t_ctx->ret_val = deny_ret(t_ctx);
		return -1;
	}

	return 0;
}

retrace_actions_define_package(sandbox) = {
	{
		.name = "sandbox",
		.action = ia_sandbox
	}
};
