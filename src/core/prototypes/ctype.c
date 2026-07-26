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

#include "funcs.h"

retrace_func_define_prototypes(ctype) = {
	{
		.name = "isalnum",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 1,
		.params = {
			{
				.name = "c",
				.type_name = "int",
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "isalpha",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 1,
		.params = {
			{
				.name = "c",
				.type_name = "int",
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "isblank",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 1,
		.params = {
			{
				.name = "c",
				.type_name = "int",
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "iscntrl",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 1,
		.params = {
			{
				.name = "c",
				.type_name = "int",
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "isdigit",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 1,
		.params = {
			{
				.name = "c",
				.type_name = "int",
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "isgraph",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 1,
		.params = {
			{
				.name = "c",
				.type_name = "int",
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "islower",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 1,
		.params = {
			{
				.name = "c",
				.type_name = "int",
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "isprint",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 1,
		.params = {
			{
				.name = "c",
				.type_name = "int",
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "ispunct",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 1,
		.params = {
			{
				.name = "c",
				.type_name = "int",
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "isspace",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 1,
		.params = {
			{
				.name = "c",
				.type_name = "int",
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "isupper",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 1,
		.params = {
			{
				.name = "c",
				.type_name = "int",
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "isxdigit",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 1,
		.params = {
			{
				.name = "c",
				.type_name = "int",
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "toupper",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 1,
		.params = {
			{
				.name = "c",
				.type_name = "int",
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "tolower",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 1,
		.params = {
			{
				.name = "c",
				.type_name = "int",
				.direction = PDIR_IN
			}
		}
	}
};

