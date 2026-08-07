/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * libFuzzer harness for the script_resolver surface
 * (TODO.complete/33 P0).
 *
 * Feeds arbitrary bytes to parson to construct a JSON array,
 * then calls retrace_script_find with a fixed func_name + NULL
 * ret_addr. The contract: either parson rejects the input
 * (NULL) and we skip, or parson returns a valid JSON_Value
 * and the resolver doesn't crash.
 *
 * Catches: malformed JSON arrays, missing func_name fields,
 * huge/deeply-nested values, integer-overflow return_addr, etc.
 *
 * Build (clang-only) via CMake:
 *   CC=clang cmake -B build-fuzz -DRETRACE_BUILD_FUZZERS=ON ...
 *   cmake --build build-fuzz --target fuzz_script_resolve
 *
 * Run:
 *   ./build-fuzz/test/fuzz/fuzz_script_resolve \
 *     -max_total_time=60 test/fuzz/corpus/script_resolve/
 */

#include "parson.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern const JSON_Object *retrace_script_find(const JSON_Array *i_array,
	const char *func_name,
	void *ret_addr);

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	char *buf = malloc(size + 1);

	if (buf == NULL)
		return 0;

	memcpy(buf, data, size);
	buf[size] = '\0';

	{
		JSON_Value *v = json_parse_string_with_comments(buf);

		if (v != NULL) {
			if (json_value_get_type(v) == JSONArray) {
				JSON_Array *arr = json_array(v);

				(void)retrace_script_find(arr,
					"fuzz_func", NULL);
			}
			json_value_free(v);
		}
	}

	free(buf);
	return 0;
}
