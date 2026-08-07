/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * libFuzzer harness for the JSON config parser surface
 * (TODO.complete/33 P0).
 *
 * Feeds arbitrary bytes to parson's comment-tolerant parser
 * (the same one retrace_conf_init uses for RETRACE_JSON_CONFIG
 * files). The contract: parse either succeeds (returns a valid
 * JSON_Value*) or fails (returns NULL). It never crashes.
 *
 * Build:
 *   cc -fsanitize=fuzzer,address -I src/config/json \
 *     test/fuzz/fuzz_config_parse.c src/config/json/parson.c \
 *     -o fuzz_config_parse
 *
 * Run:
 *   ./fuzz_config_parse -max_total_time=300
 *
 * The seed corpus lives at test/fuzz/corpus/config_parse/. Each
 * file is a starter input; libFuzzer mutates from there.
 */

#include "parson.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	/* parson expects a NUL-terminated string. Copy to a buffer
	 * with room for the terminator.
	 */
	char *buf = malloc(size + 1);

	if (buf == NULL)
		return 0;

	memcpy(buf, data, size);
	buf[size] = '\0';

	/* Comment-tolerant parse is what retrace_conf_init uses. */
	{
		JSON_Value *v = json_parse_string_with_comments(buf);

		/* Free is a no-op on NULL per parson docs. */
		json_value_free(v);
	}

	free(buf);
	return 0;
}
