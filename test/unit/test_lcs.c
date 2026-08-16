/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Unit tests for the LCS alignment (TODO.complete/27 P1).
 *
 * diff_lcs_walk drives the call-order diff (cookbook 26): the
 * edit count is what CI gates on and the alignment is what users
 * read. Wrong LCS = false "behavior changed" alarms or missed
 * reorderings.
 *
 * Covers:
 *   - diff_lcs_len: identical / disjoint / interleaved / classic
 *     LCS examples, empty inputs, prefix/suffix cases
 *   - diff_lcs_walk: identical sequences emit all MATCH, 0 edits
 *   - disjoint sequences emit only DELETE + INSERT
 *   - one-side-empty emits only that side's items
 *   - edit count = len(a) + len(b) - 2*lcs_len (identity)
 *   - emitted item count = len(a) + len(b) - lcs_len
 *   - early-stop: callback returning non-zero halts the walk
 *   - MATCH items carry the before-side name; INSERT carry the
 *     after-side name
 *
 * Items arrive in back-walk (tail-first) order by design -- the
 * tests record then reverse where head-first assertions are
 * clearer.
 */

#include "lcs.h"

#include <stdio.h>
#include <string.h>

static int tests_run;
static int tests_pass;
static int tests_fail;

#define TEST(name) do { \
	tests_run++; \
	printf("  TEST %s ... ", #name); \
	test_##name(); \
	tests_pass++; \
	printf("OK\n"); \
} while (0)

#define CHECK(cond) do { \
	if (!(cond)) { \
		printf("FAIL [%s:%d] %s\n", __FILE__, __LINE__, #cond); \
		tests_fail++; \
		return; \
	} \
} while (0)

/* Recording callback: captures items into a fixed array. */
#define MAX_ITEMS 64
struct Record {
	struct diff_lcs_item items[MAX_ITEMS];
	int count;
	int stop_after;  /* if > 0, stop after this many items */
};

static int record_cb(const struct diff_lcs_item *item, void *p)
{
	struct Record *r = (struct Record *)p;

	if (r->count < MAX_ITEMS)
		r->items[r->count++] = *item;
	if (r->stop_after > 0 && r->count >= r->stop_after)
		return 1;
	return 0;
}

static void record_init(struct Record *r)
{
	memset(r, 0, sizeof(*r));
}

static int count_type(const struct Record *r, enum diff_lcs_type t)
{
	int i, n = 0;

	for (i = 0; i < r->count; i++)
		if (r->items[i].type == t)
			n++;
	return n;
}

/* Reverse a record in place (back-walk -> head-first). */
static void record_reverse(struct Record *r)
{
	int i;

	for (i = 0; i < r->count / 2; i++) {
		struct diff_lcs_item tmp = r->items[i];

		r->items[i] = r->items[r->count - 1 - i];
		r->items[r->count - 1 - i] = tmp;
	}
}

/* ----- diff_lcs_len ----- */

static void test_len_identical(void)
{
	const char *a[] = {"open", "read", "close"};

	CHECK(diff_lcs_len(a, 3, a, 3) == 3);
}

static void test_len_disjoint(void)
{
	const char *a[] = {"open", "read"};
	const char *b[] = {"send", "recv"};

	CHECK(diff_lcs_len(a, 2, b, 2) == 0);
}

static void test_len_interleaved(void)
{
	const char *a[] = {"a", "b", "c", "d"};
	const char *b[] = {"b", "d", "a", "c"};

	/* LCS is "b","c" or "b","d" or "a","c" -- length 2. */
	CHECK(diff_lcs_len(a, 4, b, 4) == 2);
}

static void test_len_classic(void)
{
	const char *a[] = {"a", "b", "c", "b", "d", "a", "b"};
	const char *b[] = {"b", "d", "c", "a", "b", "a"};

	/* Classic textbook LCS: "b","d","a","b" length 4
	 * (also "b","c","a","b" length 4).
	 */
	CHECK(diff_lcs_len(a, 7, b, 6) == 4);
}

static void test_len_empty_inputs(void)
{
	const char *a[] = {"open"};

	CHECK(diff_lcs_len(a, 1, a, 0) == 0);
	CHECK(diff_lcs_len(a, 0, a, 1) == 0);
	CHECK(diff_lcs_len(a, 0, a, 0) == 0);
}

static void test_len_prefix_suffix(void)
{
	const char *a[] = {"open", "read", "write", "close"};
	const char *prefix[] = {"open", "read"};

	CHECK(diff_lcs_len(a, 4, prefix, 2) == 2);
	/* Same elements, same relative order -> full LCS. */
	CHECK(diff_lcs_len(a, 4, a, 4) == 4);
}

static void test_len_reordering_only(void)
{
	/* Same multiset, different order: LCS is the longest
	 * increasing subsequence mapping -- for abc vs acb it's 2.
	 */
	const char *a[] = {"a", "b", "c"};
	const char *b[] = {"a", "c", "b"};

	CHECK(diff_lcs_len(a, 3, b, 3) == 2);
}

/* ----- diff_lcs_walk ----- */

static void test_walk_identical_all_match_zero_edits(void)
{
	const char *a[] = {"open", "read", "close"};
	struct Record r;

	record_init(&r);
	CHECK(diff_lcs_walk(a, 3, a, 3, record_cb, &r) == 3);
	CHECK(r.count == 3);
	CHECK(count_type(&r, DIFF_LCS_MATCH) == 3);
	CHECK(count_type(&r, DIFF_LCS_DELETE) == 0);
	CHECK(count_type(&r, DIFF_LCS_INSERT) == 0);
}

static void test_walk_disjoint_only_edits(void)
{
	const char *a[] = {"open", "read"};
	const char *b[] = {"send", "recv"};
	struct Record r;

	record_init(&r);
	CHECK(diff_lcs_walk(a, 2, b, 2, record_cb, &r) == 4);
	CHECK(count_type(&r, DIFF_LCS_MATCH) == 0);
	CHECK(count_type(&r, DIFF_LCS_DELETE) == 2);
	CHECK(count_type(&r, DIFF_LCS_INSERT) == 2);
}

static void test_walk_empty_before_all_insert(void)
{
	const char *b[] = {"send", "recv", "close"};
	struct Record r;

	record_init(&r);
	CHECK(diff_lcs_walk(NULL, 0, b, 3, record_cb, &r) == 3);
	CHECK(count_type(&r, DIFF_LCS_INSERT) == 3);
	CHECK(count_type(&r, DIFF_LCS_MATCH) == 0);
}

static void test_walk_empty_after_all_delete(void)
{
	const char *a[] = {"open", "read", "close"};
	struct Record r;

	record_init(&r);
	CHECK(diff_lcs_walk(a, 3, NULL, 0, record_cb, &r) == 3);
	CHECK(count_type(&r, DIFF_LCS_DELETE) == 3);
	CHECK(count_type(&r, DIFF_LCS_MATCH) == 0);
}

static void test_walk_edit_count_identity(void)
{
	/* edits = alen + blen - 2*lcs_len, for several shapes. */
	const char *a1[] = {"open", "read", "close"};
	const char *b1[] = {"open", "close", "read"};
	const char *a2[] = {"a", "b", "c", "d"};
	const char *b2[] = {"b", "d", "a", "c"};
	const char *a3[] = {"x"};
	const char *b3[] = {"x", "y"};
	struct Record r;

	record_init(&r);
	diff_lcs_walk(a1, 3, b1, 3, record_cb, &r);
	CHECK(count_type(&r, DIFF_LCS_DELETE) +
	      count_type(&r, DIFF_LCS_INSERT) ==
	      3 + 3 - 2 * diff_lcs_len(a1, 3, b1, 3));

	record_init(&r);
	diff_lcs_walk(a2, 4, b2, 4, record_cb, &r);
	CHECK(count_type(&r, DIFF_LCS_DELETE) +
	      count_type(&r, DIFF_LCS_INSERT) ==
	      4 + 4 - 2 * diff_lcs_len(a2, 4, b2, 4));

	record_init(&r);
	diff_lcs_walk(a3, 1, b3, 2, record_cb, &r);
	CHECK(count_type(&r, DIFF_LCS_DELETE) +
	      count_type(&r, DIFF_LCS_INSERT) ==
	      1 + 2 - 2 * diff_lcs_len(a3, 1, b3, 2));
}

static void test_walk_item_count_identity(void)
{
	/* items = alen + blen - lcs_len. */
	const char *a[] = {"open", "read", "close"};
	const char *b[] = {"open", "close", "read"};
	struct Record r;

	record_init(&r);
	CHECK(diff_lcs_walk(a, 3, b, 3, record_cb, &r) ==
		3 + 3 - (int)diff_lcs_len(a, 3, b, 3));
	CHECK(r.count == 3 + 3 - (int)diff_lcs_len(a, 3, b, 3));
}

static void test_walk_names_come_from_correct_side(void)
{
	/* MATCH and DELETE names are pointers into the before
	 * sequence; INSERT names point into the after sequence.
	 */
	const char *before[] = {"shared", "onlybefore"};
	const char *after[] = {"shared", "onlyafter"};
	struct Record r;
	int i;
	int saw_shared_match = 0;
	int saw_before_delete = 0;
	int saw_after_insert = 0;

	record_init(&r);
	diff_lcs_walk(before, 2, after, 2, record_cb, &r);
	CHECK(r.count == 3);

	for (i = 0; i < r.count; i++) {
		if (r.items[i].type == DIFF_LCS_MATCH &&
		    strcmp(r.items[i].name, "shared") == 0)
			saw_shared_match = 1;
		if (r.items[i].type == DIFF_LCS_DELETE &&
		    r.items[i].name == before[1])
			saw_before_delete = 1;
		if (r.items[i].type == DIFF_LCS_INSERT &&
		    r.items[i].name == after[1])
			saw_after_insert = 1;
	}
	CHECK(saw_shared_match == 1);
	CHECK(saw_before_delete == 1);
	CHECK(saw_after_insert == 1);
}

static void test_walk_alignment_shape_reordering(void)
{
	/* before: open read close / after: open close read.
	 * LCS length 2 ("open" plus one of read/close). Head-first
	 * alignment: MATCH open, then 2 more matches? No -- exactly
	 * 2 matches total, 1 delete, 1 insert.
	 */
	const char *a[] = {"open", "read", "close"};
	const char *b[] = {"open", "close", "read"};
	struct Record r;

	record_init(&r);
	diff_lcs_walk(a, 3, b, 3, record_cb, &r);
	record_reverse(&r);

	CHECK(r.count == 4);
	CHECK(count_type(&r, DIFF_LCS_MATCH) == 2);
	CHECK(count_type(&r, DIFF_LCS_DELETE) == 1);
	CHECK(count_type(&r, DIFF_LCS_INSERT) == 1);
	CHECK(r.items[0].type == DIFF_LCS_MATCH);
	CHECK(strcmp(r.items[0].name, "open") == 0);
}

static void test_walk_early_stop(void)
{
	const char *a[] = {"a", "b", "c", "d"};
	struct Record r;

	record_init(&r);
	r.stop_after = 2;
	CHECK(diff_lcs_walk(a, 4, a, 4, record_cb, &r) == 2);
	CHECK(r.count == 2);
}

static void test_walk_null_callback_ok(void)
{
	const char *a[] = {"a", "b"};

	/* NULL cb must not crash; returns the item count. */
	CHECK(diff_lcs_walk(a, 2, a, 2, NULL, NULL) == 2);
}

int main(void)
{
	printf("-- diff_lcs_len --\n");
	TEST(len_identical);
	TEST(len_disjoint);
	TEST(len_interleaved);
	TEST(len_classic);
	TEST(len_empty_inputs);
	TEST(len_prefix_suffix);
	TEST(len_reordering_only);

	printf("-- diff_lcs_walk --\n");
	TEST(walk_identical_all_match_zero_edits);
	TEST(walk_disjoint_only_edits);
	TEST(walk_empty_before_all_insert);
	TEST(walk_empty_after_all_delete);
	TEST(walk_edit_count_identity);
	TEST(walk_item_count_identity);
	TEST(walk_names_come_from_correct_side);
	TEST(walk_alignment_shape_reordering);
	TEST(walk_early_stop);
	TEST(walk_null_callback_ok);

	printf("\nPass: %d, Fail: %d (of %d)\n",
		tests_pass, tests_fail, tests_run);
	return tests_fail == 0 ? 0 : 1;
}
