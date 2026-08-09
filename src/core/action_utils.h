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
 * Shared utilities for action implementations (TODO 14 / DRY).
 *
 * The param-by-name lookup pattern was duplicated across filter.c,
 * decode_http.c, decode_dns.c, and basic.c (5 sites). This header
 * centralizes it. Actions include this header and call
 * retrace_action_find_param() instead of reimplementing the loop.
 *
 * Header-only (static inline). No new .c file or link dependency.
 */

#ifndef RETRACE_CORE_ACTION_UTILS_H_
#define RETRACE_CORE_ACTION_UTILS_H_

#include "engine.h"
#include "real_impls.h"

/*
 * Find a param by name in the thread context's params array.
 * Returns the index (0-based), or -1 if not found.
 *
 * Uses retrace_real_impls.strcmp (not raw strcmp) to avoid
 * reentering the engine through the string interception
 * trampoline.
 */
static inline int retrace_action_find_param(
	const struct ThreadContext *t_ctx, const char *name)
{
	int i;

	for (i = 0; i < t_ctx->params_cnt; i++) {
		if (retrace_real_impls.strcmp(
			    t_ctx->params[i].param_meta.name, name) == 0)
			return i;
	}

	return -1;
}

#endif /* RETRACE_CORE_ACTION_UTILS_H_ */
