/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * fuzz-workbench target (TODO.trace-profile/20): the classic
 * unchecked-malloc bug. Under memory_fuzz some seeds fail the
 * allocation -> NULL deref -> SIGSEGV; the workbench clusters
 * the crashes (all in the same function) and hands back a
 * reproducer config.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void process(void)
{
	/* deliberately unchecked: the bug under fuzz */
	char *p = (char *)malloc(64);

	strcpy(p, "payload");
	free(p);
}

int main(void)
{
	int i;

	for (i = 0; i < 8; i++)
		process();
	printf("survived\n");
	return 0;
}
