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

retrace_func_define_prototypes(stdlib) = {
	/* TODO: Uncomment when float is supported */
#if 0
	{
		.name = "atof",
		.conv = CC_SYSTEM_V,
		.type_name = "double",
		.params = {
			{
				.name = "str",
				.type_name = "ptr",
				.modifiers = CDM_POINTER | CDM_CONST,
				.ref_type_name = "sz",
				.direction = PDIR_IN
			},
			{
				.name = ""
			}
		}
	}
#endif
	{
		.name = "atoi",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params = {
			{
				.name = "str",
				.type_name = "ptr",
				.modifiers = CDM_POINTER | CDM_CONST,
				.ref_type_name = "sz",
				.direction = PDIR_IN
			},
			{
				.name = ""
			}
		}
	},
	{
		.name = "atol",
		.conv = CC_SYSTEM_V,
		.type_name = "long int",
		.params = {
			{
				.name = "str",
				.type_name = "ptr",
				.modifiers = CDM_POINTER | CDM_CONST,
				.ref_type_name = "sz",
				.direction = PDIR_IN
			},
			{
				.name = ""
			}
		}
	},
	/* TODO: Uncomment when float is supported */
#if 0
	{
		.name = "strtod",
		.conv = CC_SYSTEM_V,
		.type_name = "double",
		.params = {
			{
				.name = "str",
				.type_name = "ptr",
				.modifiers = CDM_POINTER | CDM_CONST,
				.ref_type_name = "sz",
				.direction = PDIR_IN
			},
			{
				.name = "endptr",
				.type_name = "ptr",
				.modifiers = CDM_POINTER,
				.ref_type_name = "ptr",
				.direction = PDIR_OUT
			},
			{
				.name = ""
			}
		}
	},
#endif
	{
		.name = "strtol",
		.conv = CC_SYSTEM_V,
		.type_name = "long int",
		.params = {
			{
				.name = "str",
				.type_name = "ptr",
				.modifiers = CDM_POINTER | CDM_CONST,
				.ref_type_name = "sz",
				.direction = PDIR_IN
			},
			{
				.name = "endptr",
				.type_name = "ptr",
				.modifiers = CDM_POINTER,
				.ref_type_name = "ptr",
				.direction = PDIR_OUT
			},
			{
				.name = "base",
				.type_name = "int",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = ""
			}
		}
	},
	{
		.name = "strtoul",
		.conv = CC_SYSTEM_V,
		.type_name = "unsigned long int",
		.params = {
			{
				.name = "str",
				.type_name = "ptr",
				.modifiers = CDM_POINTER | CDM_CONST,
				.ref_type_name = "sz",
				.direction = PDIR_IN
			},
			{
				.name = "endptr",
				.type_name = "ptr",
				.modifiers = CDM_POINTER,
				.ref_type_name = "ptr",
				.direction = PDIR_OUT
			},
			{
				.name = "base",
				.type_name = "int",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = ""
			}
		}
	},
	{
		.name = "calloc",
		.conv = CC_SYSTEM_V,
		.type_name = "ptr",
		.params = {
			{
				.name = "nitems",
				.type_name = "size_t",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "size",
				.type_name = "size_t",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = ""
			}
		}
	},
	{
		.name = "free",
		.conv = CC_SYSTEM_V,
		.type_name = "void",
		.params = {
			{
				.name = "ptr",
				.type_name = "ptr",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = ""
			}
		}
	},
	{
		.name = "malloc",
		.conv = CC_SYSTEM_V,
		.type_name = "ptr",
		.params = {
			{
				.name = "size",
				.type_name = "size_t",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = ""
			}
		}
	},
	{
		.name = "realloc",
		.conv = CC_SYSTEM_V,
		.type_name = "ptr",
		.params = {
			{
				.name = "ptr",
				.type_name = "ptr",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "size",
				.type_name = "size_t",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = ""
			}
		}
	},
	{
		.name = "abort",
		.conv = CC_SYSTEM_V,
		.type_name = "void",
		.params = {
			{
				.name = ""
			}
		}
	},
	{
		.name = "atexit",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params = {
			{
				.name = "func",
				.type_name = "ptr",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = ""
			}
		}
	},
	{
		.name = "exit",
		.conv = CC_SYSTEM_V,
		.type_name = "void",
		.params = {
			{
				.name = "status",
				.type_name = "int",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = ""
			}
		}
	},
	{
		.name = "getenv",
		.conv = CC_SYSTEM_V,
		.type_name = "ptr",
		.params = {
			{
				.name = "name",
				.type_name = "ptr",
				.modifiers = CDM_POINTER | CDM_CONST,
				.ref_type_name = "sz",
				.direction = PDIR_IN
			},
			{
				.name = ""
			}
		}
	},
	{
		.name = "system",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params = {
			{
				.name = "string",
				.type_name = "ptr",
				.modifiers = CDM_POINTER | CDM_CONST,
				.ref_type_name = "sz",
				.direction = PDIR_IN
			},
			{
				.name = ""
			}
		}
	},
	{
		.name = "bsearch",
		.conv = CC_SYSTEM_V,
		.type_name = "ptr",
		.params = {
			{
				.name = "key",
				.type_name = "ptr",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "base",
				.type_name = "ptr",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "nitems",
				.type_name = "size_t",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "size",
				.type_name = "size_t",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "compar",
				.type_name = "ptr",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = ""
			}
		}
	},
	{
		.name = "qsort",
		.conv = CC_SYSTEM_V,
		.type_name = "void",
		.params = {
			{
				.name = "base",
				.type_name = "ptr",
				.direction = PDIR_IN,
				.modifiers = CDM_NOMOD
			},
			{
				.name = "nitems",
				.type_name = "size_t",
				.direction = PDIR_IN,
				.modifiers = CDM_NOMOD
			},
			{
				.name = "size",
				.type_name = "size_t",
				.direction = PDIR_IN,
				.modifiers = CDM_NOMOD
			},
			{
				.name = "compar",
				.type_name = "ptr",
				.direction = PDIR_IN,
				.modifiers = CDM_NOMOD
			},
			{
				.name = ""
			}
		}
	},
	{
		.name = "abs",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params = {
			{
				.name = "x",
				.type_name = "int",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = ""
			}
		}
	},
	{
		.name = "div",
		.conv = CC_SYSTEM_V,
		.type_name = "ptr",
		.params = {
			{
				.name = "numer",
				.type_name = "int",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "denom",
				.type_name = "int",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = ""
			}
		}
	},
	{
		.name = "labs",
		.conv = CC_SYSTEM_V,
		.type_name = "long int",
		.params = {
			{
				.name = "x",
				.type_name = "long int",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = ""
			}
		}
	},
	{
		.name = "ldiv",
		.conv = CC_SYSTEM_V,
		.type_name = "ptr",
		.params = {
			{
				.name = "numer",
				.type_name = "long int",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "denom",
				.type_name = "long int",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = ""
			}
		}
	},
	{
		.name = "rand",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params = {
			{
				.name = ""
			}
		}
	},
	{
		.name = "srand",
		.conv = CC_SYSTEM_V,
		.type_name = "void",
		.params = {
			{
				.name = "seed",
				.type_name = "unsigned int",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = ""
			}
		}
	},
	{
		.name = "mblen",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params = {
			{
				.name = "str",
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
			},
			{
				.name = ""
			}
		}
	},
	{
		.name = "mbstowcs",
		.conv = CC_SYSTEM_V,
		.type_name = "size_t",
		.params = {
			{
				.name = "pwcs",
				.type_name = "ptr",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_OUT
			},
			{
				.name = "str",
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
			},
			{
				.name = ""
			}
		}
	},
	{
		.name = "mbtowc",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params = {
			{
				.name = "pwcs",
				.type_name = "ptr",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_OUT
			},
			{
				.name = "str",
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
			},
			{
				.name = ""
			}
		}
	},
	{
		.name = "wcstombs",
		.conv = CC_SYSTEM_V,
		.type_name = "size_t",
		.params = {
			{
				.name = "str",
				.type_name = "ptr",
				.modifiers = CDM_POINTER,
				.ref_type_name = "sz",
				.direction = PDIR_OUT
			},
			{
				.name = "pwcs",
				.type_name = "ptr",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "n",
				.type_name = "size_t",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = ""
			}
		}
	},
	{
		.name = "wctomb",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params = {
			{
				.name = "str",
				.type_name = "ptr",
				.modifiers = CDM_POINTER,
				.ref_type_name = "sz",
				.direction = PDIR_OUT
			},
			{
				.name = "wchar",
				.type_name = "int16_t",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = ""
			}
		}
	}
};
