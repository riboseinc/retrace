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
 * Trampoline allocator for Windows. Allocates RWX memory within +/-2GB
 * of a target address so x64 rel32 jumps can reach the trampoline.
 *
 * Strategy: walk candidate addresses at 64 KiB (Windows allocation
 * granularity) steps above and below the target, calling VirtualAlloc
 * with MEM_RESERVE | MEM_COMMIT. VirtualAlloc returns NULL if the
 * candidate region is already reserved; we move on.
 */

#include "trampoline_allocator.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/* +/-2 GiB, minus a slack for the trampoline body itself. */
#define RETRACE_NEAR_WINDOW ((SIZE_T)(1u << 31) - (1u << 20))

/* Round x up to the next multiple of `align` (align must be a power of 2). */
static SIZE_T
align_up(SIZE_T x, SIZE_T align)
{
	return (x + align - 1) & ~(align - 1);
}

void *
retrace_trampoline_alloc_near(const void *target, size_t size)
{
	const ULONG_PTR target_addr = (const ULONG_PTR)target;
	const SIZE_T alloc_size = align_up(size, 0x1000); /* page-granular */
	const SIZE_T step = 0x10000; /* 64 KiB = Windows allocation granularity */
	SIZE_T offset;
	ULONG_PTR base;
	void *p;

	if (target == NULL || size == 0)
		return NULL;

	/* Try addresses above the target first. */
	for (offset = step; offset < RETRACE_NEAR_WINDOW; offset += step) {
		if (target_addr + offset < target_addr)
			break; /* overflow */
		base = align_up(target_addr + offset, step);
		p = VirtualAlloc((LPVOID)base, alloc_size,
				 MEM_RESERVE | MEM_COMMIT,
				 PAGE_EXECUTE_READWRITE);
		if (p != NULL)
			return p;
	}

	/* Then below. */
	for (offset = step; offset < RETRACE_NEAR_WINDOW; offset += step) {
		if (target_addr < offset)
			break;
		base = align_up(target_addr - offset, step);
		p = VirtualAlloc((LPVOID)base, alloc_size,
				 MEM_RESERVE | MEM_COMMIT,
				 PAGE_EXECUTE_READWRITE);
		if (p != NULL)
			return p;
	}

	return NULL;
}

void
retrace_trampoline_free(void *ptr)
{
	if (ptr != NULL)
		VirtualFree(ptr, 0, MEM_RELEASE);
}
