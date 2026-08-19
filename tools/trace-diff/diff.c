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
#include "trace_load.h"
#include "normalize.h"
#include "threshold.h"
#include "lcs.h"
#include "stats.h"

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
"Statistical significance mode (TODO.complete/27 P2):\n"
"  retrace-diff --stats BASE1.json,BASE2.json,... TEST.json\n"
"                [--zscore N]\n"
"\n"
"Loads multiple baseline traces, computes per-function mean +\n"
"stddev, and flags functions whose test count deviates by more\n"
"than N standard deviations (default N=2, ~95%% confidence).\n"
"\n"
"Options:\n"
"  --threshold pct=N  Suppress diffs at or below N%% change (CI\n"
"                     gating). Default: 0 (report every change).\n"
"  --order            Also run a sequence-alignment (LCS) diff.\n"
"  --stats BASES      Statistical significance mode. BASES is a\n"
"                     comma-separated list of baseline files.\n"
"  --zscore N         Z-score threshold for --stats (default 2.0).\n"
"  --help, -h         Show this message\n"
"\n"
"Exit codes:\n"
"  0  no diff above the threshold\n"
"  1  diff found above the threshold\n"
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
		    diff_exceeds_threshold(b->call_count, a->call_count,
			cfg->threshold_pct) : 1;
		time_exceeds = a ?
		    diff_exceeds_threshold(b->total_duration_us,
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
 * Prints the alignment computed by lcs.c as unified-diff-style
 * lines: "  func" for entries in both, "- func" for before-only,
 * "+ func" for after-only. Returns the number of insertions +
 * deletions (the "edit distance"); 0 means identical order.
 */
struct print_ctx {
	int edits;
};

static int print_item(const struct diff_lcs_item *item, void *p)
{
	struct print_ctx *ctx = (struct print_ctx *)p;

	switch (item->type) {
	case DIFF_LCS_MATCH:
		printf("  %s\n", item->name);
		break;
	case DIFF_LCS_DELETE:
		printf("- %s\n", item->name);
		ctx->edits++;
		break;
	case DIFF_LCS_INSERT:
		printf("+ %s\n", item->name);
		ctx->edits++;
		break;
	}
	return 0;
}

static int print_order_diff(char **before_seq, size_t before_len,
			    char **after_seq, size_t after_len)
{
	struct print_ctx ctx = {0};
	int rc;

	if (before_len == 0 && after_len == 0)
		return 0;

	printf("\n--- order diff (LCS) ---\n");
	rc = diff_lcs_walk((const char *const *)before_seq, before_len,
			   (const char *const *)after_seq, after_len,
			   print_item, &ctx);
	if (rc < 0) {
		fprintf(stderr,
			"retrace-diff: OOM in LCS (sizes %zu x %zu)\n",
			before_len, after_len);
		return -1;
	}
	return ctx.edits;
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

	root = trace_load_file(path, NULL);
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
	if (root_out != NULL)
		*root_out = root;
	else
		json_value_free(root);
	if (arr_out != NULL)
		*arr_out = arr;
	return 0;
}

/*
 * Statistical significance mode (TODO.complete/27 P2).
 *
 * Loads N baseline traces, computes per-function mean + stddev
 * of call counts, then compares the test trace. A function is
 * "significant" if its test count deviates from the mean by
 * more than `zscore` standard deviations.
 *
 * Returns the number of significant functions (0 = none).
 */
#include <math.h>

#define MAX_BASELINES 64

static int run_stats_mode(const char *baseline_list,
			  const char *test_path, double zscore)
{
	char *list_copy = strdup(baseline_list);
	char *tok;
	struct NormalizedLog *baselines = NULL;
	int n_baselines = 0;
	int cap = 0;
	struct NormalizedLog test = {0};
	JSON_Value *test_root = NULL;
	JSON_Array *test_arr = NULL;
	int significant = 0;
	size_t i;
	int j;

	if (list_copy == NULL) {
		fprintf(stderr, "retrace-diff: OOM\n");
		return -1;
	}

	/* Parse the comma-separated baseline list and load each. */
	tok = strtok(list_copy, ",");
	while (tok != NULL) {
		struct NormalizedLog log = {0};

		if (n_baselines == cap) {
			int newcap = (cap == 0) ? 4 : cap * 2;

			baselines = realloc(baselines,
				newcap * sizeof(*baselines));
			if (baselines == NULL) {
				free(list_copy);
				return -1;
			}
			cap = newcap;
		}
		if (load_trace(tok, &log, NULL, NULL) != 0) {
			free(list_copy);
			for (i = 0; i < (size_t)n_baselines; i++)
				normalize_free(&baselines[i]);
			free(baselines);
			return -1;
		}
		baselines[n_baselines++] = log;
		tok = strtok(NULL, ",");
	}
	free(list_copy);

	if (n_baselines < 2) {
		fprintf(stderr,
			"retrace-diff: --stats needs at least 2 baselines (got %d)\n",
			n_baselines);
		for (i = 0; i < (size_t)n_baselines; i++)
			normalize_free(&baselines[i]);
		free(baselines);
		return -1;
	}

	/* Load the test trace. */
	if (load_trace(test_path, &test, &test_root, &test_arr) != 0) {
		for (i = 0; i < (size_t)n_baselines; i++)
			normalize_free(&baselines[i]);
		free(baselines);
		return -1;
	}

	printf("--- statistical significance (z > %.1f) ---\n", zscore);
	printf("baselines: %d\n", n_baselines);

	/* For each function in the test trace, compute z-score. */
	for (i = 0; i < test.count; i++) {
		const char *name = test.funcs[i].name;
		uint64_t counts[MAX_BASELINES];
		struct diff_stats st;
		int j;

		for (j = 0; j < n_baselines; j++) {
			const struct FuncStat *s = normalize_find(
				&baselines[j], name);

			counts[j] = s ? s->call_count : 0;
		}

		if (diff_stats_compute(counts, (size_t)n_baselines,
			    test.funcs[i].call_count, zscore, &st) != 0)
			continue;

		if (st.no_variance) {
			/* No variance in baselines. Flag if test
			 * differs from the constant value at all.
			 */
			if (st.significant) {
				printf("  %s: test=%llu baseline=%.1f+0.0"
				       " (NO VARIANCE -- manual review)\n",
					name,
					(unsigned long long)test.funcs[i].call_count,
					st.mean);
				significant++;
			}
		} else if (st.significant) {
			printf("  %s: test=%llu baseline=%.1f+%.1f"
			       " (z=%.2f ***)\n",
				name,
				(unsigned long long)test.funcs[i].call_count,
				st.mean, st.stddev, st.z);
			significant++;
		}
	}

	/* Check for functions that existed in baselines but not
	 * in the test (regression: feature removed).
	 */
	for (j = 0; j < n_baselines; j++) {
		size_t k;

		for (k = 0; k < baselines[j].count; k++) {
			const char *name = baselines[j].funcs[k].name;

			if (normalize_find(&test, name) == NULL) {
				/* Only report once. */
				int already = 0;
				int m;

				for (m = 0; m < j; m++) {
					if (normalize_find(&baselines[m],
						    name) != NULL) {
						already = 1;
						break;
					}
				}
				if (!already && j == 0) {
					printf("  %s: ABSENT in test (was in baseline)\n",
						name);
					significant++;
				}
			}
		}
	}

	if (significant == 0)
		printf("All functions within %.1f sigma of baseline.\n",
			zscore);
	else
		printf("\n%d function(s) significant.\n", significant);

	/* Cleanup. */
	for (i = 0; i < (size_t)n_baselines; i++)
		normalize_free(&baselines[i]);
	free(baselines);
	normalize_free(&test);
	json_value_free(test_root);
	return significant;
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
	const char *stats_baselines = NULL;
	double stats_zscore = 2.0;
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
		if (strcmp(argv[argi], "--stats") == 0 &&
		    argi + 1 < argc) {
			stats_baselines = argv[++argi];
			continue;
		}
		if (strcmp(argv[argi], "--zscore") == 0 &&
		    argi + 1 < argc) {
			stats_zscore = atof(argv[++argi]);
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

	/* Stats mode takes precedence over two-file mode. */
	if (stats_baselines != NULL) {
		const char *test_file = before_path ?
			before_path : after_path;

		if (test_file == NULL) {
			fprintf(stderr,
				"retrace-diff: --stats needs a test file\n");
			usage(stderr);
			return 2;
		}
		diffs = run_stats_mode(stats_baselines, test_file,
			stats_zscore);
		return (diffs > 0) ? 1 : 0;
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
