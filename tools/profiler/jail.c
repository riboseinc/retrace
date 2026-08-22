/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Jail emission (TODO.trace-profile/10), extracted from the CLI:
 * the config shape lives with the profile model, not with the
 * argument parser. Consumed by capture --jail-out, --libc
 * --jail-out, and the `jail` subcommand.
 */

#include "jail.h"

JSON_Value *prof_jail_config(const struct Profile *funcs_src,
			     const struct Profile *paths_src,
			     const struct ProfJailOpts *opts)
{
	JSON_Value *v = json_value_init_object();
	JSON_Object *root = json_value_get_object(v);
	JSON_Value *scripts = json_value_init_array();
	size_t i, f;

	for (f = 0; f < funcs_src->functions.count; f++) {
		JSON_Value *script = json_value_init_object();
		JSON_Object *script_o = json_value_get_object(script);
		JSON_Value *actions = json_value_init_array();
		JSON_Value *action = json_value_init_object();
		JSON_Object *action_o = json_value_get_object(action);
		JSON_Value *params = json_value_init_object();
		JSON_Object *params_o = json_value_get_object(params);
		JSON_Value *allow = json_value_init_array();

		for (i = 0; i < paths_src->accesses.count; i++)
			json_array_append_string(
				json_value_get_array(allow),
				paths_src->accesses.items[i].path);

		json_object_set_value(params_o, "allow_paths", allow);
		if (opts != NULL && opts->read_only) {
			JSON_Value *dc = json_value_init_array();

			json_array_append_string(
				json_value_get_array(dc), "write");
			json_object_set_value(params_o, "deny_classes",
				dc);
		}
		if (opts != NULL && opts->decoy_dir != NULL)
			json_object_set_string(params_o, "decoy_dir",
				opts->decoy_dir);
		json_object_set_string(action_o, "action_name",
			"sandbox");
		json_object_set_value(action_o, "action_params", params);
		json_array_append_value(json_value_get_array(actions),
			action);

		/* allowed paths must still reach the real call */
		{
			JSON_Value *cr = json_value_init_object();

			json_object_set_string(json_value_get_object(cr),
				"action_name", "call_real");
			json_array_append_value(
				json_value_get_array(actions), cr);
		}

		json_object_set_string(script_o, "func_name",
			funcs_src->functions.names[f]);
		json_object_set_value(script_o, "actions", actions);
		json_array_append_value(json_value_get_array(scripts),
			script);
	}
	/* clock pinning: time() mocked to a fixed epoch for
	 * deterministic reruns (drift oracles, diffable traces)
	 */
	if (opts != NULL && opts->pin_clock_set) {
		JSON_Value *script = json_value_init_object();
		JSON_Object *script_o = json_value_get_object(script);
		JSON_Value *actions = json_value_init_array();
		JSON_Value *mr = json_value_init_object();

		json_object_set_string(json_value_get_object(mr),
			"action_name", "modify_return_value_int");
		json_object_set_number(json_value_get_object(mr),
			"new_int", (double)opts->pin_clock);
		json_array_append_value(json_value_get_array(actions),
			mr);
		json_object_set_string(script_o, "func_name", "time");
		json_object_set_value(script_o, "actions", actions);
		json_array_append_value(json_value_get_array(scripts),
			script);
	}
	json_object_set_value(root, "intercept_scripts", scripts);
	return v;
}
