/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * The Windows hook table (TODO.windows/05-06). See hook_targets.h
 * for the layer/opt-in policy. This module owns: the table,
 * target resolution, hook installation, and the name->trampoline
 * map the arch-spec consults for real-impl resolution.
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "hook.h"
#include "hook_targets.h"
#include "trampoline_allocator.h"

#include "engine.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Wrapper name table: SAME ORDER as the wrapper entries in
 * wrapper_x64.S / wrapper_x64.asm. The assembly is label-free:
 * each wrapper passes its table INDEX to retrace_win_enter as a
 * pure immediate (assemblers disagree on label addressing; they
 * agree on immediates and extern calls).
 */
const char *const retrace_win_wrapper_names[] = {
	"fopen",		    /* 0 */
	"NtCreateFile",		    /* 1 */
	"NtOpenFile",		    /* 2 */
	"NtQueryAttributesFile",    /* 3 */
	"NtClose",		    /* 4 */
	"LdrLoadDll",		    /* 5 */
	"open",			    /* 6 */
	"close",		    /* 7 */
	"read",			    /* 8 */
	"write",		    /* 9 */
	"lseek",		    /* 10 */
	"stat",			    /* 11 */
	"unlink",		    /* 12 */
	"remove",		    /* 13 */
	"rename",		    /* 14 */
	"rmdir",		    /* 15 */
	"getenv",		    /* 16 */
	"connect",		    /* 17 */
	"send",			    /* 18 */
	"recv",			    /* 19 */
	"NtWriteFile",		    /* 20 */
	"NtReadFile",		    /* 21 */
	"NtQueryDirectoryFile",	    /* 22 */
};

/*
 * Boot-loop diagnostics (TODO.trace-profile/07): RETRACE_WIN_DIAG=1
 * prints every wrapper entry (name + repeat count) via WriteFile --
 * NO CRT on this path (interposition recursion). A trampoline that
 * re-enters its own patched target shows as one name repeating
 * forever; the count proves the loop.
 */
static void win_diag_entry(const char *name)
{
	static int enabled = -1;
	static char last[32];
	static unsigned long repeats;

	if (enabled < 0) {
		/* Win32 ONLY -- getenv is hooked (TODO.trace-profile/11);
		 * a CRT getenv here recurses through the wrapper
		 */
		char buf[8];

		enabled = GetEnvironmentVariableA(
			"RETRACE_WIN_DIAG", buf, sizeof(buf)) > 0 &&
			buf[0] == '1';
	}
	if (!enabled)
		return;

	if (strcmp(name, last) == 0) {
		repeats++;
		if (repeats % 64 != 0)
			return;
	} else {
		strcpy(last, name);
		repeats = 0;
	}
	{
		char buf[96];
		int len = snprintf(buf, sizeof(buf), "we: %s x%lu\n",
			name, repeats + 1);
		DWORD wrote = 0;
		HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);

		if (len > 0)
			WriteFile(h, buf, (DWORD)len, &wrote, NULL);
	}
}

void retrace_win_enter(int idx, void *frame)
{
	if (idx < 0 || (size_t)idx >=
	    sizeof(retrace_win_wrapper_names) /
		    sizeof(retrace_win_wrapper_names[0]))
		return;
	win_diag_entry(retrace_win_wrapper_names[idx]);
	retrace_engine_wrapper(
		(char *)retrace_win_wrapper_names[idx], frame);
}

struct win_hook {
	/*
	 * name: the EXPORT to patch. engine: the prototype/engine
	 * name (NULL = same as name) -- ucrt exports carry a leading
	 * underscore the POSIX-shaped prototype tables do not.
	 */
	const char *name;
	const char *engine;
	const char *module;   /* module to resolve the export from */
	void *wrapper;        /* assembly wrapper entry */
	int opt_in;           /* 1: only when RETRACE_WIN_NTDLL=1 */
};

/*
 * The wrappers exist for x64 and arm64, both dialects (gas +
 * MASM/armasm64 -- TODO.trace-profile/08). The table compiles
 * everywhere (the registry walk is arch-neutral); only the
 * hookable entries are gated.
 */
#if defined(_M_X64) || defined(__x86_64__) ||	\
	defined(_M_ARM64) || defined(__aarch64__)
#define HAVE_WRAPPERS 1
#else
#define HAVE_WRAPPERS 0
#endif

#if HAVE_WRAPPERS
static const struct win_hook g_hook_table[] = {
	/*
	 * ucrt layer (TODO.trace-profile/04): default-on. The
	 * underscore exports map to the POSIX-shaped prototypes the
	 * engine looks up (open/close/read/write/stat/...), so
	 * Windows profiles and jails cover normal C programs.
	 */
	{ "fopen", NULL, "ucrtbase.dll", retrace_wrap_fopen, 0 },
	{ "_open", "open", "ucrtbase.dll", retrace_wrap_open, 0 },
	{ "_close", "close", "ucrtbase.dll", retrace_wrap_close, 0 },
	{ "_read", "read", "ucrtbase.dll", retrace_wrap_read, 0 },
	{ "_write", "write", "ucrtbase.dll", retrace_wrap_write, 0 },
	{ "_lseek", "lseek", "ucrtbase.dll", retrace_wrap_lseek, 0 },
	{ "_stat", "stat", "ucrtbase.dll", retrace_wrap_stat, 0 },
	{ "_unlink", "unlink", "ucrtbase.dll",
		retrace_wrap_unlink, 0 },
	{ "_remove", "remove", "ucrtbase.dll",
		retrace_wrap_remove, 0 },
	{ "_rename", "rename", "ucrtbase.dll",
		retrace_wrap_rename, 0 },
	{ "_rmdir", "rmdir", "ucrtbase.dll", retrace_wrap_rmdir, 0 },

	/*
	 * Env visibility (TODO.trace-profile/11): the capture
	 * default config always listed getenv -- without the hook,
	 * Windows profiles reported env: [] silently.
	 */
	{ "getenv", NULL, "ucrtbase.dll", retrace_wrap_getenv, 0 },

	/*
	 * Net visibility (TODO.trace-profile/11): ws2_32 direct
	 * exports; profiles get connect/send/recv like POSIX.
	 */
	{ "connect", NULL, "ws2_32.dll", retrace_wrap_connect, 0 },
	{ "send", NULL, "ws2_32.dll", retrace_wrap_send, 0 },
	{ "recv", NULL, "ws2_32.dll", retrace_wrap_recv, 0 },

	/* ntdll layer (TODO.windows/06, TODO.trace-profile/13):
	 * opt-in only. Catches Win32-direct callers (CreateFileW
	 * funnels into NtCreateFile; GetFileAttributesW into
	 * NtQueryAttributesFile; WriteFile/ReadFile into
	 * NtWriteFile/NtReadFile; FindFirstFile into
	 * NtQueryDirectoryFile) that never touch the CRT.
	 */
	{ "NtCreateFile", NULL, "ntdll.dll",
		retrace_wrap_NtCreateFile, 1 },
	{ "NtOpenFile", NULL, "ntdll.dll",
		retrace_wrap_NtOpenFile, 1 },
	{ "NtQueryAttributesFile", NULL, "ntdll.dll",
		retrace_wrap_NtQueryAttributesFile, 1 },
	{ "NtClose", NULL, "ntdll.dll", retrace_wrap_NtClose, 1 },
	{ "LdrLoadDll", NULL, "ntdll.dll",
		retrace_wrap_LdrLoadDll, 1 },
	/*
	 * NtWriteFile/NtReadFile REMOVED (TODO.trace-profile/27
	 * round 4 bisect): hooking NtWriteFile alone crashes ANY
	 * target under injection -- the logger's own fprintf write
	 * re-enters the engine through the hook (unlisted functions
	 * get the default log_params script), recursing until the
	 * stack dies with an AV that cannot dispatch through any
	 * handler. Content-level read/write truth belongs to ETW /
	 * procmon; the path-truth hooks (create/open/query) carry
	 * the grading value. TODO 28 revisits with a guard-proof
	 * logger write path if syscall-level rw truth is ever
	 * needed.
	 */
	{ "NtQueryDirectoryFile", NULL, "ntdll.dll",
		retrace_wrap_NtQueryDirectoryFile, 1 },
};
#else
static const struct win_hook g_hook_table[] = {
	{ NULL, NULL, NULL, NULL, 0 }
};
#endif

#define G_HOOK_COUNT (sizeof(g_hook_table) / sizeof(g_hook_table[0]))

struct installed_hook {
	retrace_hook_t *handle;
	void *trampoline;
	const char *name;
};

static struct installed_hook g_installed[G_HOOK_COUNT];
static int g_installed_count;

/* last refusal reason, for tests/diagnostics (OutputDebugString
 * is invisible in CI logs)
 */
static char g_last_refusal[128] = "none";

const char *retrace_win_last_refusal(void)
{
	return g_last_refusal;
}

/*
 * Thunk-path installer: build trampoline = [prefix][jmp rel32
 * worker+0] and patch [E9 wrapper] at the thunk. The worker is
 * never patched -- its prologue runs fresh from worker+0.
 */
retrace_hook_status_t retrace_win_install_thunk(void *thunk_target,
	void *worker, void *wrapper, const unsigned char *prefix,
	size_t prefix_len, void **trampoline_out,
	retrace_hook_t **hook_out);

retrace_hook_status_t
retrace_win_install_thunk(void *thunk_target, void *worker,
	void *wrapper, const unsigned char *prefix,
	size_t prefix_len, void **trampoline_out,
	retrace_hook_t **hook_out)
{
	size_t tramp_size = prefix_len + 5; /* prefix + jmp rel32 */
#if defined(_M_ARM64) || defined(__aarch64__)
	/* [ldr x17, [pc,#8]][br x17][.quad worker] -- 16 bytes */
	tramp_size = prefix_len + 16;
#endif
	unsigned char *buf = (unsigned char *)
		retrace_trampoline_alloc_near(thunk_target, tramp_size);
	ptrdiff_t rel;
	retrace_hook_status_t st;
	unsigned char patch[16];
	size_t patch_len = 0;
	ptrdiff_t patch_rel;
	DWORD old_protect = 0;
	static const unsigned char ldr_x17_pc8[4] = { 0x51, 0x00,
						      0x00, 0x58 };
	static const unsigned char br_x17[4] = { 0x20, 0x02,
						 0x1F, 0xD6 };

	(void)worker;
	if (buf == NULL)
		return RETRACE_HOOK_NO_MEMORY;

	if (prefix_len > 0)
		memcpy(buf, prefix, prefix_len);
#if defined(_M_ARM64) || defined(__aarch64__)
	memcpy(buf + prefix_len + 0, ldr_x17_pc8, 4);
	memcpy(buf + prefix_len + 4, br_x17, 4);
	memcpy(buf + prefix_len + 8, &worker, 8);
#else
	rel = (const char *)worker -
	      ((const char *)buf + prefix_len + 5);
	if (rel > 0x7fffffffLL || rel < -0x80000000LL)
		return RETRACE_HOOK_INTERNAL;
	buf[prefix_len] = 0xE9;
	memcpy(buf + prefix_len + 1, &rel, 4);
#endif

	/*
	 * Thunk patch: always the absolute form. The thunk export's
	 * pad bytes (x86 CC/CC, arm64 NOP/zero) extend the writable
	 * window, and rel32 can't reach the wrapper (system DLL vs
	 * retrace.dll -- arbitrary 47-bit distance).
	 */
#if defined(_M_ARM64) || defined(__aarch64__)
	patch_len = 16;
	memcpy(patch + 0, ldr_x17_pc8, 4);
	memcpy(patch + 4, br_x17, 4);
	memcpy(patch + 8, &wrapper, 8);
#else
	patch_len = 14;
	patch[0] = 0x48;            /* REX.W */
	patch[1] = 0xB8;            /* mov rax, imm64 */
	memcpy(patch + 2, &wrapper, 8);
	patch[10] = 0xFF;           /* jmp r/m64 */
	patch[11] = 0xE0;           /* ModR/M: r/m = rax */
	/* pad with NOPs so a fall-through (there is none) stays
	 * executable
	 */
	patch[12] = 0x90;
	patch[13] = 0x90;
#endif

	if (!VirtualProtect(thunk_target, patch_len,
		PAGE_EXECUTE_READWRITE, &old_protect))
		return RETRACE_HOOK_INTERNAL;
	/* capture the original bytes BEFORE the patch: uninstall
	 * restores them (a live patch after uninstall loops through
	 * the wrapper forever -- the fallback real-impl resolution
	 * returns the patched export itself)
	 */
	st = retrace_hook_bookmark(thunk_target, patch_len, buf, hook_out);
	if (st != RETRACE_HOOK_OK) {
		VirtualProtect(thunk_target, patch_len, old_protect,
			&old_protect);
		return st;
	}
	memcpy(thunk_target, patch, patch_len);
	VirtualProtect(thunk_target, patch_len, old_protect, &old_protect);
	FlushInstructionCache(GetCurrentProcess(), thunk_target, patch_len);

	*trampoline_out = buf;
	return RETRACE_HOOK_OK;
}

/*
 * arm64 thunk follower (TODO.trace-profile/12): arm64 ucrt
 * exports are "b <worker>" branch stubs (the arm64 shape of the
 * x86 tail-jump thunk): [b imm26][nop][nop][pad]. Follow ONE
 * unconditional branch to the worker; a destination that is
 * itself padding/nop is refused (mirrors the x86 guards).
 */
#if defined(_M_ARM64) || defined(__aarch64__)
static void *follow_thunk_arm64(void *addr, size_t *prefix_len)
{
	const unsigned char *p = (const unsigned char *)addr;
	uint32_t insn;
	int64_t imm;
	const unsigned char *dest;
	uint32_t first;

	*prefix_len = 0;
	memcpy(&insn, p, sizeof(insn));
	/*
	 * movz/movn/movk prefix: the arm64 shape of the x86 thunk's
	 * "mov r8d, 0x40" variant-selector setup (ucrtbase fopen is
	 * [movz w2, #0x40][b <worker>]). Replayed in the trampoline.
	 */
	if ((insn & 0x7F800000U) == 0x52800000U ||
	    (insn & 0x7F800000U) == 0x12800000U ||
	    (insn & 0x7F800000U) == 0x72800000U) {
		*prefix_len = 4;
		p += 4;
		memcpy(&insn, p, sizeof(insn));
	}
	if ((insn & 0xFC000000U) != 0x14000000U) /* B imm26 */
		return addr;
	imm = (int64_t)(insn & 0x03FFFFFFU);
	if (imm & 0x02000000U)
		imm -= 0x04000000U;
	dest = p + ((ptrdiff_t)imm << 2);
	if (dest == addr)
		return addr;
	memcpy(&first, dest, sizeof(first));
	/* landing in zero padding or on a nop: not a worker */
	if (first == 0x00000000U || first == 0xD503201FU)
		return addr;
	return (void *)dest;
}
#endif

/* Win32 env read (TODO.trace-profile/28): install-time diag
 * gate -- CRT getenv may already flow through hooks installed
 * earlier in THIS loop.
 */
static int diag_enabled(void)
{
	char buf[8];

	return GetEnvironmentVariableA("RETRACE_WIN_DIAG", buf,
		sizeof(buf)) > 0 && buf[0] == '1';
}

static int ntdll_opt_in(void)
{
	const char *env = getenv("RETRACE_WIN_NTDLL");

	return env != NULL && env[0] == '1' && env[1] == '\0';
}

/*
 * Debug/bisect subset selector (TODO.trace-profile/27): when
 * RETRACE_WIN_NTDLL_LIST="NtCreateFile,NtClose" is set, only
 * the listed opt-in hooks install. Finds the killer hook in one
 * CI round. Plain Win32 env read -- getenv here runs BEFORE any
 * hook exists, and this TU's installer uses CRT freely.
 */
static int ntdll_listed(const char *name)
{
	char buf[256];
	DWORD n = GetEnvironmentVariableA("RETRACE_WIN_NTDLL_LIST",
		buf, sizeof(buf));
	const char *p;

	if (n == 0 || n >= sizeof(buf))
		return 1; /* unset/oversized: no filtering */
	p = buf;
	while (*p != '\0') {
		const char *e = p;
		size_t len;

		while (*e != '\0' && *e != ',')
			e++;
		len = (size_t)(e - p);
		if (strlen(name) == len && strncmp(p, name, len) == 0)
			return 1;
		p = (*e == ',') ? e + 1 : e;
	}
	return 0;
}

/*
 * CRT exports are thin thunks: ucrtbase!fopen is
 * "mov r8d, 0x40; jmp __common_fopen" -- the tail jump makes the
 * thunk itself unhookable (relocation-unsafe prologue), but the
 * jump target is the real worker every variant funnels into.
 * Follow (bounded) and hook THERE.
 */
static void *follow_thunk(void *addr, const unsigned char **prefix,
			  size_t *prefix_len)
{
	const unsigned char *p;
	int hop;

	p = (const unsigned char *)addr;
	*prefix = NULL;
	*prefix_len = 0;
	for (hop = 0; hop < 4; hop++) {
		size_t skip = 0;

		/* mov r32/r8d, imm32 (b8..bf / 41 b8..bf): skip it */
		if (p[0] == 0x41 && p[1] >= 0xB8 && p[1] <= 0xBF)
			skip = 6;
		else if (p[0] >= 0xB8 && p[0] <= 0xBF)
			skip = 5;

		/*
		 * only the FIRST hop may set up arguments; a chain
		 * of prefixed thunks is pathological -- stop there
		 */
		if (skip > 0) {
			if (hop == 0) {
				*prefix = p;
				*prefix_len = skip;
			} else {
				break;
			}
		}

		/* jmp rel32 */
		if (p[skip] == 0xE9) {
			const unsigned char *dest =
				p + skip + 5 + *(const int *)(p + skip + 1);

			if (dest == p)
				break; /* self-loop guard */
			/*
			 * A jump that lands within this stub's own
			 * neighborhood is NOT a worker: ucrtbase getenv
			 * on older images is "e9 07" -- 7 bytes ahead,
			 * into the export's CC padding. Following it
			 * built a trampoline INTO padding (the 2022-leg
			 * segfault). Same for a destination that starts
			 * with int3 padding. Refuse: the hook is skipped,
			 * the function runs uninstrumented.
			 */
			if (dest > p && dest < p + 32)
				break;
			if (dest[0] == 0xCC)
				break;
			p = dest;
			continue;
		}

		/* jmp [rip+disp32] -- read the indirect target */
		if (p[skip] == 0xFF && p[skip + 1] == 0x25) {
			const void *const *slot =
				(const void *const *)(p + skip + 6 +
					*(const int *)(p + skip + 2));

			if (*slot == NULL || *slot == (const void *)p)
				break;
			p = (const unsigned char *)*slot;
			continue;
		}
		break;
	}
	return (void *)p;
}

/*
 * The table accessor hook.h declares for the backends. Windows
 * resolves targets at install time (GetProcAddress), so the raw
 * table is internal; expose the count for the install driver.
 */
const struct retrace_hook_target *
retrace_win_hook_targets(size_t *count_out)
{
	if (count_out != NULL)
		*count_out = 0;
	return NULL;
}

int retrace_win_install_hooks(void)
{
	size_t i;
	int opt_in = ntdll_opt_in();

	g_installed_count = 0;
	for (i = 0; i < G_HOOK_COUNT; i++) {
		const struct win_hook *h = &g_hook_table[i];
		HMODULE mod;
		void *target;
		void *trampoline = NULL;
		retrace_hook_t *handle = NULL;
		retrace_hook_status_t st;
		char msg[128];

		if (h->opt_in && (!opt_in || !ntdll_listed(h->name)))
			continue;

		mod = GetModuleHandleA(h->module);
		if (mod == NULL) {
			snprintf(msg, sizeof(msg),
				"retrace: module '%s' not loaded, not hooking %s",
				h->module, h->name);
			snprintf(g_last_refusal, sizeof(g_last_refusal), "%s",
				msg);
			OutputDebugStringA(msg);
			continue;
		}
		target = (void *)GetProcAddress(mod, h->name);
		if (target == NULL) {
			/*
			 * Export tables vary per arch/image (arm64
			 * ucrtbase lacks several x86-decorated names) --
			 * a normal skip, not a refusal: do not overwrite
			 * the decoder-refusal diagnostic the tests read.
			 */
			snprintf(msg, sizeof(msg),
				"retrace: no export '%s' in %s",
				h->name, h->module);
			OutputDebugStringA(msg);
			continue;
		}

		st = retrace_hook_install(target, h->wrapper,
					  &trampoline, &handle);
#if defined(_M_ARM64) || defined(__aarch64__)
		if (st == RETRACE_HOOK_UNSAFE) {
			/*
			 * arm64 CRT exports are "b <worker>" branch
			 * stubs; follow one hop and hook the worker via
			 * the arm64 thunk trampoline shape.
			 */
			size_t prefix_len = 0;
			void *resolved = follow_thunk_arm64(target,
				&prefix_len);

			if (resolved != target) {
				st = retrace_win_install_thunk(target,
					resolved, h->wrapper,
					(const unsigned char *)target,
					prefix_len,
					&trampoline, &handle);
				if (st == RETRACE_HOOK_OK)
					goto installed;
			}
		}
#else
		if (st == RETRACE_HOOK_UNSAFE) {
			/*
			 * a thunk? follow the tail jump to the worker
			 * and hook the real implementation instead.
			 * The thunk's argument-setup prefix is replayed
			 * in the trampoline so calls through it still
			 * deliver the variant selector.
			 */
			const unsigned char *prefix = NULL;
			size_t prefix_len = 0;
			void *resolved = follow_thunk(target, &prefix,
						      &prefix_len);

			if (resolved != target) {
				/*
				 * Thunk path: the worker's bytes were never
				 * patched -- the trampoline must only replay
				 * the prefix and JMP to the worker's entry
				 * (its intact prologue executes fresh). The
				 * generic install_ex would also copy a
				 * prologue window of the worker into the
				 * trampoline, which is BOTH unnecessary AND
				 * unsafe (truncating a multi-byte instruction
				 * at a disasm boundary crashes inside the
				 * trampoline). Inline the thunk trampoline
				 * here.
				 */
				st = retrace_win_install_thunk(target,
					resolved, h->wrapper, prefix,
					prefix_len, &trampoline, &handle);
				if (st == RETRACE_HOOK_OK)
					goto installed;
			}
		}
#endif
		if (st != RETRACE_HOOK_OK) {
			/*
			 * conservative v1: log and skip. Include the
			 * first prologue bytes -- CI-visible ground
			 * truth for decoder gaps.
			 */
			const unsigned char *bytes = target;
			int bi;

			snprintf(msg, sizeof(msg),
				"retrace: refused to hook '%s' (status %d) bytes:",
				h->name, (int)st);
			for (bi = 0; bi < 16; bi++) {
				char hex[8];

				snprintf(hex, sizeof(hex), " %02x",
					bytes[bi]);
				strncat(msg, hex, sizeof(msg) -
					strlen(msg) - 1);
			}
			snprintf(g_last_refusal, sizeof(g_last_refusal), "%s",
				msg);
			OutputDebugStringA(msg);
			continue;
		}


installed:
		if (diag_enabled()) {
			const unsigned char *bytes = target;
			int bi;

			snprintf(msg, sizeof(msg),
				"retrace: hooked '%s' prologue_len=%lu bytes:",
				h->name,
				(unsigned long)
					retrace_hook_last_prologue_len());
			for (bi = 0; bi < 16; bi++) {
				char hex[8];

				snprintf(hex, sizeof(hex), " %02x",
					bytes[bi]);
				strncat(msg, hex, sizeof(msg) -
					strlen(msg) - 1);
			}
			OutputDebugStringA(msg);
		}
		g_installed[g_installed_count].handle = handle;
		g_installed[g_installed_count].trampoline = trampoline;
		g_installed[g_installed_count].name =
			h->engine != NULL ? h->engine : h->name;
		g_installed_count++;
	}
	return g_installed_count;
}

void retrace_win_uninstall_hooks(void)
{
	int i;

	for (i = 0; i < g_installed_count; i++) {
		if (g_installed[i].handle != NULL)
			retrace_hook_uninstall(g_installed[i].handle);
	}
	g_installed_count = 0;
}

void *retrace_win_trampoline_for(const char *func_name)
{
	int i;

	if (func_name == NULL)
		return NULL;
	for (i = 0; i < g_installed_count; i++) {
		if (strcmp(g_installed[i].name, func_name) == 0)
			return g_installed[i].trampoline;
	}
	return NULL;
}
