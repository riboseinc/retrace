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
				/*
				 *.type_name = "ptr",
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
 *
 *	int fprintf(FILE *stream, const char *format, ...)
 *	int sprintf(char *str, const char *format, ...)
 *	int vfprintf(FILE *stream, const char *format, va_list arg)
 *	int vprintf(const char *format, va_list arg)
 *	int vsprintf(char *str, const char *format, va_list arg)
 *	int fscanf(FILE *stream, const char *format, ...)
 *	int scanf(const char *format, ...)
 *	int sscanf(const char *str, const char *format, ...)
 */

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
	}
};
