/*
 * sbrk test. NOT in runtests.sh.in -- intentionally.
 *
 * sbrk() grows/shrinks the data segment. Under LD_PRELOAD the
 * trampoline fires for every malloc/calloc/free inside libc's brk
 * path. libc's malloc may have allocated small objects from the
 * brk-managed region; when sbrk + brk shrink the segment back, those
 * allocations are no longer backed by valid memory. libc's heap
 * validator then emits "free(): invalid pointer" on the next free.
 *
 * This is a fundamental sbrk + LD_PRELOAD interaction, not a retrace
 * bug. The test exits 0 (sbrk + brk round-trip succeeds at the
 * syscall level) but glibc's diagnostic is unavoidable. Build it
 * for completeness; do not run it as part of CI. See issue #400.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <unistd.h>

static void test_sbrk(void)
{
#ifndef __APPLE__
	void *p, *request;

	p = sbrk(0);
	request = sbrk(1024);

	if (request == (void *) -1)
		return;

	brk(p);
#endif
}

int main(void)
{
	int i;

	for (i = 0; i < 1000; i++)
		test_sbrk();
}
