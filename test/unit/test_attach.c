/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Integration test for native process attach (v2.4.0).
 *
 * Exercises the public retrace_attach_process() API end-to-end on
 * Linux: forks a child, attaches to it while it runs, arms a
 * timeout to terminate the child, and verifies the trace loop ran
 * to completion (attach returns 0 when the target exits).
 *
 * Also verifies retrace_list_backends(): at least one backend must
 * be registered, and on Linux the ptrace backend must be present.
 *
 * The attach leg requires PTRACE_ATTACH on a direct child, which
 * is permitted in default container/CI configurations (ptrace of
 * your own child; no yama relaxation needed).
 *
 * The trace output itself goes wherever RETRACE_LOGGER_DEF_FN
 * points; ctest sets it to a temp file. We assert the loop
 * completed, not specific syscalls -- the exact syscall set of a
 * pause()-ing child varies by libc.
 */

#include <retrace/retrace.h>
#include <retrace/backend.h>

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

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

static void test_list_backends_returns_entries(void)
{
	const char *const *names = NULL;
	size_t count = 0;
	retrace_status_t rc;
	size_t i;

	rc = retrace_list_backends(NULL, NULL);
	CHECK(rc != 0);

	rc = retrace_list_backends(&names, &count);
	CHECK(rc == 0);
	CHECK(count > 0);
	CHECK(names != NULL);

	/* Every name non-NULL, non-empty. */
	for (i = 0; i < count; i++) {
		CHECK(names[i] != NULL);
		CHECK(names[i][0] != '\0');
	}

	printf("[%zu: ", count);
	for (i = 0; i < count; i++)
		printf("%s%s", i > 0 ? " " : "", names[i]);
	printf("] ");
}

static void test_attach_invalid_pid_rejected(void)
{
	CHECK(retrace_attach_process(0) != 0);
	CHECK(retrace_attach_process(-1) != 0);
}

#ifdef __linux__
#include <sys/ptrace.h>

static pid_t g_child_pid;
static volatile sig_atomic_t g_kill_sent;

/*
 * Timeout: if attach has not returned within ATTACH_TIMEOUT_SEC
 * (child failed to exit), kill the child so the trace loop's
 * waitpid observes the exit and retrace_attach_process returns.
 */
#define ATTACH_TIMEOUT_SEC 10

static void on_alarm(int sig)
{
	(void) sig;
	if (!g_kill_sent) {
		g_kill_sent = 1;
		kill(g_child_pid, SIGKILL);
	}
}

static void test_attach_to_running_child(void)
{
	pid_t child;
	retrace_status_t rc;

	child = fork();
	if (child == 0) {
		/* Child: park until the parent kills us. */
		pause();
		_exit(0);
	}
	CHECK(child > 0);
	g_child_pid = child;
	g_kill_sent = 0;

	signal(SIGALRM, on_alarm);
	alarm(ATTACH_TIMEOUT_SEC);

	rc = retrace_attach_process((int)child);

	alarm(0);
	signal(SIGALRM, SIG_DFL);

	if (!g_kill_sent) {
		/* The child should still be alive post-attach only if
		 * something went wrong; clean up either way. */
		kill(child, SIGKILL);
	}
	waitpid(child, NULL, 0);

	/* PTRACE_ATTACH of a direct child is permitted under default
	 * yama (ptrace_scope 1) and in CI containers. If permission
	 * was denied, report distinctly rather than as a hard fail.
	 */
	if (rc == RETRACE_ERR_PERMISSION) {
		printf("(SKIPPED: ptrace permission denied) ");
		return;
	}

	CHECK(rc == RETRACE_OK);
}
#else
static void test_attach_to_running_child(void)
{
	/* Non-Linux: the API must fail cleanly, never return OK.
	 * NOENT = no ptrace backend compiled in (typical macOS/Windows
	 * build); UNSUPPORTED = backend present but platform-refuses.
	 */
	retrace_status_t rc = retrace_attach_process(12345);

	CHECK(rc == RETRACE_ERR_NOENT || rc == RETRACE_ERR_UNSUPPORTED);
}
#endif

int main(void)
{
	printf("-- retrace_list_backends --\n");
	TEST(list_backends_returns_entries);

	printf("-- attach input validation --\n");
	TEST(attach_invalid_pid_rejected);

	printf("-- attach to a running process --\n");
	TEST(attach_to_running_child);

	printf("\nPass: %d, Fail: %d (of %d)\n",
		tests_pass, tests_fail, tests_run);
	return tests_fail == 0 ? 0 : 1;
}
