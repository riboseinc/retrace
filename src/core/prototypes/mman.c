/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * mmap/munmap prototypes. The repo's oldest open PR (#414,
 * pablo-mendoza, Feb 2019) asked for exactly this pair; its
 * v1-era shape went stale through the rewrite, the intent
 * never shipped -- until now. mmap is where allocators get
 * their pages: intercepting it puts fault injection at the
 * MEMORY level (fail_first on mmap is deterministic OOM at
 * the page; memory_fuzz reaches below malloc; log_params
 * shows the anonymous-mapping churn a detonation does).
 */

#include "funcs.h"

retrace_func_define_prototypes(mman) = {
	{
		.name = "mmap",
		.conv = CC_SYSTEM_V,
		.type_name = "ptr",
		.params_cnt = 6,
		.params = {
			{
				.name = "addr",
				.type_name = "ptr",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "length",
				.type_name = "size_t",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "prot",
				.type_name = "int",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "flags",
				.type_name = "int",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "fd",
				.type_name = "int",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "offset",
				.type_name = "off_t",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "munmap",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 2,
		.params = {
			{
				.name = "addr",
				.type_name = "ptr",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "length",
				.type_name = "size_t",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			}
		}
	}
};
