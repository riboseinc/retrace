/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Windows wrapper runtime tests (TODO.windows/05). x64 legs only
 * (the arm64 WrapperWinArm64Frame variant is the follow-up
 * slice).
 *
 * Proves the whole chain IN PROCESS: inline hook install ->
 * assembly wrapper -> WrapperWinX64Frame -> engine ->
 * config-driven actions -> trampoline back to the real fopen.
 * This is the same machinery the injected DLL runs, exercised
 * without CreateRemoteThread.
 *
 *   1. env: config with fopen -> [log_params, call_real], JSON
 *      log to a temp file
 *   2. install hooks (dllmain order: hooks BEFORE boot so
 *      real-impl resolution finds the trampoline)
 *   3. boot the engine
 *   4. fopen a temp file: the call must round-trip through the
 *      engine and STILL WORK (file created), and the log must
 *      contain the entry
 *   5. uninstall: fopen works again, plain
 *   6. refuse-to-hook: an E9 (rel32 jmp) prologue is unsafe
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "posix_compat.h"

#include "parson.h"

#include "data_types.h"
#include "engine.h"
#include "funcs.h"
#include "logger.h"
#include "real_impls.h"
#include "hook.h"
#include "hook_targets.h"

#include "disasm.h"

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

/* Always-on check: assert() compiles to nothing under NDEBUG. */
#define CHECK(cond) do { \
	if (!(cond)) { \
		printf("FAIL [%s:%d] %s\n", __FILE__, __LINE__, #cond); \
		tests_fail++; \
		return; \
	} \
} while (0)

static char g_log_path[MAX_PATH];
static char g_cfg_path[MAX_PATH];
static char g_deny_path[MAX_PATH];

static void write_text(const char *path, const char *text)
{
	FILE *f = fopen(path, "wb");

	CHECK(f != NULL);
	fwrite(text, 1, strlen(text), f);
	fclose(f);
}

/*
 * Read a file with Win32 (no CRT fopen -- keep the trace clean).
 * FULL sharing: the logger never closes its append handle (JSON
 * framing -- deinit only flushes), so a FILE_SHARE_READ-only
 * reader dies with ERROR_SHARING_VIOLATION against that write
 * handle and a healthy log READS as empty (the TODO 07 mystery
 * was exactly this measurement artifact, not env visibility).
 */
static size_t read_all(const char *path, char *buf, size_t bufsz)
{
	HANDLE h = CreateFileA(path, GENERIC_READ,
		FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
		OPEN_EXISTING, 0, NULL);
	DWORD got = 0;

	if (h == INVALID_HANDLE_VALUE)
		return 0;
	if (!ReadFile(h, buf, (DWORD)bufsz - 1, &got, NULL))
		got = 0;
	CloseHandle(h);
	buf[got] = '\0';
	return (size_t)got;
}

/*
 * Log-state probe (TODO.trace-profile/23, round 3): open the log
 * FULLY shared (the logger never closes its append handle --
 * deinit only flushes). A FILE_SHARE_READ-only open cannot
 * coexist with that write handle: ERROR_SHARING_VIOLATION would
 * make a healthy log READ as empty. Size + first bytes at each
 * stage pinpoints exactly when content lands.
 */
static void probe_log_state(const char *tag)
{
	HANDLE h = CreateFileA(g_log_path, GENERIC_READ,
		FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
		OPEN_EXISTING, 0, NULL);

	if (h == INVALID_HANDLE_VALUE) {
		printf("log-state %s: open failed err=%lu\n", tag,
			(unsigned long)GetLastError());
		return;
	}
	{
		LARGE_INTEGER sz;

		if (GetFileSizeEx(h, &sz) && sz.QuadPart > 0) {
			char head[64];
			DWORD got = 0;

			if (ReadFile(h, head, sizeof(head) - 1, &got,
				    NULL) &&
			    got > 0) {
				head[got] = '\0';
				printf("log-state %s: size=%lld head="
					"%.40s\n", tag,
					(long long)sz.QuadPart, head);
			} else {
				printf("log-state %s: size=%lld "
					"head unreadable\n", tag,
					(long long)sz.QuadPart);
			}
		} else if (GetFileSizeEx(h, &sz)) {
			printf("log-state %s: size=0\n", tag);
		} else {
			printf("log-state %s: size failed err=%lu\n",
				tag, (unsigned long)GetLastError());
		}
	}
	CloseHandle(h);
}

static void setenv_str(const char *name, const char *value)
{
	CHECK(_putenv_s(name, value) == 0);
}

/*
 * The full round trip. Runs as ONE test because the hook is
 * process-global state (install -> use -> uninstall).
 */
static void test_fopen_round_trip(void)
{
	FILE *f;
	char log[8192];
	size_t n;

	/*
	 * 1. config: fopen -> sandbox(allow: cfg + log) -> log_params
	 * -> call_real. The allowlist jail (TODO.trace-profile/12):
	 * an UNDECLARED path must be denied before libc executes.
	 */
	{
		FILE *cf = fopen(g_cfg_path, "wb");
		char esc_cfg[MAX_PATH * 2];
		char esc_log[MAX_PATH * 2];
		char *o;
		const char *i;

		CHECK(cf != NULL);
		/* Windows paths carry backslashes: raw in a JSON
		 * string they are invalid escapes -- conf_init would
		 * reject the whole config and the engine would never
		 * boot (the v2.17.0 CI lesson)
		 */
		for (o = esc_cfg, i = g_cfg_path; *i != '\0'; i++) {
			if (*i == '\\')
				*o++ = '\\';
			*o++ = *i;
		}
		*o = '\0';
		for (o = esc_log, i = g_log_path; *i != '\0'; i++) {
			if (*i == '\\')
				*o++ = '\\';
			*o++ = *i;
		}
		*o = '\0';

		fputs("{\"intercept_scripts\":[{\"func_name\":\"fopen\",", cf);
		fputs("\"actions\":[{\"action_name\":\"sandbox\",", cf);
		fputs("\"action_params\":{\"allow_paths\":[", cf);
		fprintf(cf, "\"%s\",\"%s\"]}},", esc_cfg, esc_log);
		fputs("{\"action_name\":\"log_params\"},", cf);
		fputs("{\"action_name\":\"call_real\"}]}]}", cf);
		fputs("\n", cf);
		fclose(cf);
	}
	write_text(g_deny_path, "undocumented");
	DeleteFileA(g_log_path);

	setenv_str("RETRACE_JSON_CONFIG", g_cfg_path);
	setenv_str("RETRACE_LOGGER_DEF_ENA", "1");
	setenv_str("RETRACE_LOGGER_DEF_STDOUT_ENA", "0");
	setenv_str("RETRACE_LOGGER_DEF_FN", g_log_path);

	printf("stage: env + config written\n");

	/* 2. hooks BEFORE boot (dllmain order) */
	if (retrace_win_install_hooks() < 1 ||
	    retrace_win_trampoline_for("fopen") == NULL) {
		printf("FAIL: fopen hook not installed, reason: %s\n",
			retrace_win_last_refusal());
		tests_fail++;
		return;
	}

	printf("stage: hooks installed\n");

	/*
	 * 3. boot (the full sequence; the stepwise diagnostic that
	 * cracked the NULL atoi/fseek members is no longer needed)
	 */
	retrace_core_boot();
	printf("stage: engine booted\n");

	/* 4. the hooked call must round-trip and still work */
	printf("stage: calling hooked fopen\n");
	f = fopen(g_cfg_path, "rb");
	CHECK(f != NULL);
	fclose(f);

	f = fopen(g_log_path, "rb");
	CHECK(f != NULL);
	fclose(f);

	/*
	 * Jail denial (TODO.trace-profile/12): g_deny_path is NOT in
	 * allow_paths -- sandbox aborts the script and synthesizes
	 * -1 (errno EACCES). The caller must NOT get a usable FILE*.
	 */
	{
		FILE *denied = fopen(g_deny_path, "rb");

		CHECK(denied == NULL || denied == (void *)-1);
	}

	/* give the ring flusher (non-MSVC) a beat */
	Sleep(300);
	probe_log_state("post-calls");

	/* 5. uninstall, then verify the log captured the calls */
	retrace_win_uninstall_hooks();
	CHECK(retrace_win_trampoline_for("fopen") == NULL);

	/*
	 * Deterministic flush: entries are fflush'd per write and
	 * the closing bracket lands here. NOTE: deinit does NOT
	 * close the append handle (by design -- JSON framing), so
	 * read_all must tolerate the concurrent write handle.
	 */
	retrace_logger_deinit();
	probe_log_state("post-deinit");

	n = read_all(g_log_path, log, sizeof(log));
	if (n == 0)
		printf("FAIL detail: log unreadable despite probe "
			"above\n");
	CHECK(n > 0);
	CHECK(strstr(log, "fopen") != NULL);

	/* plain fopen after uninstall */
	f = fopen(g_cfg_path, "rb");
	CHECK(f != NULL);
	fclose(f);
}

static void test_disasm_refuses_relative_jump(void)
{
	static const unsigned char code[] = {
		0xe9, 0x10, 0x00, 0x00, 0x00, /* jmp +16 -- unsafe */
		0x90, 0x90, 0x90, 0x90
	};

	CHECK(retrace_disasm_x64_prologue_len(code, 5, 16) == 0);
}

static void test_disasm_accepts_push_mov(void)
{
	/* push rbp; mov rbp, rsp; push rsi -- classic prologue */
	static const unsigned char code[] = {
		0x55,                         /* push rbp */
		0x48, 0x89, 0xe5,             /* mov rbp, rsp */
		0x56                          /* push rsi */
	};

	CHECK(retrace_disasm_x64_prologue_len(code, 5, 16) == 5);
}

static void test_install_refuses_unsafe_prologue(void)
{
	unsigned char *page;
	retrace_hook_t *hook = NULL;
	void *trampoline = NULL;
	retrace_hook_status_t st;
	static void (*nop_target)(void);

	/* craft an executable page starting with a rel32 jmp */
	page = (unsigned char *)VirtualAlloc(NULL, 64,
		MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
	CHECK(page != NULL);
	page[0] = 0xe9;
	*(int *)(page + 1) = 0x10;
	memset(page + 5, 0x90, 16);

	st = retrace_hook_install(page, (void *)&nop_target,
				  &trampoline, &hook);
	CHECK(st == RETRACE_HOOK_UNSAFE);
	CHECK(hook == NULL);

	VirtualFree(page, 0, MEM_RELEASE);
}

int main(void)
{
	char tmp_dir[MAX_PATH];

	setvbuf(stdout, NULL, _IONBF, 0);
	if (GetTempPathA(MAX_PATH, tmp_dir) == 0) {
		printf("FAIL: GetTempPathA\n");
		return 1;
	}
	printf("stage: paths resolved\n");
	snprintf(g_cfg_path, sizeof(g_cfg_path), "%sretrace_win_test.json",
		tmp_dir);
	snprintf(g_log_path, sizeof(g_log_path), "%sretrace_win_log.json",
		tmp_dir);
	snprintf(g_deny_path, sizeof(g_deny_path), "%sretrace_win_deny.txt",
		tmp_dir);

	/* minimal real_impls for the registry inits inside boot */
	retrace_real_impls.strcmp = strcmp;
	retrace_real_impls.strlen = strlen;
	retrace_real_impls.strcpy = strcpy;
	retrace_real_impls.memset = memset;
	retrace_real_impls.memcpy = memcpy;
	retrace_real_impls.malloc = malloc;
	retrace_real_impls.free = free;
	retrace_real_impls.real_snprintf = snprintf;

	printf("Windows wrapper runtime tests (x64):\n");
	TEST(fopen_round_trip);

	printf("refuse-to-hook:\n");
	TEST(disasm_refuses_relative_jump);
	TEST(disasm_accepts_push_mov);
	TEST(install_refuses_unsafe_prologue);

	printf("\nPass: %d, Fail: %d (of %d)\n",
		tests_pass, tests_fail, tests_run);
	return tests_fail == 0 ? 0 : 1;
}
