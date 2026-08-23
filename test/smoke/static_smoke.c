/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Static-CRT smoke target (TODO.trace-profile/27). Built with
 * the STATIC MSVC runtime (/MT): the EXE carries its own fopen
 * -- no ucrtbase import to hook. The file open only becomes
 * visible at the ntdll boundary (NtCreateFile/NtOpenFile),
 * which is the retrace ntdll layer's (RETRACE_WIN_NTDLL=1)
 * territory.
 *
 * The verdict goes to a FILE too (and distinct exit codes):
 * stdout can vanish under suspended-create injection, and the
 * CI smoke needs ground truth about whether main ran at all.
 */

#include <stdio.h>

int main(void)
{
	FILE *f;
	int opened;
	FILE *out = fopen("static-result.txt", "wb");

	/* marker 1: proves main() started at all (round-5 evidence:
	 * a silently-dying child needs pinning to a phase)
	 */
	if (out != NULL) {
		fprintf(out, "entry-mark\n");
		fflush(out);
	}

	f = fopen("C:\\Windows\\System32\\drivers\\etc\\hosts",
		"rb");
	opened = f != NULL;

	if (out != NULL) {
		fprintf(out, "hosts open: %d\n", opened);
		fclose(out);
	}
	printf("hosts open: %d\n", opened);
	if (f != NULL)
		fclose(f);
	return opened ? 0 : 3;
}
