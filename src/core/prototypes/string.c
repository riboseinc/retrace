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
 * string.h prototypes. Each entry mirrors a WRAPPER_ENTRY_SYSTEM_V line
 * in src/v2/funcs_symbols.S so the engine can parse the call's args for
 * log_params / modify_in_param_str.
 *
 * Conventions used below:
 *
 *   - C-string inputs/outputs use {.modifiers = CDM_POINTER|CDM_CONST,
 *     .ref_type_name = "sz", .direction = PDIR_IN} (or PDIR_OUT for
 *     dst buffers).
 *   - Opaque libc pointers (no derefdg semantics useful to the user,
 *     e.g. internal state) use {.modifiers = CDM_NOMOD}. log_params
 *     shows the address; no deref.
 *   - Counts are size_t with CDM_NOMOD.
 *
 * strtok/strerror carry caveats documented inline.
 */

#include "funcs.h"

/*
 * Length / comparison family.
 */
retrace_func_define_prototypes(string) = {
	{
		.name = "strlen",
		.conv = CC_SYSTEM_V,
		.type_name = "size_t",
		.params_cnt = 1,
		.params = {
			{
				.name = "s",
				.type_name = "ptr",
				.modifiers = CDM_POINTER | CDM_CONST,
				.ref_type_name = "sz",
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "strcmp",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 2,
		.params = {
			{
				.name = "s1",
				.type_name = "ptr",
				.modifiers = CDM_POINTER | CDM_CONST,
				.ref_type_name = "sz",
				.direction = PDIR_IN
			},
			{
				.name = "s2",
				.type_name = "ptr",
				.modifiers = CDM_POINTER | CDM_CONST,
				.ref_type_name = "sz",
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "strncmp",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 3,
		.params = {
			{
				.name = "s1",
				.type_name = "ptr",
				.modifiers = CDM_POINTER | CDM_CONST,
				.ref_type_name = "sz",
				.direction = PDIR_IN
			},
			{
				.name = "s2",
				.type_name = "ptr",
				.modifiers = CDM_POINTER | CDM_CONST,
				.ref_type_name = "sz",
				.direction = PDIR_IN
			},
			{
				.name = "n",
				.type_name = "size_t",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "strcoll",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 2,
		.params = {
			{
				.name = "s1",
				.type_name = "ptr",
				.modifiers = CDM_POINTER | CDM_CONST,
				.ref_type_name = "sz",
				.direction = PDIR_IN
			},
			{
				.name = "s2",
				.type_name = "ptr",
				.modifiers = CDM_POINTER | CDM_CONST,
				.ref_type_name = "sz",
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "strerror",
		.conv = CC_SYSTEM_V,
		.type_name = "ptr",
		.params_cnt = 1,
		.params = {
			{
				.name = "errnum",
				.type_name = "int",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "strxfrm",
		.conv = CC_SYSTEM_V,
		.type_name = "size_t",
		.params_cnt = 3,
		.params = {
			{
				.name = "dst",
				.type_name = "ptr",
				.modifiers = CDM_POINTER,
				.ref_type_name = "sz",
				.direction = PDIR_OUT
			},
			{
				.name = "src",
				.type_name = "ptr",
				.modifiers = CDM_POINTER | CDM_CONST,
				.ref_type_name = "sz",
				.direction = PDIR_IN
			},
			{
				.name = "n",
				.type_name = "size_t",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			}
		}
	},

/*
 * Search family.
 */
	{
		.name = "strchr",
		.conv = CC_SYSTEM_V,
		.type_name = "ptr",
		.params_cnt = 2,
		.params = {
			{
				.name = "s",
				.type_name = "ptr",
				.modifiers = CDM_POINTER | CDM_CONST,
				.ref_type_name = "sz",
				.direction = PDIR_IN
			},
			{
				.name = "c",
				.type_name = "int",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "strrchr",
		.conv = CC_SYSTEM_V,
		.type_name = "ptr",
		.params_cnt = 2,
		.params = {
			{
				.name = "s",
				.type_name = "ptr",
				.modifiers = CDM_POINTER | CDM_CONST,
				.ref_type_name = "sz",
				.direction = PDIR_IN
			},
			{
				.name = "c",
				.type_name = "int",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "strstr",
		.conv = CC_SYSTEM_V,
		.type_name = "ptr",
		.params_cnt = 2,
		.params = {
			{
				.name = "haystack",
				.type_name = "ptr",
				.modifiers = CDM_POINTER | CDM_CONST,
				.ref_type_name = "sz",
				.direction = PDIR_IN
			},
			{
				.name = "needle",
				.type_name = "ptr",
				.modifiers = CDM_POINTER | CDM_CONST,
				.ref_type_name = "sz",
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "strpbrk",
		.conv = CC_SYSTEM_V,
		.type_name = "ptr",
		.params_cnt = 2,
		.params = {
			{
				.name = "s",
				.type_name = "ptr",
				.modifiers = CDM_POINTER | CDM_CONST,
				.ref_type_name = "sz",
				.direction = PDIR_IN
			},
			{
				.name = "accept",
				.type_name = "ptr",
				.modifiers = CDM_POINTER | CDM_CONST,
				.ref_type_name = "sz",
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "strcspn",
		.conv = CC_SYSTEM_V,
		.type_name = "size_t",
		.params_cnt = 2,
		.params = {
			{
				.name = "s",
				.type_name = "ptr",
				.modifiers = CDM_POINTER | CDM_CONST,
				.ref_type_name = "sz",
				.direction = PDIR_IN
			},
			{
				.name = "reject",
				.type_name = "ptr",
				.modifiers = CDM_POINTER | CDM_CONST,
				.ref_type_name = "sz",
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "strspn",
		.conv = CC_SYSTEM_V,
		.type_name = "size_t",
		.params_cnt = 2,
		.params = {
			{
				.name = "s",
				.type_name = "ptr",
				.modifiers = CDM_POINTER | CDM_CONST,
				.ref_type_name = "sz",
				.direction = PDIR_IN
			},
			{
				.name = "accept",
				.type_name = "ptr",
				.modifiers = CDM_POINTER | CDM_CONST,
				.ref_type_name = "sz",
				.direction = PDIR_IN
			}
		}
	},
	/*
	 * strtok is stateful: the engine can't model libc's internal
	 * pointer that survives across calls. log_params will show the
	 * first-arg pointer correctly; users needing state visibility
	 * should switch to strtok_r (which we don't yet prototype).
	 */
	{
		.name = "strtok",
		.conv = CC_SYSTEM_V,
		.type_name = "ptr",
		.params_cnt = 2,
		.params = {
			{
				.name = "s",
				.type_name = "ptr",
				.modifiers = CDM_POINTER | CDM_CONST,
				.ref_type_name = "sz",
				.direction = PDIR_IN
			},
			{
				.name = "delim",
				.type_name = "ptr",
				.modifiers = CDM_POINTER | CDM_CONST,
				.ref_type_name = "sz",
				.direction = PDIR_IN
			}
		}
	},

/*
 * Copy / concat family.
 */
	{
		.name = "strcpy",
		.conv = CC_SYSTEM_V,
		.type_name = "ptr",
		.params_cnt = 2,
		.params = {
			{
				.name = "dst",
				.type_name = "ptr",
				.modifiers = CDM_POINTER,
				.ref_type_name = "sz",
				.direction = PDIR_OUT
			},
			{
				.name = "src",
				.type_name = "ptr",
				.modifiers = CDM_POINTER | CDM_CONST,
				.ref_type_name = "sz",
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "strncpy",
		.conv = CC_SYSTEM_V,
		.type_name = "ptr",
		.params_cnt = 3,
		.params = {
			{
				.name = "dst",
				.type_name = "ptr",
				.modifiers = CDM_POINTER,
				.ref_type_name = "sz",
				.direction = PDIR_OUT
			},
			{
				.name = "src",
				.type_name = "ptr",
				.modifiers = CDM_POINTER | CDM_CONST,
				.ref_type_name = "sz",
				.direction = PDIR_IN
			},
			{
				.name = "n",
				.type_name = "size_t",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "strcat",
		.conv = CC_SYSTEM_V,
		.type_name = "ptr",
		.params_cnt = 2,
		.params = {
			{
				.name = "dst",
				.type_name = "ptr",
				.modifiers = CDM_POINTER,
				.ref_type_name = "sz",
				.direction = PDIR_INOUT
			},
			{
				.name = "src",
				.type_name = "ptr",
				.modifiers = CDM_POINTER | CDM_CONST,
				.ref_type_name = "sz",
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "strncat",
		.conv = CC_SYSTEM_V,
		.type_name = "ptr",
		.params_cnt = 3,
		.params = {
			{
				.name = "dst",
				.type_name = "ptr",
				.modifiers = CDM_POINTER,
				.ref_type_name = "sz",
				.direction = PDIR_INOUT
			},
			{
				.name = "src",
				.type_name = "ptr",
				.modifiers = CDM_POINTER | CDM_CONST,
				.ref_type_name = "sz",
				.direction = PDIR_IN
			},
			{
				.name = "n",
				.type_name = "size_t",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			}
		}
	},

/*
 * Memory family. Note: memset/memcpy/memmove are NOT wrapped on Linux
 * (see src/v2/funcs_symbols.S comment about dlsym recursion). The
 * prototypes here are for BSD/Darwin where they ARE wrapped.
 */
	{
		.name = "memchr",
		.conv = CC_SYSTEM_V,
		.type_name = "ptr",
		.params_cnt = 3,
		.params = {
			{
				.name = "s",
				.type_name = "ptr",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "c",
				.type_name = "int",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "n",
				.type_name = "size_t",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "memcmp",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 3,
		.params = {
			{
				.name = "s1",
				.type_name = "ptr",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "s2",
				.type_name = "ptr",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "n",
				.type_name = "size_t",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "memcpy",
		.conv = CC_SYSTEM_V,
		.type_name = "ptr",
		.params_cnt = 3,
		.params = {
			{
				.name = "dst",
				.type_name = "ptr",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_OUT
			},
			{
				.name = "src",
				.type_name = "ptr",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "n",
				.type_name = "size_t",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "memmove",
		.conv = CC_SYSTEM_V,
		.type_name = "ptr",
		.params_cnt = 3,
		.params = {
			{
				.name = "dst",
				.type_name = "ptr",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_OUT
			},
			{
				.name = "src",
				.type_name = "ptr",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "n",
				.type_name = "size_t",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "memset",
		.conv = CC_SYSTEM_V,
		.type_name = "ptr",
		.params_cnt = 3,
		.params = {
			{
				.name = "s",
				.type_name = "ptr",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_OUT
			},
			{
				.name = "c",
				.type_name = "int",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "n",
				.type_name = "size_t",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			}
		}
	}
};
