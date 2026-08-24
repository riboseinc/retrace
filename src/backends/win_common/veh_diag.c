/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * VEH fault-site breadcrumbs (TODO.trace-profile/27). The
 * ntdll+injection crash (any target, 0xC0000005/0xC0000409)
 * kills the process before any wrapper breadcrumb fires; WER
 * events are absent on the runners and no debugger exists
 * there. A vectored exception handler observes EVERY exception
 * before WER -- and makes the crash name its own fault site:
 * exception code, faulting address, the containing module, and
 * the raw bytes there (hand-disassembler fuel).
 *
 * Pure Win32 + static buffers: NO CRT on this path (interposition
 * recursion). Observes only -- EXCEPTION_CONTINUE_SEARCH, the
 * crash proceeds unchanged. Gated by RETRACE_WIN_DIAG=1; zero
 * cost otherwise.
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "veh_diag.h"

static LONG g_veh_armed;

static void veh_write(const char *buf, size_t len)
{
	DWORD wrote = 0;
	HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);

	if (h != NULL && h != INVALID_HANDLE_VALUE)
		WriteFile(h, buf, (DWORD)len, &wrote, NULL);
}

/* Fixed-width hex (no leading-zero tricks: an out-of-bounds
 * p[-1] peek is exactly the class of bug this handler hunts).
 */
static char *veh_hex64(char *p, unsigned long long v)
{
	static const char digits[] = "0123456789abcdef";
	int i;

	for (i = 60; i >= 0; i -= 4)
		*p++ = digits[(v >> i) & 0xf];
	return p;
}

static LONG WINAPI veh_handler(PEXCEPTION_POINTERS ep)
{
	EXCEPTION_RECORD *er = ep->ExceptionRecord;
	char buf[320];
	char *p = buf;
	unsigned char *pc = (unsigned char *)er->ExceptionAddress;
	HMODULE mod = NULL;
	int i;

	/*
	 * Recursion latch: if THIS handler faults (writing, module
	 * lookup), the nested entry must do nothing.
	 */
	if (g_veh_armed)
		return EXCEPTION_CONTINUE_SEARCH;
	g_veh_armed = 1;

	/*
	 * DBG_PRINTEXCEPTION_C (OutputDebugString): the CRT's
	 * invalid-parameter handler logs its complaint (expression,
	 * file, line) via ODS immediately before __fastfail --
	 * which bypasses every handler. THIS is the confession we
	 * came for: print the payload. ExceptionInformation[0] =
	 * length in chars, [1] = pointer to the string.
	 */
	if (er->ExceptionCode == 0x40010006 &&
	    er->NumberParameters >= 2) {
		const char *msg = (const char *)
			er->ExceptionInformation[1];
		size_t len = er->ExceptionInformation[0];

		if (len > 200)
			len = 200;
		if (msg != NULL && !IsBadReadPtr(msg, len > 0 ? len : 1)) {
			memcpy(buf, "veh-ods: ", 9);
			memcpy(buf + 9, msg, len);
			buf[9 + len] = '\n';
			veh_write(buf, 10 + len);
		}
		g_veh_armed = 0;
		return EXCEPTION_CONTINUE_SEARCH;
	}

	p = buf;
	memcpy(p, "veh: code=", 10);
	p += 10;
	p = veh_hex64(p, er->ExceptionCode);
	memcpy(p, " addr=", 6);
	p += 6;
	p = veh_hex64(p, (unsigned long long)(size_t)er->ExceptionAddress);

	/* Module WITHOUT loading anything and WITHOUT a refcount
	 * change (no loader traffic on this path).
	 */
	if (GetModuleHandleExA(
		GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
		GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
		(LPCSTR)pc, &mod) && mod != NULL) {
		char name[MAX_PATH];
		DWORD n = GetModuleFileNameA(mod, name, sizeof(name));

		memcpy(p, " module=", 8);
		p += 8;
		if (n > 0 && n < sizeof(name)) {
			memcpy(p, name, n);
			p += n;
		} else {
			memcpy(p, "?", 1);
			p += 1;
		}
		/* offset within the module: post-mortem symbol fuel */
		memcpy(p, "+0x", 3);
		p += 3;
		p = veh_hex64(p,
			(unsigned long long)(size_t)pc -
			(unsigned long long)(size_t)mod);
	} else {
		memcpy(p, " module=?(unmapped)", 19);
		p += 19;
	}

	memcpy(p, " bytes=", 7);
	p += 7;
	/* raw bytes AT the fault address; probe first (gcc/MinGW
	 * has no __try -- and a probe avoids a nested fault)
	 */
	if (!IsBadReadPtr(pc, 8)) {
		for (i = 0; i < 8; i++) {
			static const char hex[] = "0123456789abcdef";

			*p++ = hex[pc[i] >> 4];
			*p++ = hex[pc[i] & 0xf];
		}
	} else {
		memcpy(p, "unreadable", 10);
		p += 10;
	}
	*p++ = '\n';

	veh_write(buf, (size_t)(p - buf));
	g_veh_armed = 0;
	return EXCEPTION_CONTINUE_SEARCH;
}

int retrace_win_veh_init(void)
{
	char buf[8];

	if (GetEnvironmentVariableA("RETRACE_WIN_DIAG", buf,
		sizeof(buf)) == 0 || buf[0] != '1')
		return 0;
	if (AddVectoredExceptionHandler(1, veh_handler) == NULL)
		return -1;
	return 1;
}
