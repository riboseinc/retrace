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

retrace_func_define_prototypes(dirent) = {
	{
		.name = "opendir",
		.conv = CC_SYSTEM_V,
		.type_name = "ptr",
		.params_cnt = 1,
		.params = {
			{
				.name = "name",
				.type_name = "ptr",
				.modifiers = CDM_POINTER | CDM_CONST,
				.ref_type_name = "sz",
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "fdopendir",
		.conv = CC_SYSTEM_V,
		.type_name = "ptr",
		.params_cnt = 1,
		.params = {
			{
				.name = "fd",
				.type_name = "int",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "readdir",
		.conv = CC_SYSTEM_V,
		.type_name = "ptr",
		.params_cnt = 1,
		.params = {
			{
				.name = "dirp",
				.type_name = "ptr",
				.modifiers = CDM_POINTER,
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "telldir",
		.conv = CC_SYSTEM_V,
		.type_name = "long",
		.params_cnt = 1,
		.params = {
			{
				.name = "dirp",
				.type_name = "ptr",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "seekdir",
		.conv = CC_SYSTEM_V,
		.type_name = "long",
		.params_cnt = 2,
		.params = {
			{
				.name = "dirp",
				.type_name = "ptr",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "loc",
				.type_name = "long",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN

			}
		}
	},
	{
		.name = "rewinddir",
		.conv = CC_SYSTEM_V,
		.type_name = "void",
		.params_cnt = 1,
		.params = {
			{
				.name = "dirp",
				.type_name = "ptr",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			}
		}
	}
};
