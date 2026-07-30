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

const JSON_Object *retrace_script_find(const JSON_Array *i_array,
	const char *func_name,
	void *ret_addr)
{
	size_t i;
	const JSON_Object *i_script;
	const char *i_func;
	double i_ret_addr;
	const JSON_Object *ret_cand = NULL;

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
