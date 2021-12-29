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

#include <stdlib.h>
#include <stdio.h>

#define TEST_LOOPS 1000.0

int main(void)
{
	void *p;
	int i;
	double failed_malloc = 0;
	double failed_realloc = 0;
	double failed_calloc = 0;

	for (i = 0; i < TEST_LOOPS; i++) {
		p = malloc(42);

		if (p)
			free(p);
		else
			failed_malloc++;
	}

	for (i = 0; i < TEST_LOOPS; i++) {
		p = calloc(1, 42);

		if (p)
			free(p);
		else
			failed_calloc++;
	}

	for (i = 0; i < TEST_LOOPS; i++) {
		p = realloc(NULL, 42);

		if (p)
			free(p);
		else
			failed_realloc++;
	}


	printf("Failed %.0f calls to malloc from %.0f (%.02f%%)\n", failed_malloc, TEST_LOOPS, failed_malloc / TEST_LOOPS * 100);
	printf("Failed %.0f calls to calloc from %.0f (%.02f%%)\n", failed_calloc, TEST_LOOPS, failed_calloc / TEST_LOOPS * 100);
	printf("Failed %.0f calls to realloc from %.0f (%.02f%%)\n", failed_realloc, TEST_LOOPS, failed_realloc / TEST_LOOPS * 100);

	return 0;
}
