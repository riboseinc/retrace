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
 * freeze action tests (TODO.supervisor/09 quiet hold):
 *
 *   - a pointer-returning function freezes to ret 0, an int
 *     function to -1, and the script aborts (call_real never
 *     runs);
 *   - the pure-timeout family (sleep, usleep) is EXEMPT: the
 *     action returns "continue" so the real timeout runs and a
 *     frozen polling loop stays quiet instead of spinning.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "engine.h"
#include "actions.h"
#include "funcs.h"
#include "data_types.h"
#include "real_impls.h"

typedef int (*action_fn_t)(struct ThreadContext *t_ctx,
			    const JSON_Object *action_params);

static int tests_run;
static int tests_pass;
static int tests_fail;

#define TEST(name) do { \
	tests_run++; \
	printf("  TEST %s ... ", #name); \
	test_##name(); \
	tests_pass++; \
	printf("OK\n"); \
} while (0)

#define CHECK(cond) do { \
	if (!(cond)) { \
		printf("FAIL [%s:%d] %s\n", __FILE__, __LINE__, #cond); \
		tests_fail++; \
		return; \
	} \
} while (0)

static void test_lookup(void)
{
	retrace_actions_init();
	CHECK(retrace_actions_get("freeze") != NULL);
}

static void test_ptr_returns_null(void)
{
	action_fn_t action = retrace_actions_get("freeze");
	struct ThreadContext ctx;

	memset(&ctx, 0, sizeof(ctx));
	ctx.prototype = retrace_func_get("fopen");
	CHECK(ctx.prototype != NULL);
	CHECK(action(&ctx, NULL) == -1);
	CHECK(ctx.ret_val == 0);
}

static void test_int_returns_minus_one(void)
{
	action_fn_t action = retrace_actions_get("freeze");
	struct ThreadContext ctx;

	memset(&ctx, 0, sizeof(ctx));
	ctx.prototype = retrace_func_get("open");
	CHECK(ctx.prototype != NULL);
	CHECK(action(&ctx, NULL) == -1);
	CHECK(ctx.ret_val == (uint64_t)-1);
}

static void test_sleep_exempt(void)
{
	action_fn_t action = retrace_actions_get("freeze");
	struct ThreadContext ctx;

	memset(&ctx, 0, sizeof(ctx));
	ctx.prototype = retrace_func_get("sleep");
	CHECK(ctx.prototype != NULL);
	/* continue: the real sleep runs -- the quiet hold */
	CHECK(action(&ctx, NULL) == 0);
}

static void test_usleep_exempt(void)
{
	action_fn_t action = retrace_actions_get("freeze");
	struct ThreadContext ctx;

	memset(&ctx, 0, sizeof(ctx));
	ctx.prototype = retrace_func_get("usleep");
	CHECK(ctx.prototype != NULL);
	CHECK(action(&ctx, NULL) == 0);
}

int main(void)
{
	retrace_real_impls.strcmp = strcmp;
	retrace_real_impls.strlen = strlen;
	retrace_real_impls.strcpy = strcpy;
	retrace_real_impls.strncmp = strncmp;
	retrace_real_impls.memset = memset;
	retrace_real_impls.memcpy = memcpy;
	retrace_real_impls.malloc = malloc;
	retrace_real_impls.free = free;
	retrace_real_impls.real_snprintf = snprintf;

	printf("freeze action tests:\n");
	TEST(lookup);
	TEST(ptr_returns_null);
	TEST(int_returns_minus_one);
	TEST(sleep_exempt);
	TEST(usleep_exempt);

	printf("%d tests: %d pass, %d fail\n", tests_run, tests_pass,
		tests_fail);
	return tests_fail == 0 ? 0 : 1;
}
