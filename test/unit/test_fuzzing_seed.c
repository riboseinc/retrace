/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Unit tests for the fuzzing_seed action.
 *
 * The action sets the global g_fuzzing_seed and applies it
 * immediately via srand(). Verification: calling the action with
 * the same seed twice yields identical rand() sequences.
 *
 * Tests:
 *   - Action lookup succeeds
 *   - NULL params -> -1
 *   - Missing "seed" -> -1
 *   - Setting a seed produces deterministic rand() sequence
 *   - Same seed twice -> same sequence
 *   - Different seeds -> different sequences
 *
 * Part of TODO.complete/14.
 */

#include "test_utils.h"

static JSON_Object *build_params_with_seed(double seed)
{
	return build_json_number("seed", seed);
}

static JSON_Object *build_empty_params(void)
{
	JSON_Value *root_val = json_value_init_object();
	JSON_Object *root = json_value_get_object(root_val);

	json_object_set_string(root, "unrelated", "foo");
	return root;
}

static void sample_rand_sequence(int *out, int n)
{
	int i;

	for (i = 0; i < n; i++)
		out[i] = rand();
}

static int sequences_equal(const int *a, const int *b, int n)
{
	int i;

	for (i = 0; i < n; i++)
		if (a[i] != b[i])
			return 0;
	return 1;
}

DECLARE_TEST_STATE();

static void test_action_lookup(void)
{
	retrace_actions_init();
	CHECK(retrace_actions_get("fuzzing_seed") != NULL);
}

static void test_params_null(void)
{
	action_fn_t action = retrace_actions_get("fuzzing_seed");
	struct ThreadContext ctx;
	int rc;

	memset(&ctx, 0, sizeof(ctx));
	rc = action(&ctx, NULL);
	CHECK(rc == -1);
}

static void test_missing_seed(void)
{
	action_fn_t action = retrace_actions_get("fuzzing_seed");
	struct ThreadContext ctx;
	JSON_Object *params = build_empty_params();
	int rc;

	memset(&ctx, 0, sizeof(ctx));
	rc = action(&ctx, params);
	CHECK(rc == -1);

	json_value_free(json_object_get_wrapping_value(params));
}

static void test_seed_makes_rand_deterministic(void)
{
	action_fn_t action = retrace_actions_get("fuzzing_seed");
	struct ThreadContext ctx;
	int seq_a[5];
	int seq_b[5];
	JSON_Object *p1 = build_params_with_seed(42.0);
	JSON_Object *p2 = build_params_with_seed(42.0);
	int rc;

	memset(&ctx, 0, sizeof(ctx));

	rc = action(&ctx, p1);
	CHECK(rc == 0);
	sample_rand_sequence(seq_a, 5);

	rc = action(&ctx, p2);
	CHECK(rc == 0);
	sample_rand_sequence(seq_b, 5);

	CHECK(sequences_equal(seq_a, seq_b, 5));

	json_value_free(json_object_get_wrapping_value(p1));
	json_value_free(json_object_get_wrapping_value(p2));
}

static void test_different_seeds_produce_different_sequences(void)
{
	action_fn_t action = retrace_actions_get("fuzzing_seed");
	struct ThreadContext ctx;
	int seq_a[5];
	int seq_b[5];
	JSON_Object *p1 = build_params_with_seed(1.0);
	JSON_Object *p2 = build_params_with_seed(2.0);
	int rc;

	memset(&ctx, 0, sizeof(ctx));

	rc = action(&ctx, p1);
	CHECK(rc == 0);
	sample_rand_sequence(seq_a, 5);

	rc = action(&ctx, p2);
	CHECK(rc == 0);
	sample_rand_sequence(seq_b, 5);

	CHECK(!sequences_equal(seq_a, seq_b, 5));

	json_value_free(json_object_get_wrapping_value(p1));
	json_value_free(json_object_get_wrapping_value(p2));
}

static void test_seed_zero(void)
{
	action_fn_t action = retrace_actions_get("fuzzing_seed");
	struct ThreadContext ctx;
	int seq_a[5];
	int seq_b[5];
	JSON_Object *p1 = build_params_with_seed(0.0);
	JSON_Object *p2 = build_params_with_seed(0.0);
	int rc;

	memset(&ctx, 0, sizeof(ctx));

	rc = action(&ctx, p1);
	CHECK(rc == 0);
	sample_rand_sequence(seq_a, 5);

	rc = action(&ctx, p2);
	CHECK(rc == 0);
	sample_rand_sequence(seq_b, 5);

	CHECK(sequences_equal(seq_a, seq_b, 5));

	json_value_free(json_object_get_wrapping_value(p1));
	json_value_free(json_object_get_wrapping_value(p2));
}

int main(void)
{
	init_minimal_real_impls();
	INIT_TESTS();

	printf("fuzzing_seed tests:\n");

	printf("  -- lookup + params validation --\n");
	TEST(action_lookup);
	TEST(params_null);
	TEST(missing_seed);

	printf("  -- determinism --\n");
	TEST(seed_makes_rand_deterministic);
	TEST(different_seeds_produce_different_sequences);
	TEST(seed_zero);

	return finish_tests(tests_run, tests_pass, tests_fail);
}
