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
 * time.h prototypes. Each entry mirrors a WRAPPER_ENTRY_SYSTEM_V line
 * in src/v2/funcs_symbols.S.
 *
 * Conventions:
 *
 *   - time_t is "long" everywhere (glibc, musl, BSD, Darwin all define
 *     it as an integer type large enough for epoch seconds).
 *   - struct tm is opaque (the engine doesn't model it). Use CDM_NOMOD
 *     for the pointer; log_params shows the address.
 *   - Output buffers (char buf[26] for ctime_r) use CDM_POINTER with
 *     ref_type_name = "sz" and PDIR_OUT so log_params shows the
 *     post-call string.
 */

#include "funcs.h"

retrace_func_define_prototypes(time) = {
	{
		.name = "time",
		.conv = CC_SYSTEM_V,
		.type_name = "long",
		.params_cnt = 1,
		.params = {
			{
				.name = "tloc",
				.type_name = "ptr",
				.modifiers = CDM_POINTER,
				.ref_type_name = "long",
				.direction = PDIR_OUT
			}
		}
	},
	{
		.name = "ctime_r",
		.conv = CC_SYSTEM_V,
		.type_name = "ptr",
		.params_cnt = 2,
		.params = {
			{
				.name = "time",
				.type_name = "ptr",
				.modifiers = CDM_POINTER | CDM_CONST,
				.ref_type_name = "long",
				.direction = PDIR_IN
			},
			{
				.name = "buf",
				.type_name = "ptr",
				.modifiers = CDM_POINTER,
				.ref_type_name = "sz",
				.direction = PDIR_OUT
			}
		}
	},
	{
		.name = "localtime_r",
		.conv = CC_SYSTEM_V,
		.type_name = "ptr",
		.params_cnt = 2,
		.params = {
			{
				.name = "timep",
				.type_name = "ptr",
				.modifiers = CDM_POINTER | CDM_CONST,
				.ref_type_name = "long",
				.direction = PDIR_IN
			},
			{
				/*
				 * struct tm * -- opaque to retrace. Show the address;
				 * don't try to deref (no struct member model).
				 */
				.name = "result",
				.type_name = "ptr",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_OUT
			}
		}
	}
};
