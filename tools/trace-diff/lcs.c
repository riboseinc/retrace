/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

#include "lcs.h"

#include <stdlib.h>
#include <string.h>

/*
 * Shared DP-table builder. Returns a calloc'd (alen+1) x (blen+1)
 * size_t table (caller frees), or NULL on OOM. dp[i*cols + j] is
 * the LCS length of the first i of a and the first j of b.
 */
static size_t *lcs_build_table(const char *const *a, size_t alen,
			       const char *const *b, size_t blen)
{
	size_t rows = alen + 1;
	size_t cols = blen + 1;
	size_t *dp;
	size_t i, j;

	dp = (size_t *)calloc(rows * cols, sizeof(size_t));
	if (dp == NULL)
		return NULL;

	for (i = 1; i < rows; i++) {
		for (j = 1; j < cols; j++) {
			if (strcmp(a[i - 1], b[j - 1]) == 0)
				dp[i * cols + j] =
					dp[(i - 1) * cols + (j - 1)] + 1;
			else
				dp[i * cols + j] =
					(dp[(i - 1) * cols + j] >
					 dp[i * cols + (j - 1)])
					? dp[(i - 1) * cols + j]
					: dp[i * cols + (j - 1)];
		}
	}

	return dp;
}

size_t diff_lcs_len(const char *const *a, size_t alen,
		    const char *const *b, size_t blen)
{
	size_t cols = blen + 1;
	size_t *dp;
	size_t len;

	if (alen == 0 || blen == 0)
		return 0;

	dp = lcs_build_table(a, alen, b, blen);
	if (dp == NULL)
		return 0;

	len = dp[alen * cols + blen];
	free(dp);
	return len;
}

int diff_lcs_walk(const char *const *a, size_t alen,
		  const char *const *b, size_t blen,
		  diff_lcs_cb cb, void *ctx)
{
	size_t cols = blen + 1;
	size_t *dp;
	size_t i, j;
	int emitted = 0;
	struct diff_lcs_item item;

	if (alen == 0 && blen == 0)
		return 0;

	dp = lcs_build_table(a, alen, b, blen);
	if (dp == NULL)
		return -1;

	i = alen;
	j = blen;
	while (i > 0 && j > 0) {
		if (strcmp(a[i - 1], b[j - 1]) == 0) {
			item.type = DIFF_LCS_MATCH;
			item.name = a[i - 1];
			i--;
			j--;
		} else if (dp[(i - 1) * cols + j] >=
			   dp[i * cols + (j - 1)]) {
			item.type = DIFF_LCS_DELETE;
			item.name = a[i - 1];
			i--;
		} else {
			item.type = DIFF_LCS_INSERT;
			item.name = b[j - 1];
			j--;
		}
		emitted++;
		if (cb != NULL && cb(&item, ctx) != 0)
			goto out;
	}
	while (i > 0) {
		item.type = DIFF_LCS_DELETE;
		item.name = a[i - 1];
		i--;
		emitted++;
		if (cb != NULL && cb(&item, ctx) != 0)
			goto out;
	}
	while (j > 0) {
		item.type = DIFF_LCS_INSERT;
		item.name = b[j - 1];
		j--;
		emitted++;
		if (cb != NULL && cb(&item, ctx) != 0)
			goto out;
	}

out:
	free(dp);
	return emitted;
}
