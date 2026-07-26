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
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Trampoline allocator. Allocates PAGE_EXECUTE_READWRITE pages near the
 * target function (within +/-2GB on x64 so rel32 jumps can reach them).
 *
 * On arm64, proximity is desirable (for adrp/add-based trampolines) but
 * not strictly required because the arm64 trampoline uses an absolute
 * 64-bit address load.
 */

#ifndef RETRACE_BACKENDS_WIN_COMMON_TRAMPOLINE_ALLOCATOR_H
#define RETRACE_BACKENDS_WIN_COMMON_TRAMPOLINE_ALLOCATOR_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Allocate `size` bytes of executable memory within +/-2GB of `target`.
 * Returns NULL on failure. The returned pointer is RWX and 16-byte aligned.
 */
void *retrace_trampoline_alloc_near(const void *target, size_t size);

/* Free memory previously returned by retrace_trampoline_alloc_near. */
void retrace_trampoline_free(void *ptr);

#ifdef __cplusplus
}
#endif

#endif /* RETRACE_BACKENDS_WIN_COMMON_TRAMPOLINE_ALLOCATOR_H */
