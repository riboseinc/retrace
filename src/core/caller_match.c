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

#include "caller_match.h"

#include <dlfcn.h>
#include <string.h>

#include "real_impls.h"
#include "logger.h"

/* Find the basename (chars after last '/') of a path. Returns
 * the input pointer if no '/' present.
 */
static const char *path_basename(const char *path)
{
	const char *slash;

	if (path == NULL)
		return NULL;

	slash = NULL;
	{
		const char *p;

		for (p = path; *p != '\0'; p++)
			if (*p == '/')
				slash = p;
	}

	return slash ? slash + 1 : path;
}

int retrace_caller_match_address(void *ret_addr,
				 unsigned long long expected_address)
{
	if (ret_addr == NULL)
		return -1;

	return ((unsigned long long)(unsigned long)ret_addr) ==
	       expected_address;
}

int retrace_caller_match_symbol(void *ret_addr,
				const char *expected_symbol)
{
	Dl_info info = {0};

	if (ret_addr == NULL || expected_symbol == NULL ||
	    expected_symbol[0] == '\0')
		return -1;

	/* dladdr is in libc on POSIX. */
	if (dladdr(ret_addr, &info) == 0) {
		log_dbg("caller_match_symbol: dladdr failed for %p",
			ret_addr);
		return -1;
	}

	if (info.dli_sname == NULL || info.dli_sname[0] == '\0')
		return 0;

	return retrace_real_impls.strcmp(info.dli_sname,
		       expected_symbol) == 0;
}

int retrace_caller_match_module_offset(void *ret_addr,
				       const char *expected_module_basename,
				       unsigned long long expected_offset)
{
	Dl_info info = {0};
	const char *actual_base;
	unsigned long long actual_offset;

	if (ret_addr == NULL || expected_module_basename == NULL ||
	    expected_module_basename[0] == '\0')
		return -1;

	if (dladdr(ret_addr, &info) == 0) {
		log_dbg("caller_match_module_offset: dladdr failed for %p",
			ret_addr);
		return -1;
	}

	if (info.dli_fname == NULL || info.dli_fbase == NULL)
		return 0;

	actual_base = path_basename(info.dli_fname);
	if (actual_base == NULL)
		return 0;

	if (retrace_real_impls.strcmp(actual_base,
		    expected_module_basename) != 0)
		return 0;

	actual_offset = (unsigned long long)((char *)ret_addr -
					     (char *)info.dli_fbase);

	return actual_offset == expected_offset;
}

enum retrace_caller_match_kind retrace_caller_match_kind_from_string(
	const char *s)
{
	if (s == NULL)
		return RETRACE_CALLER_MATCH_UNKNOWN;

	if (retrace_real_impls.strcmp(s, "address") == 0)
		return RETRACE_CALLER_MATCH_ADDRESS;
	if (retrace_real_impls.strcmp(s, "symbol") == 0)
		return RETRACE_CALLER_MATCH_SYMBOL;
	if (retrace_real_impls.strcmp(s, "offset_in_module") == 0)
		return RETRACE_CALLER_MATCH_MODULE_OFFSET;

	return RETRACE_CALLER_MATCH_UNKNOWN;
}
