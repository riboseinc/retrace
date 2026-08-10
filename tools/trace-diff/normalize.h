/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

#ifndef RETRACE_DIFF_NORMALIZE_H_
#define RETRACE_DIFF_NORMALIZE_H_

#include "parson.h"

#include <stddef.h>
#include <stdint.h>

/*
 * Log normalization for differential testing (TODO.complete/27 MVP).
 *
 * A normalized log is an array of per-function summaries keyed by
 * function name. Each summary counts calls and accumulates total
 * time. Non-deterministic fields (timestamps, pointers, PIDs) are
 * NOT included in the summary -- only call count + total duration
 * are compared.
 *
 * The MVP scope:
 *   - Group entries by message.func
 *   - Count calls per function
 *   - Sum call_duration_us per function
 *
 * Out-of-scope for this PR (lands in follow-ups):
 *   - Argument shape comparison
 *   - Sequence alignment
 *   - Statistical significance testing
 */

struct FuncStat {
	char name[64];

	uint64_t call_count;

	uint64_t total_duration_us;
};

struct NormalizedLog {
	struct FuncStat *funcs;

	size_t count;

	size_t cap;
};

/*
 * Build a NormalizedLog from a parsed retrace trace (a JSON_Array
 * of entry objects). Returns 0 on success, -1 on failure.
 *
 * The trace is the JSON_Array* extracted from the root JSON_Value
 * via json_value_get_array(); pass NULL or a non-array to get -1.
 */
int normalize_from_trace(JSON_Array *trace, struct NormalizedLog *out);

/*
 * Build a flat sequence of function names from a trace, in the
 * order they were called. Used for ordering diff (TODO.complete/27 P1).
 *
 * Engine-noise entries (those with `message.text` instead of
 * `message.func`) are skipped -- they don't represent real
 * intercepted calls.
 *
 * Returns 0 on success, -1 on OOM. The caller owns *out_seq and
 * must free() it.
 */
int normalize_call_sequence(JSON_Array *trace, char ***out_seq,
			    size_t *out_len);

/*
 * Lookup a function's stats by name. Returns NULL if not present.
 */
const struct FuncStat *normalize_find(const struct NormalizedLog *log,
				      const char *name);

/*
 * Free a NormalizedLog.
 */
void normalize_free(struct NormalizedLog *log);

#endif /* RETRACE_DIFF_NORMALIZE_H_ */
