// SPDX-License-Identifier: BSD-2-Clause
//
// Property tests for the caller_match module (TODO.complete/17
// acceptance criterion P14).
//
// P14 from the TODO doc: "any script with caller_matches either
// matches a call site or doesn't -- no in-between." Translates
// to: each matcher returns a definitive -1 / 0 / 1, never an
// ambiguous value, and never crashes for any (ret_addr, spec)
// pair.
//
// Properties:
//
//   P-CALLER-MATCH-ADDRESS-TOTAL     for any ret_addr and any
//                                    expected_address, the
//                                    matcher returns -1, 0, or 1.
//
//   P-CALLER-MATCH-SYMBOL-TOTAL      same for symbol matcher.
//
//   P-CALLER-MATCH-MODULE-OFFSET-TOTAL
//                                    same for module_offset matcher.
//
//   P-CALLER-MATCH-ADDRESS-DETERMINISTIC
//                                    two calls with same inputs
//                                    return the same result.
//
//   P-CALLER-MATCH-KIND-STRING-ROUNDTRIP
//                                    for any valid kind string,
//                                    from_string(kind) is idempotent.

#include "property_harness.h"
#include "caller_match.h"
#include "real_impls.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern struct RetraceRealImpls retrace_real_impls;

/* A local static function whose address is a stable, dladdr-able
 * pointer in the test binary.
 */
static int prop_anchor_fn(int x)
{
	return x + 1;
}

static int prop_caller_match_address_total(uint64_t seed)
{
	struct ret_prng prng;
	unsigned long long expected;
	int marker;
	int rc;

	ret_prng_seed(&prng, seed);
	expected = ret_prng_next(&prng);

	rc = retrace_caller_match_address(&marker, expected);

	return rc == -1 || rc == 0 || rc == 1;
}

static int prop_caller_match_symbol_total(uint64_t seed)
{
	struct ret_prng prng;
	char spec[64];
	size_t i;
	size_t len;
	int rc;
	static const char alphabet[] =
		"abcdefghijklmnopqrstuvwxyz_ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

	ret_prng_seed(&prng, seed);
	len = 1 + ret_prng_u32(&prng, (uint32_t)(sizeof(spec) - 2));
	for (i = 0; i < len; i++)
		spec[i] = alphabet[ret_prng_u32(&prng,
			(uint32_t)sizeof(alphabet) - 1)];
	spec[len] = '\0';

	rc = retrace_caller_match_symbol((void *)prop_anchor_fn, spec);

	return rc == -1 || rc == 0 || rc == 1;
}

static int prop_caller_match_module_offset_total(uint64_t seed)
{
	struct ret_prng prng;
	char mod[64];
	unsigned long long offset;
	size_t i;
	size_t len;
	int rc;
	static const char alphabet[] =
		"abcdefghijklmnopqrstuvwxyz.so_0123456789";

	ret_prng_seed(&prng, seed);
	len = 1 + ret_prng_u32(&prng, (uint32_t)(sizeof(mod) - 2));
	for (i = 0; i < len; i++)
		mod[i] = alphabet[ret_prng_u32(&prng,
			(uint32_t)sizeof(alphabet) - 1)];
	mod[len] = '\0';

	offset = ret_prng_next(&prng);

	rc = retrace_caller_match_module_offset((void *)prop_anchor_fn,
		mod, offset);

	return rc == -1 || rc == 0 || rc == 1;
}

static int prop_caller_match_address_deterministic(uint64_t seed)
{
	struct ret_prng prng;
	unsigned long long expected;
	int marker;
	int r1, r2;

	ret_prng_seed(&prng, seed);
	expected = ret_prng_next(&prng);

	r1 = retrace_caller_match_address(&marker, expected);
	r2 = retrace_caller_match_address(&marker, expected);

	return r1 == r2;
}

static int prop_caller_match_kind_string_roundtrip(uint64_t seed)
{
	static const char *known_kinds[] = {
		"address", "symbol", "offset_in_module"
	};
	struct ret_prng prng;
	const char *kind;
	enum retrace_caller_match_kind k;

	ret_prng_seed(&prng, seed);
	kind = known_kinds[ret_prng_u32(&prng, 3)];

	k = retrace_caller_match_kind_from_string(kind);

	/* Idempotent: parse the same string twice. */
	return k == retrace_caller_match_kind_from_string(kind) &&
		k != RETRACE_CALLER_MATCH_UNKNOWN;
}

int main(void)
{
	int failures = 0;

	retrace_real_impls.strcmp = strcmp;
	retrace_real_impls.strlen = strlen;
	retrace_real_impls.strcpy = strcpy;
	retrace_real_impls.memset = memset;
	retrace_real_impls.memcpy = memcpy;
	retrace_real_impls.malloc = malloc;
	retrace_real_impls.free = free;
	retrace_real_impls.real_snprintf = snprintf;

	/* Touch the anchor so the linker keeps it. */
	(void)prop_anchor_fn(0);

	printf("caller_match property tests (TODO 17 P14):\n");

	failures += property_run(prop_caller_match_address_total,
		"caller_match_address_total",
		RETRACE_PROPERTY_DEFAULT_ITERS, 300);
	failures += property_run(prop_caller_match_symbol_total,
		"caller_match_symbol_total",
		RETRACE_PROPERTY_DEFAULT_ITERS, 300);
	failures += property_run(prop_caller_match_module_offset_total,
		"caller_match_module_offset_total",
		RETRACE_PROPERTY_DEFAULT_ITERS, 300);
	failures += property_run(prop_caller_match_address_deterministic,
		"caller_match_address_deterministic",
		RETRACE_PROPERTY_DEFAULT_ITERS, 300);
	failures += property_run(prop_caller_match_kind_string_roundtrip,
		"caller_match_kind_string_roundtrip",
		RETRACE_PROPERTY_DEFAULT_ITERS, 300);

	if (failures == 0)
		printf("\n[property] all caller_match properties PASS\n");
	else
		printf("\n[property] %d caller_match properties FAILED\n",
			failures);

	return failures ? 1 : 0;
}
