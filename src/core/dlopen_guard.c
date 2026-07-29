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
 * dlopen reentrance guard (issue #450).
 *
 * When a program calls dlopen(), libc's internal init path makes many
 * malloc/calloc/free calls. Our wrappers fire for each, the engine
 * processes them, and at some point the internal state inconsistency
 * between libc and retrace causes a segfault.
 *
 * The fix: intercept dlopen/dlclose/dlsym/dlerror with thin C shims
 * that set a thread-local flag before calling real, and clear it after.
 * The engine checks the flag at entry; if set, it skips all action
 * processing and lets the asm trampoline tail-call real (Path A).
 *
 * These are PLAIN C FUNCTIONS, not asm-trampoline wrappers. They
 * deliberately avoid the retrace engine for the dl* calls themselves
 * (those are intentionally NOT in funcs_symbols.S).
 *
 * On ELF (Linux/BSD): defining dlopen as a global symbol in our
 * LD_PRELOAD library is sufficient. The dynamic linker resolves all
 * references to dlopen through our symbol.
 *
 * On Mach-O (macOS): we emit __DATA,__interpose entries so dyld
 * rebinds other images' references to our shim.
 *
 * retrace_real_impls.dlopen/dlsym/etc. are set during init via
 * dlsym(RTLD_NEXT, ...). They point to real libc, so our shims call
 * them directly without recursing.
 */

#include <dlfcn.h>

#include "real_impls.h"

static __thread int g_retrace_in_dlopen;

int retrace_dlopen_guard_active(void)
{
	return g_retrace_in_dlopen > 0;
}

/*
 * dlopen shim. Sets the thread-local flag, calls real dlopen via the
 * pre-resolved function pointer, clears the flag, returns the handle.
 *
 * Variadic on macOS (__dlopen_mode) but on all other platforms the
 * signature is (const char *, int).
 */
static void *retrace_dlopen_shim(const char *filename, int flags)
{
	void *ret;

	g_retrace_in_dlopen++;
	ret = retrace_real_impls.dlopen(filename, flags);
	g_retrace_in_dlopen--;
	return ret;
}

static int retrace_dlclose_shim(void *handle)
{
	int ret;

	g_retrace_in_dlopen++;
	ret = retrace_real_impls.dlclose ?
		retrace_real_impls.dlclose(handle) : dlclose(handle);
	g_retrace_in_dlopen--;
	return ret;
}

static void *retrace_dlsym_shim(void *handle, const char *symbol)
{
	void *ret;

	g_retrace_in_dlopen++;
	ret = retrace_real_impls.dlsym ?
		retrace_real_impls.dlsym(handle, symbol) :
		dlsym(handle, symbol);
	g_retrace_in_dlopen--;
	return ret;
}

static char *retrace_dlerror_shim(void)
{
	char *ret;

	g_retrace_in_dlopen++;
	ret = dlerror();
	g_retrace_in_dlopen--;
	return ret;
}

/*
 * Export the shims as the public symbols so LD_PRELOAD / dyld
 * interpose them. On ELF this is automatic: the linker resolves all
 * references to dlopen/dlclose/dlsym/dlerror through our library.
 *
 * NOTE: macOS interpose is disabled for now. The __DATA,__interpose
 * section causes all tests to segfault during dyld init. The dlopen
 * crash only affects Linux/glibc (issue #450), so macOS doesn't need
 * the shim. Linux's LD_PRELOAD symbol interposition is sufficient.
 */
#ifndef __APPLE__
/*
 * On ELF, LD_PRELOAD does the interposition. Export dlopen/dlclose
 * with default visibility so the dynamic linker can find them.
 *
 * We do NOT interpose dlsym/dlerror because dlsym is called during
 * our own constructor (retrace_as_get_real_safe uses dlsym(RTLD_NEXT)
 * before real_impls is set up). Interposing dlsym would cause infinite
 * recursion: shim -> real_impls.dlsym (NULL) -> fallback dlsym() ->
 * shim -> ...
 *
 * dlopen/dlclose are sufficient: dlopen's internal path triggers the
 * crash (malloc calls during library loading); dlclose may too. dlsym
 * and dlerror don't cause reentrance issues.
 */
__attribute__((weak, visibility("default")))
void *dlopen(const char *filename, int flags)
{
	return retrace_dlopen_shim(filename, flags);
}

__attribute__((weak, visibility("default")))
int dlclose(void *handle)
{
	return retrace_dlclose_shim(handle);
}
#endif
