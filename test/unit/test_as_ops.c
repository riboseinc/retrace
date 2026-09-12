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
 * Unit: the as-ops seam (ADR-0016) -- STANDALONE.
 *
 * Compiles the dispatcher (as_ops.c) and the syscall-lane table
 * (as_ptrace.c) against fakes: no engine link, no constructor,
 * no interposition. The pure semantics -- default fallback,
 * installed-lane dispatch, depth gating, deny -> -errno, the
 * DEFERRED sentinel, the exit override -- pin here; the LIVE
 * behavior (a static victim denied under policy) is the E2E's
 * (test_ptrace_deny.py), which runs the same verbs through the
 * real engine on every CI leg.
 *
 * The standalone shape is not a convenience: engine-linked, the
 * test binary interposes its own libc and the default "*"
 * script's action phase runs on the test's own printfs -- under
 * clang+musl -O that corrupted the harness itself (the unit
 * crashed before its first CHECK on the alpine legs).
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arch_spec.h"
#include "as_ptrace.h"
#include "engine.h"
#include "real_impls.h"

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

/* ---- the fakes the compiled-in modules need ------------------------ */

struct RetraceRealImpls retrace_real_impls;

static void *fake_malloc(size_t n)
{
	return malloc(n);
}

static void fake_free(void *p)
{
	free(p);
}

static void *fake_memset(void *d, int c, size_t n)
{
	return memset(d, c, n);
}

static void *fake_memcpy(void *d, const void *s, size_t n)
{
	return memcpy(d, s, n);
}

static int fake_strcmp(const char *a, const char *b)
{
	return strcmp(a, b);
}

/* the dispatcher asks the thread context; standalone owns one */
static struct ThreadContext the_ctx;

/* fakes for what the compiled-in modules reference */
void retrace_logger_log(int module, int sev, const char *fmt, ...)
{
	(void) module;
	(void) sev;
	(void) fmt;
}

const struct DataType *retrace_datatype_get(const char *name)
{
	(void) name;
	return NULL;
}

struct ThreadContext *retrace_thread_context_get(void)
{
	return &the_ctx;
}

/* fake default ops: record what ran */
static void *def_sched_ctx;
static int def_ran;

static void def_sched(void *ctx, void *impl)
{
	(void) impl;
	def_sched_ctx = ctx;
	def_ran = 1;
}

static void def_cancel(void *ctx)
{
	(void) ctx;
	def_ran = 1;
}

static void def_set_ret(void *ctx, intptr_t val)
{
	(void) ctx;
	(void) val;
	def_ran = 1;
}

static int def_setup(void *ctx, const struct FuncPrototype *proto,
	struct FuncParam params[], int *params_cnt)
{
	(void) ctx;
	(void) proto;
	(void) params;
	*params_cnt = 0;
	def_ran = 1;
	return 1;
}

static intptr_t def_call(void *ctx, const void *impl,
	const struct FuncParam params[], int params_cnt)
{
	(void) ctx;
	(void) params;
	(void) params_cnt;
	def_ran = 1;
	return 7;
}

const struct retrace_as_ops retrace_as_ops_default = {
	.sched_real = def_sched,
	.cancel_sched_real = def_cancel,
	.set_ret_val = def_set_ret,
	.setup_params = def_setup,
	.call_real = def_call
};

/* stub lane: records the verb */
static void *stub_sched_ctx;
static intptr_t stub_ret_val;

static void stub_sched(void *ctx, void *impl)
{
	(void) impl;
	stub_sched_ctx = ctx;
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

static struct ThreadContext *tc = &the_ctx;

/* ---- dispatcher ----------------------------------------------------- */

static void test_default_ops_when_unset(void)
{
	CHECK(retrace_as_ops_get() == &retrace_as_ops_default);
}

static void test_installed_ops_dispatch_and_clear(void)
{
	static unsigned char frame[64];
	int cnt = 8;

	retrace_as_ops_set(&stub_ops);
	CHECK(retrace_as_ops_get() == &stub_ops);

	stub_sched_ctx = NULL;
	retrace_as_sched_real(tc, frame, NULL);
	CHECK(stub_sched_ctx == (void *) frame);

	stub_ret_val = 0;
	retrace_as_set_ret_val(tc, frame, -7);
	CHECK(stub_ret_val == -7);

	CHECK(retrace_as_call_real(tc, NULL, NULL, NULL, 0) == 42);
	CHECK(retrace_as_setup_params(tc, NULL, NULL, NULL,
		&cnt) == 1);

	retrace_as_ops_set(NULL);
	CHECK(retrace_as_ops_get() == &retrace_as_ops_default);
}

static void test_nested_depth_uses_default(void)
{
	def_ran = 0;
	retrace_as_ops_set(&stub_ops);
	tc->dispatch_depth = 2; /* nested: the tracer's own libc */

	stub_sched_ctx = NULL;
	retrace_as_sched_real(tc, NULL, NULL);
	CHECK(stub_sched_ctx == NULL); /* stub never ran */
	CHECK(def_ran == 1);          /* the default ops did */

	tc->dispatch_depth = 0;
	retrace_as_ops_set(NULL);
}

static void test_null_ctx_uses_default(void)
{
	def_ran = 0;
	retrace_as_ops_set(&stub_ops);

	retrace_as_sched_real(NULL, NULL, NULL);
	CHECK(def_ran == 1);

	retrace_as_ops_set(NULL);
}

/* ---- syscall-lane mapping (pure frame, no tracee) -------------------- */

static struct retrace_ptrace_frame mkframe(void)
{
	struct retrace_ptrace_frame f;

	memset(&f, 0, sizeof(f));
	f.tracee_pid = 12345;
	return f;
}

static void test_cancel_denies_with_enosys(void)
{
	struct retrace_ptrace_frame f = mkframe();

	retrace_as_ops_set(&retrace_as_ops_ptrace);
	retrace_as_cancel_sched_real(tc, &f);
	CHECK(f.skip_real == 1);
	CHECK(f.forced_retval == -ENOSYS);
	retrace_as_ops_set(NULL);
}

static void test_deny_translates_errno_convention(void)
{
	struct retrace_ptrace_frame f = mkframe();

	retrace_as_ops_set(&retrace_as_ops_ptrace);
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
	retrace_as_ops_set(NULL);
}

static void test_deny_errno_clobber_falls_back_to_eperm(void)
{
	struct retrace_ptrace_frame f = mkframe();

	retrace_as_ops_set(&retrace_as_ops_ptrace);
	retrace_as_cancel_sched_real(tc, &f);

	tc->ret_errno = 0;
	errno = 0;
	retrace_as_set_ret_val(tc, &f, -1);
	CHECK(f.forced_retval == -EPERM);
	retrace_as_ops_set(NULL);
}

static void test_call_real_allows_and_defers(void)
{
	struct retrace_ptrace_frame f = mkframe();
	intptr_t sentinel;

	retrace_as_ops_set(&retrace_as_ops_ptrace);
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
	retrace_as_ops_set(NULL);
}

static void test_modify_without_call_real_forces_value(void)
{
	struct retrace_ptrace_frame f = mkframe();

	retrace_as_ops_set(&retrace_as_ops_ptrace);
	retrace_as_cancel_sched_real(tc, &f);

	retrace_as_set_ret_val(tc, &f, -5);
	CHECK(f.skip_real == 1);
	CHECK(f.forced_retval == -5);
	CHECK(f.exit_override == 0);
	retrace_as_ops_set(NULL);
}

static void test_sched_real_allows(void)
{
	struct retrace_ptrace_frame f = mkframe();

	retrace_as_ops_set(&retrace_as_ops_ptrace);
	retrace_as_cancel_sched_real(tc, &f);
	retrace_as_sched_real(tc, &f, NULL);
	CHECK(f.skip_real == 0);
	retrace_as_ops_set(NULL);
}

int
main(void)
{
	memset(&the_ctx, 0, sizeof(the_ctx));
	memset(&retrace_real_impls, 0, sizeof(retrace_real_impls));
	retrace_real_impls.malloc = fake_malloc;
	retrace_real_impls.free = fake_free;
	retrace_real_impls.memset = fake_memset;
	retrace_real_impls.memcpy = fake_memcpy;
	retrace_real_impls.strcmp = fake_strcmp;

	printf("as-ops seam (ADR-0016)\n");

	TEST(default_ops_when_unset);
	TEST(installed_ops_dispatch_and_clear);
	TEST(nested_depth_uses_default);
	TEST(null_ctx_uses_default);
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
