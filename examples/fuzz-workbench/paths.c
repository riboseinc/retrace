/*
 * paths.c: the fuzz_str demo target (TODO.trace-profile/25).
 * Opens argv[1] and reports OPEN/FAIL -- the dictionary fuzzer
 * replaces the path before libc executes, so the printed path
 * shows which dict token was spliced in. Deterministic per
 * fuzz_seed: the same seed prints the same token sequence.
 */

#include <stdio.h>

int main(int argc, char **argv)
{
	FILE *f;

	if (argc < 2)
		return 2;
	f = fopen(argv[1], "rb");
	printf("%s: %s\n", argv[1], f != NULL ? "OPEN" : "FAIL");
	if (f != NULL)
		fclose(f);
	return 0;
}
