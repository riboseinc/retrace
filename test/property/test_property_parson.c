// SPDX-License-Identifier: BSD-2-Clause
//
// Property-based tests for parson (the vendored JSON library).
//
// P-PARSE-NEVER-CRASH: for any byte sequence, json_parse_string
// either returns a valid JSON_Value* or NULL. It never crashes.
//
// P-PARSE-COMMENTS-NEVER-CRASH: same, but with the comment-tolerant
// variant. Comments can be slash-slash or slash-star. The tolerant
// parser is what retrace actually uses, so it gets its own property.
//
// P-PARSE-ROUNDTRIP: for any value built via parson's builder API,
// serializing then re-parsing produces an equal value (deep).

#include "property_harness.h"
#include "parson.h"

#include <string.h>

static void gen_random_text(struct ret_prng *p, char *buf, size_t cap)
{
	size_t n;
	size_t i;

	if (cap == 0)
		return;

	n = ret_prng_u32(p, cap);
	for (i = 0; i < n; i++)
		buf[i] = (char)(' ' + ret_prng_u32(p, 127 - ' '));
	buf[n] = '\0';
}

static int prop_parse_never_crash(uint64_t seed)
{
	struct ret_prng prng;
	char buf[256];
	JSON_Value *v;

	ret_prng_seed(&prng, seed);
	gen_random_text(&prng, buf, sizeof(buf) - 1);

	v = json_parse_string(buf);

	/* The contract: v is either NULL (parse error) or a valid
	 * JSON_Value*. Freeing NULL is a no-op per parson docs.
	 */
	json_value_free(v);

	return 1;
}

static int prop_parse_with_comments_never_crash(uint64_t seed)
{
	struct ret_prng prng;
	char buf[256];
	JSON_Value *v;

	ret_prng_seed(&prng, seed);
	gen_random_text(&prng, buf, sizeof(buf) - 1);

	/* Sprinkle in some comment markers; parson's tolerant parser
	 * must handle them.
	 */
	if (ret_prng_u32(&prng, 4) == 0) {
		size_t pos = ret_prng_u32(&prng, sizeof(buf) - 4);

		buf[pos] = '/';
		buf[pos + 1] = '/';
	}
	if (ret_prng_u32(&prng, 5) == 0) {
		size_t pos = ret_prng_u32(&prng, sizeof(buf) - 4);

		buf[pos] = '/';
		buf[pos + 1] = '*';
	}

	v = json_parse_string_with_comments(buf);
	json_value_free(v);

	return 1;
}

static int prop_parse_roundtrip(uint64_t seed)
{
	struct ret_prng prng;
	JSON_Value *root;
	JSON_Object *obj;
	char key[32];
	char val[32];
	int n_keys;
	int i;
	char *serialized;
	JSON_Value *reparsed;
	char *reserialized;
	int equal;

	ret_prng_seed(&prng, seed);

	root = json_value_init_object();
	obj = json_value_get_object(root);

	n_keys = 1 + (int)ret_prng_u32(&prng, 5);
	for (i = 0; i < n_keys; i++) {
		snprintf(key, sizeof(key), "k%lu",
			 (unsigned long)ret_prng_u32(&prng, 1000));
		snprintf(val, sizeof(val), "v%lu",
			 (unsigned long)ret_prng_u32(&prng, 1000));
		json_object_set_string(obj, key, val);
	}

	serialized = json_serialize_to_string(root);
	if (!serialized) {
		json_value_free(root);
		return 1;
	}

	reparsed = json_parse_string(serialized);
	if (!reparsed) {
		/* The serialization must be parseable. This is the
		 * actual invariant.
		 */
		printf("[property] roundtrip FAILED: reparse returned NULL\n");
		printf("[property]   serialized: %s\n", serialized);
		json_free_serialized_string(serialized);
		json_value_free(root);
		return 0;
	}

	/* Reserialize the reparsed value and compare strings. Equal
	 * strings means structurally equal.
	 */
	reserialized = json_serialize_to_string(reparsed);
	equal = (reserialized && strcmp(serialized, reserialized) == 0);

	if (!equal) {
		printf("[property] roundtrip FAILED: reserialize differs\n");
		printf("[property]   original:    %s\n", serialized);
		printf("[property]   reserialized: %s\n",
		       reserialized ? reserialized : "(null)");
	}

	json_free_serialized_string(reserialized);
	json_free_serialized_string(serialized);
	json_value_free(reparsed);
	json_value_free(root);

	return equal ? 1 : 0;
}

int main(void)
{
	int failures = 0;

	failures += property_run(prop_parse_never_crash,
				 "prop_parse_never_crash",
				 RETRACE_PROPERTY_DEFAULT_ITERS * 10, 1);
	failures += property_run(prop_parse_with_comments_never_crash,
				 "prop_parse_with_comments_never_crash",
				 RETRACE_PROPERTY_DEFAULT_ITERS * 10, 1);
	failures += property_run(prop_parse_roundtrip,
				 "prop_parse_roundtrip",
				 RETRACE_PROPERTY_DEFAULT_ITERS, 1);

	if (failures)
		printf("[property] %d property(ies) failed\n", failures);
	else
		printf("[property] all properties passed\n");

	return failures ? 1 : 0;
}
