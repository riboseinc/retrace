/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "script_resolver.h"

#include "real_impls.h"
#include "logger.h"
#include "caller_match.h"
#include "config_cache.h"

/* Evaluate one caller_matches entry against ret_addr.
 * Returns 1 on match, 0 on no-match, -1 on hard failure.
 */
static int eval_caller_match_entry(void *ret_addr,
				   const JSON_Object *entry)
{
	const char *match_type;
	const char *str_value;
	double num_value;
	const char *module;
	enum retrace_caller_match_kind kind;

	if (entry == NULL)
		return -1;

	match_type = json_object_get_string(entry, "match_type");
	kind = retrace_caller_match_kind_from_string(match_type);

	switch (kind) {
	case RETRACE_CALLER_MATCH_ADDRESS:
		num_value = json_object_get_number(entry, "value");
		return retrace_caller_match_address(ret_addr,
			(unsigned long long)num_value);

	case RETRACE_CALLER_MATCH_SYMBOL:
		str_value = json_object_get_string(entry, "value");
		return retrace_caller_match_symbol(ret_addr, str_value);

	case RETRACE_CALLER_MATCH_MODULE_OFFSET:
		module = json_object_get_string(entry, "module");
		num_value = json_object_get_number(entry, "offset");
		return retrace_caller_match_module_offset(ret_addr, module,
			(unsigned long long)num_value);

	case RETRACE_CALLER_MATCH_UNKNOWN:
	default:
		log_warn("caller_match: unknown match_type '%s'",
			match_type ? match_type : "(null)");
		return -1;
	}
}

/* Evaluate the caller_matches array (OR-semantics: any match wins).
 * Returns 1 if any entry matches, 0 if none match, -1 if no
 * caller_matches array is present or all entries failed dladdr.
 */
static int eval_caller_matches(void *ret_addr,
			       const JSON_Object *i_script)
{
	JSON_Array *matches;
	size_t i, n;
	int any_evaluated = 0;

	matches = json_object_get_array(i_script, "caller_matches");
	if (matches == NULL)
		return -1;

	n = json_array_get_count(matches);
	for (i = 0; i < n; i++) {
		const JSON_Object *entry = json_array_get_object(matches, i);
		int rc = eval_caller_match_entry(ret_addr, entry);

		if (rc == 1)
			return 1;
		if (rc != -1)
			any_evaluated = 1;
	}

	return any_evaluated ? 0 : -1;
}

const JSON_Object *retrace_script_find(const JSON_Array *i_array,
	const char *func_name,
	void *ret_addr)
{
	size_t i;
	const JSON_Object *i_script;
	const char *i_func;
	double i_ret_addr;
	const JSON_Object *ret_cand = NULL;

	/* Fast path: check the config cache for an exact-name match
	 * that has no caller_matches and no return_addr constraints.
	 * If found, return immediately -- this is the common case
	 * (most scripts are simple name + actions, no call-site
	 * filtering).
	 */
	{
		const JSON_Object *cached =
			retrace_config_cache_lookup(func_name);

		if (cached != NULL) {
			JSON_Array *cm = json_object_get_array(cached,
				"caller_matches");
			double ra = json_object_get_number(cached,
				"return_addr");

			if (cm == NULL && ra == 0.0)
				return cached;
		}
	}

	for (i = 0; i < json_array_get_count(i_array); i++) {
		i_script = json_array_get_object(i_array, i);

		i_func = json_object_get_string(i_script, "func_name");
		if (i_func == NULL) {
			log_err(
				"i_script idx: %d has no func_name member",
				i);
			continue;
		}

		/* check for match-all */
		if (!retrace_real_impls.strcmp(i_func, "*"))
			return i_script;

		/* func_name match? */
		if (!retrace_real_impls.strcmp(i_func, func_name)) {
			int caller_matches_rc;

			/* caller_matches array (TODO.complete/17) --
			 * any-match-wins semantics. If the array is
			 * absent, fall through to the legacy
			 * return_addr single-value handling.
			 */
			caller_matches_rc = eval_caller_matches(ret_addr,
				i_script);
			if (caller_matches_rc == 1)
				return i_script;
			if (caller_matches_rc == 0)
				continue;  /* array present, no match */

			/* caller_matches_rc == -1: array absent OR all
			 * entries failed dladdr. Fall through to
			 * legacy return_addr handling.
			 */

			if (!ret_addr)
				return i_script;

			i_ret_addr = json_object_get_number(i_script,
				"return_addr");

			if (!i_ret_addr) {
				/* ret_addr not specified */
				if (ret_cand == NULL)
					ret_cand = i_script;
			} else {
				/* ret_addr match? */
				if ((long long)ret_addr ==
					((long long)i_ret_addr))
					return i_script;
			}
		}
	}

	return ret_cand;
}
