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
 * Per-arch function table. The common function list lives in
 * funcs_symbols.def (DRY — shared by all backends). Each platform
 * .S file defines RETRACE_WRAP to the appropriate trampoline macro,
 * includes the .def, then adds platform-specific entries.
 */

.include "arch_spec_top.S"

#define RETRACE_WRAP WRAPPER_ENTRY_SYSTEM_V
#include "funcs_symbols.def"
#undef RETRACE_WRAP

/*
 * glibc-specific: __isoc99_* scanf variants.
 *
 * Modern gcc + glibc redirects scanf/fscanf/sscanf/v*scanf to
 * __isoc99_* via __REDIRECT. Binaries built with modern gcc call
 * __isoc99_scanf (not scanf) at the PLT. We must interpose both
 * names on glibc; musl and BSDs only need the plain names.
 */
WRAPPER_ENTRY_SYSTEM_V __isoc99_fscanf
WRAPPER_ENTRY_SYSTEM_V __isoc99_scanf
WRAPPER_ENTRY_SYSTEM_V __isoc99_sscanf
WRAPPER_ENTRY_SYSTEM_V __isoc99_vfscanf
WRAPPER_ENTRY_SYSTEM_V __isoc99_vscanf
WRAPPER_ENTRY_SYSTEM_V __isoc99_vsscanf
