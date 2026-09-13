/*
 * The coverage-bridge E2E's workload (TODO.impl/17): two
 * seeds, the SAME assertion death (the marker, one strlen as
 * the last logged call), but a DIFFERENT call history -- the
 * engine's call-hash separates what the stack signature
 * cannot.
 *
 * RETRACE_FUZZ_SEED's parity picks the history length. The
 * strings are VOLATILE: a literal strlen folds away at -O and
 * dispatches nothing.
 */
#include <stdlib.h>
#include <string.h>

static size_t vlen(const char *s)
{
	volatile const char *p = s;
	char buf[32];
	size_t i;

	for (i = 0; i < sizeof(buf) - 1 && p[i] != '\0'; i++)
		buf[i] = p[i];
	buf[i] = '\0';
	return strlen(buf);
}

int main(void)
{
	const char *s = getenv("RETRACE_FUZZ_SEED");
	unsigned long seed = s != NULL ? strtoul(s, NULL, 10) : 0;

	if (seed % 2 == 0) {
		if (vlen("even path") == 0)
			return 1;
	} else {
		if (vlen("odd path a") == 0)
			return 1;
		if (vlen("odd path b") == 0)
			return 1;
	}
	/* the shared death: the marker rides the logged param of
	 * the final call; exit(1) lets the destructor surface the
	 * call-hash
	 */
	if (vlen("FUZZASSERT-end-state") == 0)
		return 1;
	return 1;
}
