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

.include "arch_spec_top.S"

/*
 * The common function list lives in funcs_symbols.def (DRY --
 * shared by all backends; the x86_64 fallback and the Mach-O
 * aarch64 table both include it). This file was a flat
 * hand-maintained list that drifted: it lacked the entire
 * socket family plus open/openat -- 28 functions -- while every
 * other backend had them (found via the Wave C jail test: a
 * config on "open" matched nothing on linux-arm64 CI). It now
 * includes the .def and adds the glibc-aarch64 extras the flat
 * list carried.
 */

#define RETRACE_WRAP WRAPPER_ENTRY_AARCH64
#include "funcs_symbols.def"
#undef RETRACE_WRAP

/*
 * dlerror: safe to intercept on this path (the constructor
 * resolves reals via the trampoline table, not dlsym loops).
 */
WRAPPER_ENTRY_AARCH64 dlerror

/* pthread cleanup handlers (glibc-aarch64 extras). */
WRAPPER_ENTRY_AARCH64 pthread_cleanup_push
WRAPPER_ENTRY_AARCH64 pthread_cleanup_pop
