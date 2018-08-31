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

#ifndef SRC_RETRACE_V2_ACTIONS_H_
#define SRC_RETRACE_V2_ACTIONS_H_

#include "parson.h"
#include "engine.h"
#include "arch_spec_macros.h"
#include "conf.h"

#define MAXLEN_ACTION_NAME 32

int(*retrace_actions_get(const char *action_name))
	(struct ThreadContext *t_ctx,
		const rtr2_action_t *action);

int retrace_actions_init(void);

struct Action {
	char name[MAXLEN_ACTION_NAME + 1];

	int (*action)(struct ThreadContext *t_ctx,
			const JSON_Object *action_params);
};

#define retrace_actions_define_package(pkg_name) \
	retrace_as_define_var_in_sec(const struct Action,\
		retrace_actions_##pkg_name[], \
			"__DATA", "__retrace_acts")__attribute__((aligned(1)))

#endif /* SRC_RETRACE_V2_ACTIONS_H_ */
