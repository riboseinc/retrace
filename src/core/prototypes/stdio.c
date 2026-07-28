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

retrace_func_define_prototypes(stdio) = {
	{
		.name = "fclose",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 1,
		.params = {
			{
				.name = "stream",
				.type_name = "ptr",
				.modifiers = CDM_NOMOD,
				/*
				 * Do not dereference
				 *.modifiers = CDM_POINTER,
				 *.ref_type_name = "STREAM",
				 */
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "clearerr",
		.conv = CC_SYSTEM_V,
		.type_name = "void",
		.params_cnt = 1,
		.params = {
			{
				.name = "stream",
				.type_name = "ptr",
				.modifiers = CDM_NOMOD,
				/*
				 *.modifiers = CDM_POINTER,
				 *.ref_type_name = "void",
				 */
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "feof",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 1,
		.params = {
			{
				.name = "stream",
				.type_name = "ptr",
				.modifiers = CDM_NOMOD,
				/*
				 *.modifiers = CDM_POINTER,
				 *.ref_type_name = "void",
				 */
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "ferror",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 1,
		.params = {
			{
				.name = "stream",
				.type_name = "ptr",
				.modifiers = CDM_NOMOD,
				/*
				 *.modifiers = CDM_POINTER,
				 *.ref_type_name = "void",
				 */
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "fflush",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 1,
		.params = {
			{
				.name = "stream",
				.type_name = "ptr",
				.modifiers = CDM_NOMOD,
				/*
				 *.modifiers = CDM_POINTER,
				 *.ref_type_name = "void",
				 */
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "fgetpos",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 2,
		.params = {
			{
				.name = "stream",
				.type_name = "ptr",
				.modifiers = CDM_NOMOD,
				/*
				 *.modifiers = CDM_POINTER,
				 *.ref_type_name = "void",
				 */
				.direction = PDIR_IN
			},
			{
				.name = "pos",
				.type_name = "ptr",
				.modifiers = CDM_NOMOD,
				/*
				 *.modifiers = CDM_POINTER,
				 *.ref_type_name = "void",
				 */
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "fopen",
		.conv = CC_SYSTEM_V,
		.type_name = "ptr",
		.params_cnt = 2,
		.params = {
			{
				.name = "filename",
				.type_name = "ptr",
				.modifiers = CDM_POINTER | CDM_CONST,
				.ref_type_name = "sz",
				.direction = PDIR_IN
			},
			{
				.name = "mode",
				.type_name = "ptr",
				.modifiers = CDM_POINTER | CDM_CONST,
				.ref_type_name = "sz",
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "fread",
		.conv = CC_SYSTEM_V,
		.type_name = "size_t",
		.params_cnt = 4,
		.params = {
			{
				.name = "ptr",
				.type_name = "ptr",
				.modifiers = CDM_NOMOD,
				/*
				 *.modifiers = CDM_POINTER,
				 *.ref_type_name = "void",
				 */
				.direction = PDIR_OUT
			},
			{
				.name = "size",
				.type_name = "size_t",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "nmemb",
				.type_name = "size_t",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "stream",
				.type_name = "ptr",
				/*
				 *.modifiers = CDM_POINTER,
				 *.ref_type_name = "void",
				 */
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "freopen",
		.conv = CC_SYSTEM_V,
		.type_name = "ptr",
		.params_cnt = 3,
		.params = {
			{
				.name = "filename",
				.type_name = "ptr",
				.modifiers = CDM_POINTER | CDM_CONST,
				.ref_type_name = "sz",
				.direction = PDIR_IN
			},
			{
				.name = "mode",
				.type_name = "ptr",
				.modifiers = CDM_POINTER | CDM_CONST,
				.ref_type_name = "sz",
				.direction = PDIR_IN
			},
			{
				.name = "stream",
				.type_name = "ptr",
				.modifiers = CDM_NOMOD,
				/*
				 *.modifiers = CDM_POINTER,
				 *.ref_type_name = "void",
				 */
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "fseek",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 3,
		.params = {
			{
				.name = "stream",
				.type_name = "ptr",
				.modifiers = CDM_NOMOD,
				/*
				 *.modifiers = CDM_POINTER,
				 *.ref_type_name = "void",
				 */
				.direction = PDIR_IN
			},
			{
				.name = "offset",
				.type_name = "long",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "whence",
				.type_name = "int",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "fsetpos",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 2,
		.params = {
			{
				.name = "stream",
				.type_name = "ptr",
				.modifiers = CDM_NOMOD,
				/*
				 *.modifiers = CDM_POINTER,
				 *.ref_type_name = "void",
				 */
				.direction = PDIR_IN
			},
			{
				.name = "pos",
				.type_name = "ptr",
				.modifiers = CDM_NOMOD,
				/*
				 *.modifiers = CDM_POINTER | CDM_CONST,
				 *.ref_type_name = "void",
				 */
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "ftell",
		.conv = CC_SYSTEM_V,
		.type_name = "long",
		.params_cnt = 1,
		.params = {
			{
				.name = "stream",
				.type_name = "ptr",
				.modifiers = CDM_NOMOD,
				/*
				 *.modifiers = CDM_POINTER,
				 *.ref_type_name = "void",
				 */
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "fwrite",
		.conv = CC_SYSTEM_V,
		.type_name = "size_t",
		.params_cnt = 4,
		.params = {
			{
				.name = "ptr",
				.type_name = "ptr",
				.modifiers = CDM_NOMOD,
				/*
				 *.modifiers = CDM_POINTER | CDM_CONST,
				 *.ref_type_name = "void",
				 */
				.direction = PDIR_IN
			},
			{
				.name = "size",
				.type_name = "size_t",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "nmemb",
				.type_name = "size_t",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "stream",
				.modifiers = CDM_NOMOD,
				.type_name = "ptr",
				/*
				 *.modifiers = CDM_POINTER,
				 *.ref_type_name = "void",
				 */
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "remove",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 1,
		.params = {
			{
				.name = "filename",
				.type_name = "ptr",
				.modifiers = CDM_POINTER | CDM_CONST,
				.ref_type_name = "sz",
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "rename",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 2,
		.params = {
			{
				.name = "old_filename",
				.type_name = "ptr",
				.modifiers = CDM_POINTER | CDM_CONST,
				.ref_type_name = "sz",
				.direction = PDIR_IN
			},
			{
				.name = "new_filename",
				.type_name = "ptr",
				.modifiers = CDM_POINTER | CDM_CONST,
				.ref_type_name = "sz",
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "rewind",
		.conv = CC_SYSTEM_V,
		.type_name = "void",
		.params_cnt = 1,
		.params = {
			{
				.name = "stream",
				.type_name = "ptr",
				.modifiers = CDM_NOMOD,
				/*
				 *.modifiers = CDM_POINTER,
				 *.ref_type_name = "void",
				 */
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "setbuf",
		.conv = CC_SYSTEM_V,
		.type_name = "void",
		.params_cnt = 2,
		.params = {
			{
				.name = "stream",
				.type_name = "ptr",
				.modifiers = CDM_NOMOD,
				/*
				 *.modifiers = CDM_POINTER,
				 *.ref_type_name = "void",
				 */
				.direction = PDIR_IN
			},
			{
				.name = "buffer",
				.type_name = "ptr",
				.modifiers = CDM_NOMOD,
				/*
				 *.modifiers = CDM_POINTER,
				 *.ref_type_name = "void",
				 */
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "setvbuf",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 4,
		.params = {
			{
				.name = "stream",
				.type_name = "ptr",
				.modifiers = CDM_NOMOD,
				/*
				 *.modifiers = CDM_POINTER,
				 *.ref_type_name = "void",
				 */
				.direction = PDIR_IN
			},
			{
				.name = "buffer",
				.type_name = "ptr",
				.modifiers = CDM_NOMOD,
				/*
				 *.modifiers = CDM_POINTER,
				 *.ref_type_name = "void",
				 */
				.direction = PDIR_IN
			},
			{
				.name = "mode",
				.type_name = "int",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "size",
				.type_name = "size_t",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "tmpfile",
		.conv = CC_SYSTEM_V,
		.type_name = "ptr",
		.params_cnt = 0,
		.params = {
		}
	},
	{
		.name = "tmpnam",
		.conv = CC_SYSTEM_V,
		.type_name = "ptr",
		.params_cnt = 1,
		.params = {
			{
				.name = "str",
				.type_name = "ptr",
				.modifiers = CDM_POINTER,
				.ref_type_name = "sz",
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "printf",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.fmt = FAT_PRINTF,
		.fmt_param_idx = 0,
		.params_cnt = 1,
		.params = {
			{
				.name = "format",
				.type_name = "ptr",
				.modifiers = CDM_POINTER | CDM_CONST,
				.ref_type_name = "sz",
				.direction = PDIR_IN
			}
		}
	},

/* FIXME: Prototype when vararg funcs are supported by the engine */

/*
 * Variadic printf-family prototypes. fmt = FAT_PRINTF routes them through
 * retrace_as_call_real_variadic (src/core/as_call_real.c) so the compiler
 * emits the correct per-arch variadic ABI:
 *
 *   - Apple AArch64: variadic args pushed to stack
 *   - AAPCS64 (Linux/BSD ARM64): variadic args in x1..x7 then stack
 *   - Sys V x86-64: variadic args in rsi..r9 then stack
 *
 * v*printf variants take a va_list (typed as ptr) -- they are NOT variadic
 * from retrace's perspective, so they use FAT_NOVARARGS.
 */

/*
 * int fprintf(FILE *stream, const char *format, ...);
 */
{
	.name = "fprintf",
	.conv = CC_SYSTEM_V,
	.type_name = "int",
	.fmt = FAT_PRINTF,
	.fmt_param_idx = 1,
	.params_cnt = 2,
	.params = {
		{
			.name = "stream",
			.type_name = "ptr",
			.modifiers = CDM_NOMOD,
			.direction = PDIR_IN
		},
		{
			.name = "format",
			.type_name = "ptr",
			.modifiers = CDM_POINTER | CDM_CONST,
			.ref_type_name = "sz",
			.direction = PDIR_IN
		}
	}
},
{
	.name = "sprintf",
	.conv = CC_SYSTEM_V,
	.type_name = "int",
	.fmt = FAT_PRINTF,
	.fmt_param_idx = 1,
	.params_cnt = 2,
	.params = {
		{
			.name = "str",
			.type_name = "ptr",
			.modifiers = CDM_POINTER,
			.ref_type_name = "sz",
			.direction = PDIR_OUT
		},
		{
			.name = "format",
			.type_name = "ptr",
			.modifiers = CDM_POINTER | CDM_CONST,
			.ref_type_name = "sz",
			.direction = PDIR_IN
		}
	}
},
{
	.name = "snprintf",
	.conv = CC_SYSTEM_V,
	.type_name = "int",
	.fmt = FAT_PRINTF,
	.fmt_param_idx = 2,
	.params_cnt = 3,
	.params = {
		{
			.name = "str",
			.type_name = "ptr",
			.modifiers = CDM_POINTER,
			.ref_type_name = "sz",
			.direction = PDIR_OUT
		},
		{
			.name = "size",
			.type_name = "size_t",
			.modifiers = CDM_NOMOD,
			.direction = PDIR_IN
		},
		{
			.name = "format",
			.type_name = "ptr",
			.modifiers = CDM_POINTER | CDM_CONST,
			.ref_type_name = "sz",
			.direction = PDIR_IN
		}
	}
},
{
	.name = "dprintf",
	.conv = CC_SYSTEM_V,
	.type_name = "int",
	.fmt = FAT_PRINTF,
	.fmt_param_idx = 1,
	.params_cnt = 2,
	.params = {
		{
			.name = "fd",
			.type_name = "int",
			.modifiers = CDM_NOMOD,
			.direction = PDIR_IN
		},
		{
			.name = "format",
			.type_name = "ptr",
			.modifiers = CDM_POINTER | CDM_CONST,
			.ref_type_name = "sz",
			.direction = PDIR_IN
		}
	}
},
/*
 * v*printf variants take va_list (opaque pointer); no varargs to walk.
 */
{
	.name = "vprintf",
	.conv = CC_SYSTEM_V,
	.type_name = "int",
	.params_cnt = 2,
	.params = {
		{
			.name = "format",
			.type_name = "ptr",
			.modifiers = CDM_POINTER | CDM_CONST,
			.ref_type_name = "sz",
			.direction = PDIR_IN
		},
		{
			.name = "arg",
			.type_name = "ptr",
			.modifiers = CDM_NOMOD,
			.direction = PDIR_IN
		}
	}
},
{
	.name = "vfprintf",
	.conv = CC_SYSTEM_V,
	.type_name = "int",
	.params_cnt = 3,
	.params = {
		{
			.name = "stream",
			.type_name = "ptr",
			.modifiers = CDM_NOMOD,
			.direction = PDIR_IN
		},
		{
			.name = "format",
			.type_name = "ptr",
			.modifiers = CDM_POINTER | CDM_CONST,
			.ref_type_name = "sz",
			.direction = PDIR_IN
		},
		{
			.name = "arg",
			.type_name = "ptr",
			.modifiers = CDM_NOMOD,
			.direction = PDIR_IN
		}
	}
},
{
	.name = "vsprintf",
	.conv = CC_SYSTEM_V,
	.type_name = "int",
	.params_cnt = 3,
	.params = {
		{
			.name = "str",
			.type_name = "ptr",
			.modifiers = CDM_POINTER,
			.ref_type_name = "sz",
			.direction = PDIR_OUT
		},
		{
			.name = "format",
			.type_name = "ptr",
			.modifiers = CDM_POINTER | CDM_CONST,
			.ref_type_name = "sz",
			.direction = PDIR_IN
		},
		{
			.name = "arg",
			.type_name = "ptr",
			.modifiers = CDM_NOMOD,
			.direction = PDIR_IN
		}
	}
},
{
	.name = "vsnprintf",
	.conv = CC_SYSTEM_V,
	.type_name = "int",
	.params_cnt = 4,
	.params = {
		{
			.name = "str",
			.type_name = "ptr",
			.modifiers = CDM_POINTER,
			.ref_type_name = "sz",
			.direction = PDIR_OUT
		},
		{
			.name = "size",
			.type_name = "size_t",
			.modifiers = CDM_NOMOD,
			.direction = PDIR_IN
		},
		{
			.name = "format",
			.type_name = "ptr",
			.modifiers = CDM_POINTER | CDM_CONST,
			.ref_type_name = "sz",
			.direction = PDIR_IN
		},
		{
			.name = "arg",
			.type_name = "ptr",
			.modifiers = CDM_NOMOD,
			.direction = PDIR_IN
		}
	}
},
{
	.name = "vdprintf",
	.conv = CC_SYSTEM_V,
	.type_name = "int",
	.params_cnt = 3,
	.params = {
		{
			.name = "fd",
			.type_name = "int",
			.modifiers = CDM_NOMOD,
			.direction = PDIR_IN
		},
		{
			.name = "format",
			.type_name = "ptr",
			.modifiers = CDM_POINTER | CDM_CONST,
			.ref_type_name = "sz",
			.direction = PDIR_IN
		},
		{
			.name = "arg",
			.type_name = "ptr",
			.modifiers = CDM_NOMOD,
			.direction = PDIR_IN
		}
	}
},

/*
 * scanf-family variadic prototypes. fmt = FAT_SCANF routes them through
 * the same variadic dispatch as printf (retrace_as_call_real_variadic).
 *
 * All scanf variants write to the varargs pointers (PDIR_OUT semantics
 * for the user), but retrace treats each vararg as PDIR_IN by default
 * since we don't parse the format string to determine each vararg's
 * actual direction. Logging the pointer value is enough for tracing;
 * callers that want post-scan values can read through the pointers in
 * their own action.
 */
{
	.name = "scanf",
	.conv = CC_SYSTEM_V,
	.type_name = "int",
	.fmt = FAT_SCANF,
	.fmt_param_idx = 0,
	.params_cnt = 1,
	.params = {
		{
			.name = "format",
			.type_name = "ptr",
			.modifiers = CDM_POINTER | CDM_CONST,
			.ref_type_name = "sz",
			.direction = PDIR_IN
		}
	}
},
{
	.name = "fscanf",
	.conv = CC_SYSTEM_V,
	.type_name = "int",
	.fmt = FAT_SCANF,
	.fmt_param_idx = 1,
	.params_cnt = 2,
	.params = {
		{
			.name = "stream",
			.type_name = "ptr",
			.modifiers = CDM_NOMOD,
			.direction = PDIR_IN
		},
		{
			.name = "format",
			.type_name = "ptr",
			.modifiers = CDM_POINTER | CDM_CONST,
			.ref_type_name = "sz",
			.direction = PDIR_IN
		}
	}
},
{
	.name = "sscanf",
	.conv = CC_SYSTEM_V,
	.type_name = "int",
	.fmt = FAT_SCANF,
	.fmt_param_idx = 1,
	.params_cnt = 2,
	.params = {
		{
			.name = "str",
			.type_name = "ptr",
			.modifiers = CDM_POINTER | CDM_CONST,
			.ref_type_name = "sz",
			.direction = PDIR_IN
		},
		{
			.name = "format",
			.type_name = "ptr",
			.modifiers = CDM_POINTER | CDM_CONST,
			.ref_type_name = "sz",
			.direction = PDIR_IN
		}
	}
},
/*
 * v*scanf variants take va_list (opaque pointer); no varargs to walk.
 */
{
	.name = "vscanf",
	.conv = CC_SYSTEM_V,
	.type_name = "int",
	.params_cnt = 2,
	.params = {
		{
			.name = "format",
			.type_name = "ptr",
			.modifiers = CDM_POINTER | CDM_CONST,
			.ref_type_name = "sz",
			.direction = PDIR_IN
		},
		{
			.name = "arg",
			.type_name = "ptr",
			.modifiers = CDM_NOMOD,
			.direction = PDIR_IN
		}
	}
},
{
	.name = "vsscanf",
	.conv = CC_SYSTEM_V,
	.type_name = "int",
	.params_cnt = 3,
	.params = {
		{
			.name = "str",
			.type_name = "ptr",
			.modifiers = CDM_POINTER | CDM_CONST,
			.ref_type_name = "sz",
			.direction = PDIR_IN
		},
		{
			.name = "format",
			.type_name = "ptr",
			.modifiers = CDM_POINTER | CDM_CONST,
			.ref_type_name = "sz",
			.direction = PDIR_IN
		},
		{
			.name = "arg",
			.type_name = "ptr",
			.modifiers = CDM_NOMOD,
			.direction = PDIR_IN
		}
	}
},
{
	.name = "vfscanf",
	.conv = CC_SYSTEM_V,
	.type_name = "int",
	.params_cnt = 3,
	.params = {
		{
			.name = "stream",
			.type_name = "ptr",
			.modifiers = CDM_NOMOD,
			.direction = PDIR_IN
		},
		{
			.name = "format",
			.type_name = "ptr",
			.modifiers = CDM_POINTER | CDM_CONST,
			.ref_type_name = "sz",
			.direction = PDIR_IN
		},
		{
			.name = "arg",
			.type_name = "ptr",
			.modifiers = CDM_NOMOD,
			.direction = PDIR_IN
		}
	}
},

/*
 * glibc __isoc99_* variants. Modern gcc + glibc headers redirect the
 * standard scanf-family names to these via __REDIRECT, so binaries
 * built today call __isoc99_scanf (not scanf) at the PLT. We interpose
 * both: the plain names above catch older binaries and non-glibc
 * targets; the __isoc99_* names catch modern glibc binaries.
 *
 * Suffix is part of the C identifier, but prototype fields are
 * per-symbol -- the engine matches func_name verbatim.
 */
{
	.name = "__isoc99_scanf",
	.conv = CC_SYSTEM_V,
	.type_name = "int",
	.fmt = FAT_SCANF,
	.fmt_param_idx = 0,
	.params_cnt = 1,
	.params = {
		{
			.name = "format",
			.type_name = "ptr",
			.modifiers = CDM_POINTER | CDM_CONST,
			.ref_type_name = "sz",
			.direction = PDIR_IN
		}
	}
},
{
	.name = "__isoc99_fscanf",
	.conv = CC_SYSTEM_V,
	.type_name = "int",
	.fmt = FAT_SCANF,
	.fmt_param_idx = 1,
	.params_cnt = 2,
	.params = {
		{
			.name = "stream",
			.type_name = "ptr",
			.modifiers = CDM_NOMOD,
			.direction = PDIR_IN
		},
		{
			.name = "format",
			.type_name = "ptr",
			.modifiers = CDM_POINTER | CDM_CONST,
			.ref_type_name = "sz",
			.direction = PDIR_IN
		}
	}
},
{
	.name = "__isoc99_sscanf",
	.conv = CC_SYSTEM_V,
	.type_name = "int",
	.fmt = FAT_SCANF,
	.fmt_param_idx = 1,
	.params_cnt = 2,
	.params = {
		{
			.name = "str",
			.type_name = "ptr",
			.modifiers = CDM_POINTER | CDM_CONST,
			.ref_type_name = "sz",
			.direction = PDIR_IN
		},
		{
			.name = "format",
			.type_name = "ptr",
			.modifiers = CDM_POINTER | CDM_CONST,
			.ref_type_name = "sz",
			.direction = PDIR_IN
		}
	}
},
{
	.name = "__isoc99_vscanf",
	.conv = CC_SYSTEM_V,
	.type_name = "int",
	.params_cnt = 2,
	.params = {
		{
			.name = "format",
			.type_name = "ptr",
			.modifiers = CDM_POINTER | CDM_CONST,
			.ref_type_name = "sz",
			.direction = PDIR_IN
		},
		{
			.name = "arg",
			.type_name = "ptr",
			.modifiers = CDM_NOMOD,
			.direction = PDIR_IN
		}
	}
},
{
	.name = "__isoc99_vsscanf",
	.conv = CC_SYSTEM_V,
	.type_name = "int",
	.params_cnt = 3,
	.params = {
		{
			.name = "str",
			.type_name = "ptr",
			.modifiers = CDM_POINTER | CDM_CONST,
			.ref_type_name = "sz",
			.direction = PDIR_IN
		},
		{
			.name = "format",
			.type_name = "ptr",
			.modifiers = CDM_POINTER | CDM_CONST,
			.ref_type_name = "sz",
			.direction = PDIR_IN
		},
		{
			.name = "arg",
			.type_name = "ptr",
			.modifiers = CDM_NOMOD,
			.direction = PDIR_IN
		}
	}
},
{
	.name = "__isoc99_vfscanf",
	.conv = CC_SYSTEM_V,
	.type_name = "int",
	.params_cnt = 3,
	.params = {
		{
			.name = "stream",
			.type_name = "ptr",
			.modifiers = CDM_NOMOD,
			.direction = PDIR_IN
		},
		{
			.name = "format",
			.type_name = "ptr",
			.modifiers = CDM_POINTER | CDM_CONST,
			.ref_type_name = "sz",
			.direction = PDIR_IN
		},
		{
			.name = "arg",
			.type_name = "ptr",
			.modifiers = CDM_NOMOD,
			.direction = PDIR_IN
		}
	}
},

	{
		.name = "fgetc",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 1,
		.params = {
			{
				.name = "stream",
				.type_name = "ptr",
				.modifiers = CDM_NOMOD,
				/*
				 *.modifiers = CDM_POINTER,
				 *.ref_type_name = "void",
				 */
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "fgets",
		.conv = CC_SYSTEM_V,
		.type_name = "ptr",
		.params_cnt = 3,
		.params = {
			{
				.name = "str",
				.type_name = "ptr",
				.modifiers = CDM_POINTER,
				.ref_type_name = "sz",
				.direction = PDIR_OUT
			},
			{
				.name = "n",
				.type_name = "int",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "stream",
				.type_name = "ptr",
				.modifiers = CDM_NOMOD,
				/*
				 * .modifiers = CDM_POINTER,
				 * .ref_type_name = "void",
				 */

				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "fputc",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 2,
		.params = {
			{
				.name = "char",
				.type_name = "int",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "stream",
				.type_name = "ptr",
				.modifiers = CDM_NOMOD,
				/*
				 *.modifiers = CDM_POINTER,
				 *.ref_type_name = "void",
				 */
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "fputs",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 2,
		.params = {
			{
				.name = "str",
				.type_name = "ptr",
				.modifiers = CDM_POINTER | CDM_CONST,
				.ref_type_name = "sz",
				.direction = PDIR_IN
			},
			{
				.name = "stream",
				.type_name = "ptr",
				.modifiers = CDM_NOMOD,
				/*
				 *.modifiers = CDM_POINTER,
				 *.ref_type_name = "void",
				 */
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "getc",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 1,
		.params = {
			{
				.name = "stream",
				.type_name = "ptr",
				.modifiers = CDM_NOMOD,
				/*
				 *.modifiers = CDM_POINTER,
				 *.ref_type_name = "void",
				 */
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "getchar",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 0,
		.params = {
		}
	},
	{
		.name = "gets",
		.conv = CC_SYSTEM_V,
		.type_name = "ptr",
		.params_cnt = 1,
		.params = {
			{
				.name = "str",
				.type_name = "ptr",
				.modifiers = CDM_POINTER,
				.ref_type_name = "sz",
				.direction = PDIR_OUT
			}
		}
	},
	{
		.name = "putc",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 2,
		.params = {
			{
				.name = "char",
				.type_name = "int",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "stream",
				.type_name = "ptr",
				.modifiers = CDM_NOMOD,
				/*
				 *.modifiers = CDM_POINTER,
				 *.ref_type_name = "void",
				 */
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "putchar",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 2,
		.params = {
			{
				.name = "char",
				.type_name = "int",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "stream",
				.type_name = "ptr",
				.modifiers = CDM_NOMOD,
				/*
				 *.modifiers = CDM_POINTER,
				 *.ref_type_name = "void",
				 */
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "puts",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 1,
		.params = {
			{
				.name = "str",
				.type_name = "ptr",
				.modifiers = CDM_POINTER | CDM_CONST,
				.ref_type_name = "sz",
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "ungetc",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 2,
		.params = {
			{
				.name = "char",
				.type_name = "int",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "stream",
				.type_name = "ptr",
				.modifiers = CDM_NOMOD,
				/*
				 *.modifiers = CDM_POINTER,
				 *.ref_type_name = "void",
				 */
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "perror",
		.conv = CC_SYSTEM_V,
		.type_name = "void",
		.params_cnt = 1,
		.params = {
			{
				.name = "str",
				.type_name = "ptr",
				.modifiers = CDM_POINTER | CDM_CONST,
				.ref_type_name = "sz",
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "popen",
		.conv = CC_SYSTEM_V,
		.type_name = "ptr",
		.params_cnt = 2,
		.params = {
			{
				.name = "command",
				.type_name = "ptr",
				.modifiers = CDM_POINTER | CDM_CONST,
				.ref_type_name = "sz",
				.direction = PDIR_IN
			},
			{
				.name = "type",
				.type_name = "ptr",
				.modifiers = CDM_POINTER | CDM_CONST,
				.ref_type_name = "sz",
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "pclose",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 1,
		.params = {
			{
				.name = "stream",
				.type_name = "ptr",
				.modifiers = CDM_NOMOD,
				/*
				 * Do not dereference
				 *.modifiers = CDM_POINTER,
				 *.ref_type_name = "STREAM",
				 */
				.direction = PDIR_IN
			}
		}
	}
};
