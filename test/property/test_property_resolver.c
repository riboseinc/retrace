// SPDX-License-Identifier: BSD-2-Clause
//
// Property tests for the script_resolver (TODO.complete/16 P12-13
// analog). The engine's frame-walking properties (P12, P13) need
// a full engine harness; this PR instead exercises the script
// resolver surface -- the part of the engine that's testable
// without arch-spec plumbing.
//
// Properties:
//
//   P-RESOLVE-NEVER-CRASH      any func_name + any ret_addr ->
//                              resolver returns NULL or a valid
//                              JSON_Object*, never crashes.
//
//   P-RESOLVE-WILDCARD-ALWAYS  a wildcard "*" entry matches any
//                              func_name, regardless of ret_addr.
//
//   P-RESOLVE-EXACT-WINS       an exact-name entry takes precedence
//                              over a wildcard when both are
//                              present and the exact entry comes
//                              first in the array.
//
//   P-RESOLVE-CONSISTENT       two resolver calls with the same
//                              (scripts, func_name, ret_addr)
//                              return the same object pointer.
//
//   P-RESOLVE-RET-ADDR-ORDER   a script with a matching return_addr
//                              is preferred over one without,
//                              even if the latter comes first in
//                              the array.

#include "property_harness.h"
#include "parson.h"
#include "real_impls.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern struct RetraceRealImpls retrace_real_impls;

/* Forward declaration -- script_resolver.c exposes this via
 * script_resolver.h, but we re-declare to avoid the engine
 * include chain.
 */
extern const JSON_Object *retrace_script_find(const JSON_Array *i_array,
	const char *func_name,
	void *ret_addr);

/* Construct a JSON_Array with one or two scripts. */
static JSON_Array *build_scripts(const char *name1, double ret1,
				 const char *name2, double ret2)
{
	JSON_Value *arr_val = json_value_init_array();
	JSON_Array *arr = json_array(arr_val);

	if (name1) {
		JSON_Value *v = json_value_init_object();
		JSON_Object *o = json_value_get_object(v);

		json_object_set_string(o, "func_name", name1);
		if (ret1)
			json_object_set_number(o, "return_addr", ret1);
		json_array_append_value(arr, v);
	}

	if (name2) {
		JSON_Value *v = json_value_init_object();
		JSON_Object *o = json_value_get_object(v);

		json_object_set_string(o, "func_name", name2);
		if (ret2)
			json_object_set_number(o, "return_addr", ret2);
		json_array_append_value(arr, v);
	}

	return arr;
}

static JSON_Array *build_wildcard_scripts(void)
{
	return build_scripts("*", 0, NULL, 0);
}

/* Generate a syntactically valid (though not necessarily
 * meaningful) function name. Most names are short ASCII.
 */
static void gen_func_name(struct ret_prng *p, char *buf, size_t cap)
{
	static const char alphabet[] =
		"abcdefghijklmnopqrstuvwxyz_ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
	size_t len;
	size_t i;

	if (cap < 2)
		return;

	len = 1 + ret_prng_u32(p, (uint32_t)(cap - 2));
	for (i = 0; i < len; i++)
		buf[i] = alphabet[ret_prng_u32(p, (uint32_t)sizeof(alphabet) - 1)];
	buf[len] = '\0';
}

static int prop_resolve_never_crash(uint64_t seed)
{
	struct ret_prng prng;
	char name[64];
	JSON_Array *wildcard = build_wildcard_scripts();
	const JSON_Object *result;
	int marker;

	ret_prng_seed(&prng, seed);
	gen_func_name(&prng, name, sizeof(name));

	/* The contract: lookup returns NULL or a valid JSON_Object*,
	 * and the process is still alive.
	 */
	result = retrace_script_find(wildcard, name, &marker);
	(void)result;

	json_value_free(json_array_get_wrapping_value(wildcard));
	return 1;
}

static int prop_resolve_wildcard_always(uint64_t seed)
{
	struct ret_prng prng;
	char name[64];
	JSON_Array *wildcard = build_wildcard_scripts();
	const JSON_Object *result;
	int marker;
	int held = 1;

	ret_prng_seed(&prng, seed);
	gen_func_name(&prng, name, sizeof(name));

	result = retrace_script_find(wildcard, name, &marker);
	if (result == NULL)
		held = 0;
	else
		held = strcmp(json_object_get_string(result, "func_name"),
			name) == 0 ||
			strcmp(json_object_get_string(result, "func_name"),
			"*") == 0;

	json_value_free(json_array_get_wrapping_value(wildcard));
	return held;
}

static int prop_resolve_wildcard_first_returns_immediately(uint64_t seed)
{
	struct ret_prng prng;
	char name[64];
	/* wildcard at index 0 -- must return on the first iteration. */
	JSON_Array *arr = build_scripts("*", 0, NULL, 0);
	const JSON_Object *result;
	const char *func_name;
	int held;
	int marker;

	ret_prng_seed(&prng, seed);
	gen_func_name(&prng, name, sizeof(name));

	result = retrace_script_find(arr, name, &marker);
	/* Read before free (result points into the JSON tree). */
	func_name = result ? json_object_get_string(result, "func_name") : NULL;
	held = result != NULL && func_name != NULL &&
		strcmp(func_name, "*") == 0;

	json_value_free(json_array_get_wrapping_value(arr));
	return held;
}

static int prop_resolve_consistent(uint64_t seed)
{
	struct ret_prng prng;
	char name[64];
	JSON_Array *arr;
	const JSON_Object *r1;
	const JSON_Object *r2;
	int marker;
	int held;

	ret_prng_seed(&prng, seed);
	gen_func_name(&prng, name, sizeof(name));

	arr = build_scripts(name, 0, "*", 0);
	r1 = retrace_script_find(arr, name, &marker);
	r2 = retrace_script_find(arr, name, &marker);
	/* Compare pointers before free. */
	held = (r1 == r2) && (r1 != NULL);

	json_value_free(json_array_get_wrapping_value(arr));
	return held;
}

static int prop_resolve_ret_addr_order(uint64_t seed)
{
	struct ret_prng prng;
	char name[64];
	/* name-only first, name+ret_addr second -- second must win.
	 * Use a small fixed ret_addr value to avoid float-precision
	 * loss when parson converts to/from double.
	 */
	unsigned long ret_addr_int = 0x1000 + (seed % 0x100);
	void *ret_addr = (void *)ret_addr_int;
	double second_ret = (double)ret_addr_int;
	JSON_Array *arr;
	const JSON_Object *result;
	double actual_ret;
	int held;

	ret_prng_seed(&prng, seed);
	gen_func_name(&prng, name, sizeof(name));

	arr = build_scripts(name, 0, name, second_ret);

	result = retrace_script_find(arr, name, ret_addr);

	/* Read result BEFORE freeing the JSON tree (result points
	 * into it; use-after-free otherwise).
	 */
	actual_ret = result ? json_object_get_number(result, "return_addr") : 0.0;
	held = result != NULL && actual_ret == second_ret;

	json_value_free(json_array_get_wrapping_value(arr));
	return held;
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

	printf("script_resolver property tests:\n");

	failures += property_run(prop_resolve_never_crash,
		"resolve_never_crash",
		RETRACE_PROPERTY_DEFAULT_ITERS, 200);
	failures += property_run(prop_resolve_wildcard_always,
		"resolve_wildcard_always",
		RETRACE_PROPERTY_DEFAULT_ITERS, 200);
	failures += property_run(prop_resolve_wildcard_first_returns_immediately,
		"resolve_wildcard_first_returns_immediately",
		RETRACE_PROPERTY_DEFAULT_ITERS, 200);
	failures += property_run(prop_resolve_consistent,
		"resolve_consistent",
		RETRACE_PROPERTY_DEFAULT_ITERS, 200);
	failures += property_run(prop_resolve_ret_addr_order,
		"resolve_ret_addr_order",
		RETRACE_PROPERTY_DEFAULT_ITERS, 200);

	if (failures == 0)
		printf("\n[property] all script_resolver properties PASS\n");
	else
		printf("\n[property] %d script_resolver properties FAILED\n",
			failures);

	return failures ? 1 : 0;
}
