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
 * Unit: the as-ops seam (ADR-0016).
 *
 * Part 1 -- the dispatcher: with no lane installed on the
 * thread context the default (trampoline) ops serve; an
 * installed table takes over every verb until cleared.
 *
 * Part 2 -- the syscall-lane mapping (pure, no tracee): cancel
 * denies with -ENOSYS, set_ret_val translates the libc
 * convention (-1 + errno) to the kernel's (-errno), the DEFERRED
 * sentinel suppresses a bogus exit override on an allowed call,
 * and call_real re-allows. These are the semantics the E2E
 * (test_ptrace_deny.py) rides end to end.
 */

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "arch_spec.h"
#include "as_ptrace.h"
#include "engine.h"

#include "../../src/backends/ptrace/translate.h"

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

/* --- dispatcher ------------------------------------------------------ */

static void *sched_ctx_seen;
static intptr_t stub_ret_val;

static void stub_sched(void *ctx, void *impl)
{
	(void) impl;
	sched_ctx_seen = ctx;
}

static void stub_cancel(void *ctx)
{
	(void) ctx;
}

static void stub_set_ret(void *ctx, intptr_t val)
{
	(void) ctx;
	stub_ret_val = val;
}

static int stub_setup(void *ctx, const struct FuncPrototype *proto,
	struct FuncParam params[], int *params_cnt)
{
	(void) ctx;
	(void) proto;
	(void) params;
	*params_cnt = 0;
	return 1;
}

static intptr_t stub_call(void *ctx, const void *impl,
	const struct FuncParam params[], int params_cnt)
{
	(void) ctx;
	(void) impl;
	(void) params;
	(void) params_cnt;
	return 42;
}

static const struct retrace_as_ops stub_ops = {
	.sched_real = stub_sched,
	.cancel_sched_real = stub_cancel,
	.set_ret_val = stub_set_ret,
	.setup_params = stub_setup,
	.call_real = stub_call
};

static struct ThreadContext *tc;

static void test_default_ops_when_unset(void)
{
	CHECK(retrace_as_ops_get() == &retrace_as_ops_default);
}

static void test_installed_ops_dispatch_and_clear(void)
{
	int dummy;
	int cnt = 8;

	retrace_as_ops_set(&stub_ops);
	CHECK(retrace_as_ops_get() == &stub_ops);

	sched_ctx_seen = NULL;
	retrace_as_sched_real(tc, &dummy, NULL);
	CHECK(sched_ctx_seen == &dummy);

	stub_ret_val = 0;
	retrace_as_set_ret_val(tc, &dummy, -7);
	CHECK(stub_ret_val == -7);

	CHECK(retrace_as_call_real(tc, NULL, NULL, NULL, 0) == 42);
	CHECK(retrace_as_setup_params(tc, NULL, NULL, NULL, &cnt) == 1);

	retrace_as_ops_set(NULL);
	CHECK(retrace_as_ops_get() == &retrace_as_ops_default);
}

static void test_nested_depth_uses_default(void)
{
	int dummy;

	retrace_as_ops_set(&stub_ops);
	tc->dispatch_depth = 2; /* nested: tracer's own libc */

	retrace_as_sched_real(tc, &dummy, NULL);
	/* stub would have recorded &dummy; the DEFAULT ops ran
	 * instead (no crash, no stub state change)
	 */
	CHECK(sched_ctx_seen != &dummy);

	tc->dispatch_depth = 0;
	retrace_as_ops_set(NULL);
}

/* --- syscall-lane mapping (pure frame, no tracee) -------------------- */

static struct retrace_ptrace_frame mkframe(void)
{
	struct retrace_ptrace_frame f;

	memset(&f, 0, sizeof(f));
	f.tracee_pid = 12345;
	return f;
}

static void install_ptrace_lane(void)
{
	retrace_as_ops_set(&retrace_as_ops_ptrace);
}

static void remove_ptrace_lane(void)
{
	retrace_as_ops_set(NULL);
}

static void test_cancel_denies_with_enosys(void)
{
	struct retrace_ptrace_frame f = mkframe();

	install_ptrace_lane();
	retrace_as_cancel_sched_real(tc, &f);
	CHECK(f.skip_real == 1);
	CHECK(f.forced_retval == -ENOSYS);
	remove_ptrace_lane();
}

static void test_deny_translates_errno_convention(void)
{
	struct retrace_ptrace_frame f = mkframe();

	install_ptrace_lane();
	retrace_as_cancel_sched_real(tc, &f);

	/* the sandbox deny shape: ret_val -1, ret_errno EACCES --
	 * recorded at deny time (the live errno is clobbered by
	 * the logging before the engine tail; CI-observed)
	 */
	tc->ret_errno = EACCES;
	errno = ENOSYS;
	retrace_as_set_ret_val(tc, &f, -1);
	CHECK(f.skip_real == 1);
	CHECK(f.forced_retval == -EACCES);
	tc->ret_errno = 0;
	remove_ptrace_lane();
}

static void test_deny_errno_clobber_falls_back_to_eperm(void)
{
	struct retrace_ptrace_frame f = mkframe();

	install_ptrace_lane();
	retrace_as_cancel_sched_real(tc, &f);

	tc->ret_errno = 0;
	errno = 0;
	retrace_as_set_ret_val(tc, &f, -1);
	CHECK(f.forced_retval == -EPERM);
	remove_ptrace_lane();
}

static void test_call_real_allows_and_defers(void)
{
	struct retrace_ptrace_frame f = mkframe();
	intptr_t sentinel;

	install_ptrace_lane();
	retrace_as_cancel_sched_real(tc, &f);

	sentinel = retrace_as_call_real(tc, &f, NULL, NULL, 0);
	CHECK(sentinel > 0);
	CHECK(f.skip_real == 0);

	/* the engine tail's write of the sentinel must NOT become an
	 * exit override
	 */
	retrace_as_set_ret_val(tc, &f, sentinel);
	CHECK(f.exit_override == 0);

	/* a real modify after the allow DOES override, at the exit */
	retrace_as_set_ret_val(tc, &f, 3);
	CHECK(f.exit_override == 1);
	CHECK(f.exit_retval == 3);
	remove_ptrace_lane();
}

static void test_modify_without_call_real_forces_value(void)
{
	struct retrace_ptrace_frame f = mkframe();

	install_ptrace_lane();
	retrace_as_cancel_sched_real(tc, &f);

	retrace_as_set_ret_val(tc, &f, -5);
	CHECK(f.skip_real == 1);
	CHECK(f.forced_retval == -5);
	CHECK(f.exit_override == 0);
	remove_ptrace_lane();
}

static void test_sched_real_allows(void)
{
	struct retrace_ptrace_frame f = mkframe();

	install_ptrace_lane();
	retrace_as_cancel_sched_real(tc, &f);
	retrace_as_sched_real(tc, &f, NULL);
	CHECK(f.skip_real == 0);
	remove_ptrace_lane();
}

int
main(void)
{
	printf("as-ops seam (ADR-0016)\n");

	tc = retrace_thread_context_get();
	CHECK(tc != NULL);

	TEST(default_ops_when_unset);
	TEST(installed_ops_dispatch_and_clear);
	TEST(nested_depth_uses_default);
	TEST(cancel_denies_with_enosys);
	TEST(deny_translates_errno_convention);
	TEST(deny_errno_clobber_falls_back_to_eperm);
	TEST(call_real_allows_and_defers);
	TEST(modify_without_call_real_forces_value);
	TEST(sched_real_allows);

	printf("%d run, %d pass, %d fail\n",
		tests_run, tests_pass, tests_fail);
	return tests_fail == 0 ? 0 : 1;
}
