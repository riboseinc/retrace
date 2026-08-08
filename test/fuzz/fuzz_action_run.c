/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * libFuzzer harness for the action dispatch surface
 * (TODO.complete/33 P0).
 *
 * Feeds arbitrary bytes to parson to construct a JSON action
 * entry (action_name + action_params), looks up the action,
 * and calls it with a constructed ThreadContext. The contract:
 * never crash for any input.
 *
 * Catches: unknown action names, malformed action_params,
 * integer overflow values, missing required fields, etc.
 *
 * Build via CMake with -DRETRACE_BUILD_FUZZERS=ON.
 */

#include "parson.h"
#include "engine.h"
#include "actions.h"
#include "funcs.h"
#include "data_types.h"
#include "real_impls.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Stub real_impl for call_real -- returns 0 without crashing.
 * In the real engine, real_impl is always non-NULL before
 * actions run (engine.c checks). The fuzzer bypasses the engine,
 * so we provide a safe default.
 */
static long fuzz_stub_real(void) { return 0; }

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	char *buf;
	JSON_Value *v;

	buf = (char *)malloc(size + 1);
	if (buf == NULL)
		return 0;

	memcpy(buf, data, size);
	buf[size] = '\0';

	v = json_parse_string_with_comments(buf);
	free(buf);

	if (v != NULL && json_value_get_type(v) == JSONObject) {
		JSON_Object *root = json_value_get_object(v);
		const char *action_name;
		const JSON_Object *action_params;
		static struct ThreadContext ctx;
		static struct FuncPrototype proto;
		static int initialized;

		if (!initialized) {
			initialized = 1;
			retrace_actions_init();
			retrace_datatypes_init();
		}

		action_name = json_object_get_string(root, "action_name");
		action_params = json_object_get_object(root, "action_params");

		if (action_name != NULL) {
			int (*action)(struct ThreadContext *t_ctx,
				      const JSON_Object *params) =
				retrace_actions_get(action_name);

			if (action != NULL) {
				memset(&ctx, 0, sizeof(ctx));
				memset(&proto, 0, sizeof(proto));
				strncpy(proto.name, "fuzz",
					sizeof(proto.name) - 1);
				ctx.prototype = &proto;
				ctx.params_cnt = 0;
				ctx.real_impl = (void *)fuzz_stub_real;

				(void)action(&ctx, action_params);
			}
		}
	}

	json_value_free(v);
	return 0;
}
