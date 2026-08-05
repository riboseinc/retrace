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
 * addr_deny -- deny-list network destinations.
 *
 * The network equivalent of `sandbox`. Where sandbox inspects path
 * arguments of open/fopen/openat and returns -1 on match, addr_deny
 * inspects sockaddr arguments of connect/bind/sendto/recvfrom and
 * returns -ECONNREFUSED on match.
 *
 * Why a separate action (not an extension of sandbox)
 * ===================================================
 *
 * sandbox's contract is "deny access to specific file paths."
 * Adding "and also network addresses" turns it into a denylist
 * for everything, violating MECE: the action name should encode
 * what it operates on. Keeping them separate also keeps each
 * action's JSON schema simple -- one array per action -- and
 * lets users combine them independently (sandbox paths in one
 * script, addr_deny in another, both attached to the same
 * function if desired).
 *
 * sockaddr extraction
 * ===================
 *
 * Different functions put the sockaddr at different parameter
 * indices:
 *   connect(sockfd, addr, addrlen)             -- params[1]
 *   bind(sockfd, addr, addrlen)                -- params[1]
 *   sendto(..., dest_addr, addrlen)            -- params[4]
 *   recvfrom(..., src_addr, addrlen)           -- params[4]
 *   accept(sockfd, addr, addrlen)              -- params[1]
 *   getpeername(sockfd, addr, addrlen)         -- params[1]
 *
 * To avoid hardcoding every function->index map, the action
 * scans t_ctx->params for the first parameter whose name is
 * "addr", "dest_addr", or "src_addr" and treats its value as
 * the sockaddr pointer. The prototype metadata (param_meta.name)
 * is the source of truth -- the action never inspects the
 * function name.
 *
 * action_params:
 *   deny_addrs - JSON array of "host:port" specs. Each spec is
 *                matched via retrace_sockaddr_match. Wildcards
 *                supported: "host:*", "*:port", "*:*" (deny all),
 *                "[::1]:443" (IPv6), "/var/run/x.sock" (AF_UNIX).
 *
 * Example JSON (block all outbound connects to port 443 and to
 * a specific IP):
 *   {
 *     "func_name": "connect",
 *     "actions": [
 *       { "action_name": "addr_deny",
 *         "action_params": {
 *           "deny_addrs": ["*:443", "10.0.0.1:*"]
 *         }
 *       },
 *       { "action_name": "call_real" }
 *     ]
 *   }
 *
 * On match, the action sets t_ctx->ret_val = -ECONNREFUSED and
 * returns -1 to abort the script (call_real will not run).
 */

#include <errno.h>
#include <string.h>

#include "actions.h"
#include "engine.h"
#include "logger.h"
#include "real_impls.h"
#include "sockaddr_inspect.h"

/* Names that identify a sockaddr argument in prototype metadata.
 * The action scans params and picks the first match.
 */
static int is_addr_param_name(const char *name)
{
	if (name == NULL)
		return 0;

	if (retrace_real_impls.strcmp(name, "addr") == 0)
		return 1;
	if (retrace_real_impls.strcmp(name, "dest_addr") == 0)
		return 1;
	if (retrace_real_impls.strcmp(name, "src_addr") == 0)
		return 1;
	return 0;
}

/* Find the sockaddr pointer among the call's params. Returns the
 * pointer, or NULL if no suitable param was found.
 */
static const void *find_sockaddr(struct ThreadContext *t_ctx)
{
	int i;

	for (i = 0; i < t_ctx->params_cnt; i++) {
		const char *name = t_ctx->params[i].param_meta.name;

		if (is_addr_param_name(name))
			return (const void *)t_ctx->params[i].val;
	}

	return NULL;
}

static int ia_addr_deny(struct ThreadContext *t_ctx,
			const JSON_Object *action_params)
{
	JSON_Array *deny_addrs;
	size_t i, n;
	const void *sa;
	struct retrace_sockaddr_info info;

	if (action_params == NULL) {
		log_err("addr_deny: action_params required");
		return -1;
	}

	deny_addrs = json_object_get_array(action_params, "deny_addrs");
	if (deny_addrs == NULL) {
		log_err("addr_deny: 'deny_addrs' array required");
		return -1;
	}

	sa = find_sockaddr(t_ctx);
	if (sa == NULL) {
		/* Not a function we can inspect. Let the script continue;
		 * call_real will run.
		 */
		return 0;
	}

	if (retrace_sockaddr_inspect(sa, 0, &info) != 0) {
		log_warn("addr_deny: could not inspect sockaddr; allowing");
		return 0;
	}

	n = json_array_get_count(deny_addrs);
	for (i = 0; i < n; i++) {
		const char *spec = json_array_get_string(deny_addrs, i);
		int match;

		if (spec == NULL)
			continue;

		match = retrace_sockaddr_match(&info, spec);
		if (match < 0) {
			log_warn("addr_deny: malformed spec '%s' ignored",
				spec);
			continue;
		}

		if (match) {
			log_warn("addr_deny: DENIED %s:%u (matches '%s')",
				info.ip[0] ? info.ip : "(unix)",
				(unsigned int)info.port, spec);
			t_ctx->ret_val = -ECONNREFUSED;
			return -1;
		}
	}

	return 0;
}

retrace_actions_define_package(addr_deny) = {
	{
		.name = "addr_deny",
		.action = ia_addr_deny
	}
};
