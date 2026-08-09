/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * retrace-diff: differential trace analysis (TODO.complete/27 MVP).
 *
 * Reads two retrace JSON logs, normalizes them into per-function
 * call-count + total-time summaries, and prints a structured diff.
 *
 * Usage:
 *   retrace run --log /tmp/before.json -- ./your-binary
 *   # ... make a change ...
 *   retrace run --log /tmp/after.json  -- ./your-binary
 *   retrace-diff /tmp/before.json /tmp/after.json
 *
 * Output (text format):
 *   function: malloc
 *     count:      before=1247, after=1532  (+285, +22.8%)
 *     total time: before=8.3ms, after=11.9ms (+3.6ms, +43.4%)
 *
 *   function: open
 *     absent in before; 3 calls in after
 *
 * Exit codes (for CI gating):
 *   0  no diff found
 *   1  diff found (some function changed)
 *   2  usage / IO error
 *
 * Variations land in follow-up PRs:
 *   - Argument shape comparison
 *   - HTML output
 *   - Sequence alignment
 *   - Threshold mode (--threshold pct=N)
 */

#include "parson.h"
#include "normalize.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(FILE *out)
{
	fprintf(out,
"retrace-diff -- differential trace analysis (TODO.complete/27 MVP)\n"
"\n"
"Usage:\n"
"  retrace-diff BEFORE.json AFTER.json\n"
"\n"
"Reads two retrace trace logs (each a JSON array of entries with\n"
"`message.func` and `message.call_duration_us`), groups calls by\n"
"function, and prints a structured diff of call counts and total\n"
"time.\n"
"\n"
"Exit codes:\n"
"  0  no diff (call surface unchanged)\n"
"  1  diff found (CI gating signal)\n"
"  2  argument or IO error\n"
"\n"
"Options:\n"
"  --help, -h   Show this message\n");
}

/*
 * Print one function's diff line. `name` is the function. `before`
 * and `after` may be NULL if the function exists only on one side.
 *
 * Format (example):
 *   function: malloc
 *     count:      before=1247, after=1532  (+285, +22.8%)
 *     total time: before=8.3ms, after=11.9ms (+3.6ms, +43.4%)
 */
static void print_func_diff(const char *name,
			    const struct FuncStat *before,
			    const struct FuncStat *after)
{
	uint64_t b_count = before ? before->call_count : 0;
	uint64_t a_count = after ? after->call_count : 0;
	uint64_t b_us = before ? before->total_duration_us : 0;
	uint64_t a_us = after ? after->total_duration_us : 0;

	/* Only print functions that changed (or are new / gone). */
	if (b_count == a_count && b_us == a_us)
		return;

	printf("function: %s\n", name);

	if (before == NULL) {
		printf("  absent in before; %llu calls in after\n",
			(unsigned long long)a_count);
		return;
	}
	if (after == NULL) {
		printf("  %llu calls in before; absent in after\n",
			(unsigned long long)b_count);
		return;
	}

	if (b_count != a_count) {
		long long delta = (long long)a_count - (long long)b_count;
		double pct = b_count ? 100.0 * (double)delta / b_count
				     : 100.0;

		printf("  count:       before=%llu, after=%llu  "
		       "(%+lld, %+.1f%%)\n",
			(unsigned long long)b_count,
			(unsigned long long)a_count, delta, pct);
	}

	if (b_us != a_us) {
		long long delta_us = (long long)a_us - (long long)b_us;
		double pct = b_us ? 100.0 * (double)delta_us / (double)b_us
				  : 100.0;

		printf("  total time:  before=%.1fms, after=%.1fms  "
		       "(%+.1fms, %+.1f%%)\n",
			(double)b_us / 1000.0, (double)a_us / 1000.0,
			(double)delta_us / 1000.0, pct);
	}
}

/*
 * Walks both normalized logs and prints every function that differs.
 * Returns the number of differing functions (0 = no diff).
 */
static int print_all_diffs(const struct NormalizedLog *before,
			   const struct NormalizedLog *after)
{
	int diffs = 0;
	size_t i;

	/* Functions in `before` -- paired or removed. */
	for (i = 0; i < before->count; i++) {
		const struct FuncStat *b = &before->funcs[i];
		const struct FuncStat *a = normalize_find(after, b->name);

		/* Only print if there's a real diff. */
		if (a != NULL &&
		    a->call_count == b->call_count &&
		    a->total_duration_us == b->total_duration_us)
			continue;

		print_func_diff(b->name, b, a);
		diffs++;
	}

	/* Functions only in `after` (new). */
	for (i = 0; i < after->count; i++) {
		const struct FuncStat *a = &after->funcs[i];

		if (normalize_find(before, a->name) != NULL)
			continue;
		print_func_diff(a->name, NULL, a);
		diffs++;
	}

	return diffs;
}

static int load_trace(const char *path, struct NormalizedLog *out)
{
	JSON_Value *root;
	JSON_Array *arr;

	root = json_parse_file(path);
	if (root == NULL) {
		fprintf(stderr, "retrace-diff: cannot parse %s\n", path);
		return -1;
	}
	arr = json_value_get_array(root);
	if (arr == NULL) {
		fprintf(stderr, "retrace-diff: %s is not a JSON array\n",
			path);
		json_value_free(root);
		return -1;
	}
	if (normalize_from_trace(arr, out) != 0) {
		fprintf(stderr, "retrace-diff: normalize failed for %s\n",
			path);
		json_value_free(root);
		return -1;
	}
	json_value_free(root);
	return 0;
}

int main(int argc, char **argv)
{
	const char *before_path = NULL;
	const char *after_path = NULL;
	struct NormalizedLog before = {0};
	struct NormalizedLog after = {0};
	int argi;
	int rc;
	int diffs;

	for (argi = 1; argi < argc; argi++) {
		if (strcmp(argv[argi], "--help") == 0 ||
		    strcmp(argv[argi], "-h") == 0) {
			usage(stdout);
			return 0;
		}
		if (argv[argi][0] == '-') {
			fprintf(stderr, "retrace-diff: unknown option '%s'\n",
				argv[argi]);
			usage(stderr);
			return 2;
		}
		if (before_path == NULL)
			before_path = argv[argi];
		else if (after_path == NULL)
			after_path = argv[argi];
		else {
			fprintf(stderr,
				"retrace-diff: too many arguments\n");
			usage(stderr);
			return 2;
		}
	}

	if (before_path == NULL || after_path == NULL) {
		fprintf(stderr, "retrace-diff: two file paths required\n");
		usage(stderr);
		return 2;
	}

	if (load_trace(before_path, &before) != 0)
		return 2;
	if (load_trace(after_path, &after) != 0) {
		normalize_free(&before);
		return 2;
	}

	diffs = print_all_diffs(&before, &after);

	if (diffs == 0)
		printf("No call-surface differences found.\n");
	else
		printf("\n%d function(s) changed.\n", diffs);

	normalize_free(&before);
	normalize_free(&after);

	/* Exit 1 if there was a diff, 0 if not. */
	rc = (diffs > 0) ? 1 : 0;
	return rc;
}
