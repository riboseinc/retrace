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
"retrace-diff -- differential trace analysis (TODO.complete/27)\n"
"\n"
"Usage:\n"
"  retrace-diff [OPTIONS] BEFORE.json AFTER.json\n"
"\n"
"Reads two retrace trace logs (each a JSON array of entries with\n"
"`message.func` and `message.call_duration_us`), groups calls by\n"
"function, and prints a structured diff of call counts and total\n"
"time.\n"
"\n"
"Options:\n"
"  --threshold pct=N  Suppress diffs at or below N%% change (CI\n"
"                     gating). Counted diffs only if a function's\n"
"                     call count OR total time changed by >N%%.\n"
"                     New/removed functions always count.\n"
"                     Default: 0 (report every change).\n"
"  --order            Also run a sequence-alignment (LCS) diff\n"
"                     to detect call-ORDER changes even when\n"
"                     per-function counts are identical.\n"
"  --help, -h         Show this message\n"
"\n"
"Exit codes:\n"
"  0  no diff above the threshold (call surface within tolerance)\n"
"  1  diff found above the threshold (CI gating signal)\n"
"  2  argument or IO error\n");
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
 * Configuration for the diff walk. The threshold_pct field
 * implements TODO.complete/27 P1 (CI gating): only count a
 * function as "changed" if its count OR total time changed by
 * more than N percent. Default 0 = report every change.
 *
 * New/removed functions always count (they're 100% changes by
 * definition; you can't get more divergent than "absent").
 */
struct DiffConfig {
	double threshold_pct;

	/* When non-zero, also run an ordering diff (TODO.complete/27 P1)
	 * via longest common subsequence. The output is appended to
	 * the standard per-function diff.
	 */
	int order_diff;
};

/*
 * Returns 1 if the change exceeds the configured threshold, 0
 * otherwise. A function with zero before-count and nonzero after
 * always exceeds (avoids divide-by-zero).
 */
static int exceeds_threshold(uint64_t before, uint64_t after,
			     double threshold_pct)
{
	double delta;
	double pct;

	if (before == after)
		return 0;
	if (threshold_pct <= 0.0)
		return 1;
	if (before == 0)
		return 1;  /* 0 -> N is unbounded; always report */

	delta = (double)((long long)after - (long long)before);
	pct = 100.0 * (delta < 0 ? -delta : delta) / (double)before;
	return pct > threshold_pct;
}

/*
 * Walks both normalized logs and prints every function that differs.
 * Returns the number of differing functions (0 = no diff).
 */
static int print_all_diffs(const struct NormalizedLog *before,
			   const struct NormalizedLog *after,
			   const struct DiffConfig *cfg)
{
	int diffs = 0;
	size_t i;

	/* Functions in `before` -- paired or removed. */
	for (i = 0; i < before->count; i++) {
		const struct FuncStat *b = &before->funcs[i];
		const struct FuncStat *a = normalize_find(after, b->name);
		int count_exceeds;
		int time_exceeds;

		/* Only print if there's a real diff. */
		if (a != NULL &&
		    a->call_count == b->call_count &&
		    a->total_duration_us == b->total_duration_us)
			continue;

		/* If threshold is set, count this diff only if it
		 * exceeds the threshold on count or time.
		 */
		count_exceeds = a ?
		    exceeds_threshold(b->call_count, a->call_count,
			cfg->threshold_pct) : 1;
		time_exceeds = a ?
		    exceeds_threshold(b->total_duration_us,
			a->total_duration_us, cfg->threshold_pct) : 1;

		print_func_diff(b->name, b, a);
		if (count_exceeds || time_exceeds)
			diffs++;
	}

	/* Functions only in `after` (new). */
	for (i = 0; i < after->count; i++) {
		const struct FuncStat *a = &after->funcs[i];

		if (normalize_find(before, a->name) != NULL)
			continue;
		print_func_diff(a->name, NULL, a);
		/* New functions always exceed the threshold. */
		diffs++;
	}

	return diffs;
}

/*
 * Order diff via longest common subsequence (TODO.complete/27 P1).
 *
 * Computes the LCS of two function-name sequences and prints
 * the differences as unified-diff-style insertions / deletions.
 * Each entry in the LCS appears as a context line ("  func");
 * each before-only entry appears as a deletion ("- func"); each
 * after-only entry appears as an insertion ("+ func").
 *
 * Returns the number of insertions + deletions (i.e. the
 * "edit distance"). 0 means identical order.
 *
 * Standard DP. O(N*M) memory for the table; bounded by trace
 * length. For typical retrace runs (< 10K calls) this is fine.
 */
static int print_order_diff(char **before_seq, size_t before_len,
			    char **after_seq, size_t after_len)
{
	size_t i, j;
	size_t *dp;
	size_t dp_rows = before_len + 1;
	size_t dp_cols = after_len + 1;
	int edits = 0;

	if (before_len == 0 && after_len == 0)
		return 0;

	/* Allocate the (before_len+1) x (after_len+1) table. */
	dp = (size_t *)calloc(dp_rows * dp_cols, sizeof(size_t));
	if (dp == NULL) {
		fprintf(stderr,
			"retrace-diff: OOM in LCS (sizes %zu x %zu)\n",
			before_len, after_len);
		return -1;
	}

	/* Fill the table. */
	for (i = 1; i < dp_rows; i++) {
		for (j = 1; j < dp_cols; j++) {
			if (strcmp(before_seq[i - 1], after_seq[j - 1]) == 0)
				dp[i * dp_cols + j] =
					dp[(i - 1) * dp_cols + (j - 1)] + 1;
			else
				dp[i * dp_cols + j] =
					(dp[(i - 1) * dp_cols + j] >
					 dp[i * dp_cols + (j - 1)])
					? dp[(i - 1) * dp_cols + j]
					: dp[i * dp_cols + (j - 1)];
		}
	}

	/* Walk back from [before_len][after_len] to reconstruct. */
	printf("\n--- order diff (LCS) ---\n");
	i = before_len;
	j = after_len;
	while (i > 0 && j > 0) {
		if (strcmp(before_seq[i - 1], after_seq[j - 1]) == 0) {
			printf("  %s\n", before_seq[i - 1]);
			i--;
			j--;
		} else if (dp[(i - 1) * dp_cols + j] >=
			   dp[i * dp_cols + (j - 1)]) {
			printf("- %s\n", before_seq[i - 1]);
			i--;
			edits++;
		} else {
			printf("+ %s\n", after_seq[j - 1]);
			j--;
			edits++;
		}
	}
	while (i > 0) {
		printf("- %s\n", before_seq[i - 1]);
		i--;
		edits++;
	}
	while (j > 0) {
		printf("+ %s\n", after_seq[j - 1]);
		j--;
		edits++;
	}

	free(dp);
	return edits;
}

/*
 * Load a trace file, returning the parsed JSON_Array and a
 * NormalizedLog summary. Caller frees both via json_value_free
 * and normalize_free.
 */
static int load_trace(const char *path, struct NormalizedLog *out,
		      JSON_Value **root_out, JSON_Array **arr_out)
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
	*root_out = root;
	*arr_out = arr;
	return 0;
}

int main(int argc, char **argv)
{
	const char *before_path = NULL;
	const char *after_path = NULL;
	struct NormalizedLog before = {0};
	struct NormalizedLog after = {0};
	struct DiffConfig cfg = {.threshold_pct = 0.0, .order_diff = 0};
	JSON_Value *trace_root_before = NULL;
	JSON_Value *trace_root_after = NULL;
	JSON_Array *trace_arr_before = NULL;
	JSON_Array *trace_arr_after = NULL;
	int argi;
	int rc;
	int diffs;

	for (argi = 1; argi < argc; argi++) {
		if (strcmp(argv[argi], "--help") == 0 ||
		    strcmp(argv[argi], "-h") == 0) {
			usage(stdout);
			return 0;
		}
		if (strcmp(argv[argi], "--order") == 0) {
			cfg.order_diff = 1;
			continue;
		}
		if (strncmp(argv[argi], "--threshold", 11) == 0) {
			const char *val = NULL;

			/* Accept either `--threshold N` or
			 * `--threshold=pct=N` / `--threshold pct=N`.
			 */
			if (argv[argi][11] == '=') {
				val = argv[argi] + 12;
			} else if (argv[argi][11] == '\0' &&
				   argi + 1 < argc) {
				val = argv[++argi];
			} else {
				fprintf(stderr,
					"retrace-diff: bad --threshold form\n");
				usage(stderr);
				return 2;
			}
			/* Skip optional `pct=` prefix. */
			if (strncmp(val, "pct=", 4) == 0)
				val += 4;
			cfg.threshold_pct = atof(val);
			if (cfg.threshold_pct < 0.0) {
				fprintf(stderr,
					"retrace-diff: --threshold must be >= 0\n");
				return 2;
			}
			continue;
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

	if (load_trace(before_path, &before, &trace_root_before,
		&trace_arr_before) != 0)
		return 2;
	if (load_trace(after_path, &after, &trace_root_after,
		&trace_arr_after) != 0) {
		normalize_free(&before);
		json_value_free(trace_root_before);
		return 2;
	}

	diffs = print_all_diffs(&before, &after, &cfg);

	if (cfg.order_diff) {
		char **before_seq = NULL;
		char **after_seq = NULL;
		size_t before_len = 0;
		size_t after_len = 0;
		int order_edits;

		if (normalize_call_sequence(trace_arr_before,
			    &before_seq, &before_len) != 0 ||
		    normalize_call_sequence(trace_arr_after,
			    &after_seq, &after_len) != 0) {
			fprintf(stderr,
				"retrace-diff: OOM building call sequences\n");
		} else {
			order_edits = print_order_diff(before_seq,
				before_len, after_seq, after_len);
			if (order_edits > 0) {
				printf("\n%d call-order edit(s) found.\n",
					order_edits);
				diffs++;
			} else {
				printf("\nCall order identical.\n");
			}
		}
		for (size_t k = 0; k < before_len; k++)
			free(before_seq[k]);
		for (size_t k = 0; k < after_len; k++)
			free(after_seq[k]);
		free(before_seq);
		free(after_seq);
	}

	if (diffs == 0) {
		if (cfg.threshold_pct > 0.0)
			printf("No call-surface differences above %.1f%% threshold.\n",
				cfg.threshold_pct);
		else
			printf("No call-surface differences found.\n");
	} else {
		printf("\n%d function(s) changed", diffs);
		if (cfg.threshold_pct > 0.0)
			printf(" above %.1f%% threshold", cfg.threshold_pct);
		printf(".\n");
	}

	normalize_free(&before);
	normalize_free(&after);
	json_value_free(trace_root_before);
	json_value_free(trace_root_after);

	/* Exit 1 if there was a diff, 0 if not. */
	rc = (diffs > 0) ? 1 : 0;
	return rc;
}
