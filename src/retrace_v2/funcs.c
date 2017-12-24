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

/**
 * @file func_and_params.h
 * @author Ilia Kolominsky
 * @date 12 Dec 2017
 * @brief File containing metadata for the known parameters and functions.
 *
 * This metadata will be used by the retrace engine.
 * In future, we want to replace this hardcoded data with the automatically
 * generated by parsing the original C source files at runtime phase
 *
 */

#include "funcs.h"

/* This is future will come from parsing of H files or JSON config
 * and shall be modifiable at runtime
 */
const struct FuncPrototype retrace_funcs[] = {
	{
		.name = "getenv",
		.conv = CC_SYSTEM_V,
		.params = {
			{
				.name = "name",
				.type_name = "ptr",
				.modifiers = CDM_POINTER | CDM_CONST,
				.ref_type_name = "sz"
			},
			{
				.name = "",
			}
		}
	},
	{
		.name = "write",
		.conv = CC_SYSTEM_V,
		.params = {
			{
				.name = "fd",
				.type_name = "int",
				.modifiers = CDM_NOMOD
			},
			{
				.name = "buff",
				.type_name = "ptr",
				.modifiers =
					CDM_POINTER | CDM_CONST | CDM_ARRAY,
				.array_cnt_param = "count",
				.ref_type_name = "char"
			},
			{
				.name = "count",
				.type_name = "size_t",
				.modifiers = CDM_NOMOD,
			},
			{
				.name = ""
			}
		}
	},
	{
		.name = "writev",
		.conv = CC_SYSTEM_V,
		.params = {
			{
				.name = "fd",
				.type_name = "int",
				.modifiers = CDM_NOMOD
			},
			{
				.name = "iovec",
				.type_name = "ptr",
				.modifiers =
					CDM_POINTER | CDM_CONST | CDM_ARRAY,
				.array_cnt_param = "iovcnt",
				.ref_type_name = "iovec"
			},
			{
				.name = "iovcnt",
				.type_name = "int",
				.modifiers = CDM_NOMOD
			},
			{
				.name = ""
			}
		}
	},
	{
		.name = ""
	}
};

