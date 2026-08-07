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

/*
 * Caller-match primitives for per-return-address routing
 * (TODO.complete/17).
 *
 * Three match kinds, mirrors the JSON spec in TODO.complete/17:
 *
 *   address           exact ret_addr == expected
 *   symbol            dladdr(ret_addr).dli_sname == expected
 *   offset_in_module  ret_addr - dladdr(ret_addr).dli_fbase
 *                     == expected_offset AND module path matches
 *
 * dladdr is POSIX (Linux/macOS/BSDs). On Windows this header
 * would dispatch to SymFromAddr + GetModuleHandle; deferred to
 * TODO 6 (Windows trampoline port).
 *
 * Performance: dladdr is ~10us per call on macOS. The engine
 * calls the matcher only when a script has caller_matches, so
 * the default-config hot path (no caller_matches) is unaffected.
 * A future PR can add per-module caching.
 */

#ifndef RETRACE_CORE_CALLER_MATCH_H_
#define RETRACE_CORE_CALLER_MATCH_H_

#include <stddef.h>

/* Match kind enum mirrors the JSON "match_type" field. */
enum retrace_caller_match_kind {
	RETRACE_CALLER_MATCH_UNKNOWN = 0,
	RETRACE_CALLER_MATCH_ADDRESS = 1,
	RETRACE_CALLER_MATCH_SYMBOL = 2,
	RETRACE_CALLER_MATCH_MODULE_OFFSET = 3,
};

/*
 * All matchers return 1 on match, 0 on no-match, -1 on hard
 * failure (bad input, dladdr unavailable). The script_resolver
 * treats -1 as "no match" (conservative -- skip this script).
 */

/* Exact ret_addr == expected_address. */
int retrace_caller_match_address(void *ret_addr,
				 unsigned long long expected_address);

/* dladdr(ret_addr).dli_sname == expected_symbol. NULL ret_addr
 * or dladdr failure returns -1. Empty expected_symbol returns -1.
 */
int retrace_caller_match_symbol(void *ret_addr,
				const char *expected_symbol);

/* ret_addr - module_load_address == expected_offset AND module
 * path basename matches expected_module_basename.
 *
 * The basename comparison is intentional: full paths differ
 * across systems (/usr/lib vs /lib), but the basename (e.g.
 * "libfoo.so") is stable.
 */
int retrace_caller_match_module_offset(void *ret_addr,
				       const char *expected_module_basename,
				       unsigned long long expected_offset);

/*
 * Parse a JSON-style match_type string into the enum. Returns
 * UNKNOWN for unrecognized values (caller treats as no-match).
 */
enum retrace_caller_match_kind retrace_caller_match_kind_from_string(
	const char *s);

#endif /* RETRACE_CORE_CALLER_MATCH_H_ */
