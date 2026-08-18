/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Public config validation (ADR-0014 implementation-first).
 *
 * The internal loader (src/config/json/conf.c) parses the config
 * but performs no semantic checks -- a typo'd action_name or
 * func_name surfaces at runtime as a silently-missing
 * interception. This entry point catches both classes up front:
 * parse the buffer (comment-tolerant, same as the loader), then
 * check every func_name against the prototype registry (the
 * literal "*" wildcard is allowed) and every action_name against
 * the action registry.
 *
 * Uses libc directly (snprintf/strcmp), matching the other
 * public_api files: the public surface must not depend on the
 * reentrancy-guarded real_impls table being resolved.
 */

#include <retrace/retrace.h>

#include "parson.h"
#include "funcs.h"
#include "actions.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

__attribute__((format(printf, 3, 4)))
static void set_err(char *err_buf, size_t err_len, const char *fmt,
		    ...)
{
	va_list ap;

	if (err_buf == NULL || err_len == 0)
		return;
	va_start(ap, fmt);
	vsnprintf(err_buf, err_len, fmt, ap);
	va_end(ap);
}

RETRACE_API retrace_status_t retrace_config_validate_buffer(
		const char *buf, size_t len, char *err_buf, size_t err_len)
{
	JSON_Value *root;
	JSON_Object *obj;
	JSON_Array *scripts;
	size_t i, n;

	if (err_buf != NULL && err_len > 0)
		err_buf[0] = '\0';
	if (buf == NULL || len == 0)
		return RETRACE_ERR_INVAL;

	/* The parser is NUL-terminated-string based. Accept the two
	 * well-formed spellings: a NUL within len, or len == strlen
	 * (terminator exactly at buf[len]). Reject anything else.
	 */
	if (memchr(buf, '\0', len) == NULL && buf[len] != '\0')
		return RETRACE_ERR_INVAL;

	root = json_parse_string_with_comments(buf);
	if (root == NULL) {
		set_err(err_buf, err_len, "malformed JSON");
		return RETRACE_ERR_FORMAT;
	}
	obj = json_value_get_object(root);
	if (obj == NULL) {
		set_err(err_buf, err_len,
			"config root is not a JSON object");
		json_value_free(root);
		return RETRACE_ERR_FORMAT;
	}

	scripts = json_object_get_array(obj, "intercept_scripts");
	if (scripts == NULL) {
		set_err(err_buf, err_len,
			"missing 'intercept_scripts' array");
		json_value_free(root);
		return RETRACE_ERR_FORMAT;
	}

	n = json_array_get_count(scripts);
	for (i = 0; i < n; i++) {
		JSON_Object *script = json_array_get_object(scripts, i);
		const char *func_name;
		JSON_Array *actions;
		size_t j, na;

		if (script == NULL) {
			set_err(err_buf, err_len,
				"intercept_scripts[%zu] is not an object", i);
			json_value_free(root);
			return RETRACE_ERR_FORMAT;
		}
		func_name = json_object_get_string(script, "func_name");
		if (func_name == NULL || *func_name == '\0') {
			set_err(err_buf, err_len,
				"intercept_scripts[%zu]: missing func_name",
				i);
			json_value_free(root);
			return RETRACE_ERR_FORMAT;
		}
		if (strcmp(func_name, "*") != 0 &&
		    retrace_func_get(func_name) == NULL) {
			set_err(err_buf, err_len,
				"unknown function '%s'", func_name);
			json_value_free(root);
			return RETRACE_ERR_FORMAT;
		}

		actions = json_object_get_array(script, "actions");
		if (actions == NULL) {
			set_err(err_buf, err_len,
				"func '%s': missing actions array",
				func_name);
			json_value_free(root);
			return RETRACE_ERR_FORMAT;
		}
		na = json_array_get_count(actions);
		for (j = 0; j < na; j++) {
			JSON_Object *action =
				json_array_get_object(actions, j);
			const char *action_name;

			if (action == NULL) {
				set_err(err_buf, err_len,
					"func '%s': actions[%zu] is not an object",
					func_name, j);
				json_value_free(root);
				return RETRACE_ERR_FORMAT;
			}
			action_name = json_object_get_string(action,
				"action_name");
			if (action_name == NULL || *action_name == '\0') {
				set_err(err_buf, err_len,
					"func '%s': actions[%zu]: missing action_name",
					func_name, j);
				json_value_free(root);
				return RETRACE_ERR_FORMAT;
			}
			if (retrace_actions_get(action_name) == NULL) {
				set_err(err_buf, err_len,
					"unknown action '%s' in func '%s'",
					action_name, func_name);
				json_value_free(root);
				return RETRACE_ERR_FORMAT;
			}
		}
	}

	json_value_free(root);
	return RETRACE_OK;
}
