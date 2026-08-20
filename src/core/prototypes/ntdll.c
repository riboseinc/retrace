/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * ntdll-layer prototypes (TODO.windows/06). Windows-only; the
 * hooks are opt-in (RETRACE_WIN_NTDLL=1, see
 * win_common/hook_targets.c) because ntdll interposition is
 * what AV/EDR products watch.
 *
 * Path params use the "ntoa" decoder (OBJECT_ATTRIBUTES ->
 * ObjectName -> UNICODE_STRING); LdrLoadDll's module name uses
 * "ntus" (UNICODE_STRING). Both live in datatypes/nt_decode.c.
 */

#include "funcs.h"

retrace_func_define_prototypes(ntdll) = {
	{
		.name = "NtCreateFile",
		.conv = CC_MICROSOFT,
		.type_name = "int",
		.params_cnt = 11,
		.params = {
			{
				.name = "filehandle",
				.type_name = "ptr",
				.modifiers = CDM_POINTER,
				.ref_type_name = "void",
				.direction = PDIR_OUT
			},
			{
				.name = "desiredaccess",
				.type_name = "unsigned int",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "objectattributes",
				.type_name = "ptr",
				.modifiers = CDM_POINTER | CDM_CONST,
				.ref_type_name = "ntoa",
				.direction = PDIR_IN
			},
			{
				.name = "iostatusblock",
				.type_name = "ptr",
				.modifiers = CDM_POINTER,
				.ref_type_name = "void",
				.direction = PDIR_OUT
			},
			{
				.name = "allocationsize",
				.type_name = "ptr",
				.modifiers = CDM_POINTER | CDM_CONST,
				.ref_type_name = "void",
				.direction = PDIR_IN
			},
			{
				.name = "fileattributes",
				.type_name = "unsigned int",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "shareaccess",
				.type_name = "unsigned int",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "createdisposition",
				.type_name = "unsigned int",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "createoptions",
				.type_name = "unsigned int",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "eabuffer",
				.type_name = "ptr",
				.modifiers = CDM_POINTER | CDM_CONST,
				.ref_type_name = "void",
				.direction = PDIR_IN
			},
			{
				.name = "ealength",
				.type_name = "unsigned int",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			}
		},
		.fmt = FAT_NOVARARGS
	},
	{
		.name = "NtOpenFile",
		.conv = CC_MICROSOFT,
		.type_name = "int",
		.params_cnt = 6,
		.params = {
			{
				.name = "filehandle",
				.type_name = "ptr",
				.modifiers = CDM_POINTER,
				.ref_type_name = "void",
				.direction = PDIR_OUT
			},
			{
				.name = "desiredaccess",
				.type_name = "unsigned int",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "objectattributes",
				.type_name = "ptr",
				.modifiers = CDM_POINTER | CDM_CONST,
				.ref_type_name = "ntoa",
				.direction = PDIR_IN
			},
			{
				.name = "iostatusblock",
				.type_name = "ptr",
				.modifiers = CDM_POINTER,
				.ref_type_name = "void",
				.direction = PDIR_OUT
			},
			{
				.name = "shareaccess",
				.type_name = "unsigned int",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "openoptions",
				.type_name = "unsigned int",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			}
		},
		.fmt = FAT_NOVARARGS
	},
	{
		/*
		 * What GetFileAttributesW funnels into -- the DOMINANT
		 * importer syscall (libsass file_exists: ~14 probes per
		 * @import per directory; verified 2026-08-19).
		 */
		.name = "NtQueryAttributesFile",
		.conv = CC_MICROSOFT,
		.type_name = "int",
		.params_cnt = 2,
		.params = {
			{
				.name = "objectattributes",
				.type_name = "ptr",
				.modifiers = CDM_POINTER | CDM_CONST,
				.ref_type_name = "ntoa",
				.direction = PDIR_IN
			},
			{
				.name = "fileinformation",
				.type_name = "ptr",
				.modifiers = CDM_POINTER,
				.ref_type_name = "void",
				.direction = PDIR_OUT
			}
		},
		.fmt = FAT_NOVARARGS
	},
	{
		.name = "NtClose",
		.conv = CC_MICROSOFT,
		.type_name = "int",
		.params_cnt = 1,
		.params = {
			{
				.name = "handle",
				.type_name = "ptr",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			}
		},
		.fmt = FAT_NOVARARGS
	},
	{
		.name = "LdrLoadDll",
		.conv = CC_MICROSOFT,
		.type_name = "int",
		.params_cnt = 4,
		.params = {
			{
				.name = "pathtofile",
				.type_name = "ptr",
				.modifiers = CDM_POINTER | CDM_CONST,
				.ref_type_name = "void",
				.direction = PDIR_IN
			},
			{
				.name = "flags",
				.type_name = "ptr",
				.modifiers = CDM_POINTER,
				.ref_type_name = "void",
				.direction = PDIR_OUT
			},
			{
				.name = "modulename",
				.type_name = "ptr",
				.modifiers = CDM_POINTER | CDM_CONST,
				.ref_type_name = "ntus",
				.direction = PDIR_IN
			},
			{
				.name = "modulehandle",
				.type_name = "ptr",
				.modifiers = CDM_POINTER,
				.ref_type_name = "void",
				.direction = PDIR_OUT
			}
		},
		.fmt = FAT_NOVARARGS
	}
};
