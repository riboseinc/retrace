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
 * Per-arch function table for Mach-O aarch64 (Apple Silicon).
 *
 * The common function list lives in funcs_symbols.def (DRY — shared
 * by all backends). This file defines RETRACE_WRAP to the AArch64
 * trampoline macro, includes the .def, then adds macOS-specific
 * entries.
 */

.include "arch_spec_top.S"

#define RETRACE_WRAP WRAPPER_ENTRY_AARCH64
#include "funcs_symbols.def"
#undef RETRACE_WRAP

/*
 * macOS-specific: dlsym/dlerror are safe to intercept on Darwin.
 *
 * On glibc, dlsym is called during retrace's own constructor via
 * retrace_as_get_real_safe, so intercepting it would cause infinite
 * recursion. On Darwin, the constructor uses a different resolution
 * path (dyld interpose section walk), so dlsym interception is safe.
 */
/*
 * macOS SDKs with _DARWIN_C_SOURCE remap fopen to
 * fopen$DARWIN_EXTSN in optimized builds (the project's CMake
 * adds _DARWIN_C_SOURCE to every Darwin target). Interpose the
 * variant symbol; the engine strips the suffix (engine.c,
 * strip_darwin_extsn) so configs, prototypes, and logs all
 * use the plain name.
 */
WRAPPER_ENTRY_AARCH64 fopen$DARWIN_EXTSN

WRAPPER_ENTRY_AARCH64 dlerror
WRAPPER_ENTRY_AARCH64 dlsym
