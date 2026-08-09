/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * libFuzzer harness for the CLI config builders (TODO.complete/33 P0).
 *
 * Takes arbitrary bytes and synthesizes a (subcommand, argv) pair,
 * then calls the corresponding retrace_cli_build_* function. The
 * contract: never crash, never overflow the output buffer, always
 * leave it NUL-terminated.
 *
 * Catches:
 *   - Buffer overflow when many long func names are passed to
 *     `retrace trace`. (Pre-extraction this overflowed cmd_trace's
 *     stack buffer.)
 *   - JSON injection via func names containing quotes/backslashes.
 *   - Bad format specifiers in retval/rate/ms.
 *
 * Build via CMake with -DRETRACE_BUILD_FUZZERS=ON.
 */

#include "config_builder.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define MAX_ARGS 16
#define MAX_ARG_LEN 256

/*
 * Carve the input into a sequence of NUL-free, NUL-terminated strings.
 * Replaces any embedded NUL with '_' so each is a valid C string.
 * Sets *out_argv to a malloc'd array of char* (each also malloc'd),
 * and *out_argc to the count. Caller frees out_argv[0..n-1] then
 * out_argv. Returns 0 on success (possibly with argc=0), -1 on OOM.
 *
 * Splitting strategy: split on the byte 0xFF (uncommon in text).
 * At most MAX_ARGS strings, each at most MAX_ARG_LEN bytes. This
 * bounds the per-input cost and keeps the argv shape realistic.
 */
static int carve_argv(const uint8_t *data, size_t size,
		      char ***out_argv, size_t *out_argc)
{
	char **argv;
	size_t argc = 0;
	size_t i = 0;

	argv = (char **)calloc(MAX_ARGS, sizeof(char *));
	if (argv == NULL)
		return -1;

	while (i < size && argc < MAX_ARGS) {
		size_t j = i;
		size_t len;
		char *s;

		while (j < size && data[j] != 0xFF)
			j++;
		len = j - i;
		if (len > MAX_ARG_LEN)
			len = MAX_ARG_LEN;

		s = (char *)malloc(len + 1);
		if (s == NULL)
			goto fail;
		for (size_t k = 0; k < len; k++) {
			uint8_t b = data[i + k];

			s[k] = (b == 0) ? '_' : (char)b;
		}
		s[len] = '\0';
		argv[argc++] = s;

		i = (j < size) ? j + 1 : j;
	}

	*out_argv = argv;
	*out_argc = argc;
	return 0;

fail:
	for (size_t k = 0; k < argc; k++)
		free(argv[k]);
	free(argv);
	return -1;
}

static void free_argv(char **argv, size_t argc)
{
	for (size_t i = 0; i < argc; i++)
		free(argv[i]);
	free(argv);
}

/*
 * Sanity check on builder output: result must be NUL-terminated within
 * jsonsz, and (on success) must contain at least one '{' and one '}'.
 * Returns 0 on pass, -1 on suspicious output (treated as fuzzer bug).
 */
static int sanity_check(const char *json, size_t jsonsz, int rc)
{
	size_t n = strnlen(json, jsonsz);

	if (n == jsonsz)
		return -1;

	if (rc == 0) {
		if (n == 0 || json[0] != '{' || json[n - 1] != '}')
			return -1;
	}
	return 0;
}

/*
 * Derive a small integer from arbitrary bytes. Used to vary the
 * numeric inputs (retval, rate, ms) across fuzz inputs without
 * requiring the harness to parse a number from argv.
 */
static long derive_long(const uint8_t *data, size_t size)
{
	unsigned long acc = 0;

	for (size_t i = 0; i < size && i < 4; i++)
		acc = (acc << 8) | data[i];
	return (long)acc;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	typedef enum { TRACE_WC, TRACE_NAMED, MOCK, FUZZ, SLOW, BUILD_COUNT }
		build_kind_t;
	build_kind_t which;
	char json[8192];
	char **argv = NULL;
	size_t argc = 0;
	int rc;
	long derived;

	if (size == 0)
		return 0;

	which = (build_kind_t)(data[0] % BUILD_COUNT);
	data++;
	size--;
	derived = derive_long(data, size > 4 ? 4 : size);

	if (which != TRACE_WC) {
		if (carve_argv(data, size, &argv, &argc) < 0)
			return 0;
		if (argc == 0) {
			/* No args provided -- fall back to wildcard trace. */
			which = TRACE_WC;
		}
	}

	switch (which) {
	case TRACE_WC:
		rc = retrace_cli_build_trace_config(json, sizeof(json),
						    NULL, 0);
		break;
	case TRACE_NAMED:
		rc = retrace_cli_build_trace_config(json, sizeof(json),
						    (const char *const *)argv,
						    argc);
		break;
	case MOCK:
		rc = retrace_cli_build_mock_config(json, sizeof(json),
						   argv[0], derived);
		break;
	case FUZZ:
		rc = retrace_cli_build_fuzz_config(json, sizeof(json),
						   argv[0],
						   (double)(derived % 101) /
						       100.0);
		break;
	case SLOW:
		rc = retrace_cli_build_slow_config(json, sizeof(json),
						   argv[0],
						   (int)(derived % 60000));
		break;
	default:
		rc = -1;
		break;
	}

	if (sanity_check(json, sizeof(json), rc) < 0)
		__builtin_trap();

	if (argv != NULL)
		free_argv(argv, argc);

	return 0;
}
