/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * CLI config builders (TODO.complete/33 P0 fuzz target).
 *
 * Extracts the "argv -> JSON config" generation that was inlined in
 * cmd_trace / cmd_mock / cmd_fuzz / cmd_slow. Two reasons to extract:
 *
 *   1. MECE: cmd_* previously mixed config generation with fork/exec.
 *      Separating them gives one place to test the JSON shape and one
 *      place to test the launch path.
 *
 *   2. Fuzz target: libFuzzer needs a callable entry point. The CLI
 *      binary's main() can't be linked into a fuzzer harness, so the
 *      generation logic moves here where both can reach it.
 *
 * All builders are bounds-checked: the prior inlined code in cmd_trace
 * accumulated snprintf return values into `int pos` without clamping,
 * so a long-enough func list would overflow the fixed-size stack
 * buffer once pos exceeded sizeof(json). The new append() helper
 * treats truncation as failure.
 *
 * Each builder returns:
 *   0  on success (json is a valid JSON document, NUL-terminated)
 *   -1 on overflow  (json[0] is set to '\0')
 *   -1 on bad input (NULL func, empty funcs list, etc.)
 */

#ifndef RETRACE_CLI_CONFIG_BUILDER_H_
#define RETRACE_CLI_CONFIG_BUILDER_H_

#include <stddef.h>

/*
 * Build the JSON config for `retrace trace`.
 *   funcs: array of func names (e.g. {"malloc", "free"})
 *   nfuncs: number of entries. 0 means wildcard ("*").
 */
int retrace_cli_build_trace_config(char *json, size_t jsonsz,
				    const char *const *funcs, size_t nfuncs);

/*
 * Build the JSON config for `retrace mock <func> <retval>`.
 *   retval is the integer the function should return instead of its
 *   real return value.
 */
int retrace_cli_build_mock_config(char *json, size_t jsonsz,
				  const char *func, long retval);

/*
 * Build the JSON config for `retrace fuzz [<func>] --rate R`.
 *   rate is the failure probability in [0.0, 1.0].
 */
int retrace_cli_build_fuzz_config(char *json, size_t jsonsz,
				  const char *func, double rate);

/*
 * Build the JSON config for `retrace slow <func> --ms N`.
 *   ms is the per-call injected latency in milliseconds.
 */
int retrace_cli_build_slow_config(char *json, size_t jsonsz,
				  const char *func, int ms);

#endif /* RETRACE_CLI_CONFIG_BUILDER_H_ */
