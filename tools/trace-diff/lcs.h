/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

#ifndef RETRACE_DIFF_LCS_H_
#define RETRACE_DIFF_LCS_H_

#include <stddef.h>

/*
 * Longest-common-subsequence alignment for call-order diffing
 * (TODO.complete/27 P1).
 *
 * Owns the pure algorithm: given two sequences of names, compute
 * the LCS table and reconstruct the alignment. Presentation (the
 * "  / - / +" rendering) lives in diff.c; this module only emits
 * typed alignment items via a callback.
 */

enum diff_lcs_type {
	DIFF_LCS_MATCH = 0,   /* present in both, same relative order */
	DIFF_LCS_DELETE = 1,  /* only in the before sequence */
	DIFF_LCS_INSERT = 2   /* only in the after sequence */
};

struct diff_lcs_item {
	enum diff_lcs_type type;

	/* The name. For MATCH/DELETE: from the before sequence; for
	 * INSERT: from the after sequence. Borrowed, not copied.
	 */
	const char *name;
};

/*
 * Callback invoked once per alignment item. Items are emitted
 * tail-first by the back-walk, so callers that need head-first
 * order must reverse (diff.c prints as it goes -- for a
 * unified-diff look it emits in reverse and the terminal shows
 * the natural order). Return non-zero from the callback to stop
 * the walk early.
 */
typedef int (*diff_lcs_cb)(const struct diff_lcs_item *item,
			   void *ctx);

/*
 * Returns the length of the longest common subsequence of the two
 * name sequences (compared with strcmp). O(a*b) time, O(a*b)
 * space. Returns 0 if either sequence is empty.
 */
size_t diff_lcs_len(const char *const *a, size_t alen,
		    const char *const *b, size_t blen);

/*
 * Walks the alignment, invoking `cb` once per item (MATCH, DELETE
 * or INSERT). The number of non-MATCH items is the edit count.
 * Returns the number of items emitted, or -1 on OOM.
 */
int diff_lcs_walk(const char *const *a, size_t alen,
		  const char *const *b, size_t blen,
		  diff_lcs_cb cb, void *ctx);

#endif /* RETRACE_DIFF_LCS_H_ */
