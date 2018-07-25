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
			},
			{
				.name = ""
			}
		}
	},
	{
		.name = "clearerr",
		.conv = CC_SYSTEM_V,
		.type_name = "void",
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
				.name = ""
			}
		}
	},
	{
		.name = "feof",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
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
				.name = ""
			}
		}
	},
	{
		.name = "ferror",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
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
				.name = ""
			}
		}
	},
	{
		.name = "fflush",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
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
				.name = ""
			}
		}
	},
	{
		.name = "fgetpos",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
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
			},
			{
				.name = ""
			}
		}
	},
	{
		.name = "fopen",
		.conv = CC_SYSTEM_V,
		.type_name = "ptr",
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
				.name = ""
			}
		}
	},
	{
		.name = "fread",
		.conv = CC_SYSTEM_V,
		.type_name = "size_t",
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
			},
			{
				.name = ""
			}
		}
	},
	{
		.name = "freopen",
		.conv = CC_SYSTEM_V,
		.type_name = "ptr",
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
			},
			{
				.name = ""
			}
		}
	},
	{
		.name = "fseek",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
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
				.type_name = "long int",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "whence",
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
		.name = "fsetpos",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
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
			},
			{
				.name = ""
			}
		}
	},
	{
		.name = "ftell",
		.conv = CC_SYSTEM_V,
		.type_name = "long int",
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
				.name = ""
			}
		}
	},
	{
		.name = "fwrite",
		.conv = CC_SYSTEM_V,
		.type_name = "size_t",
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
			},
			{
				.name = ""
			}
		}
	},
	{
		.name = "remove",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params = {
			{
				.name = "filename",
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
		.name = "rename",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
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
			},
			{
				.name = ""
			}
		}
	},
	{
		.name = "rewind",
		.conv = CC_SYSTEM_V,
		.type_name = "void",
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
				.name = ""
			}
		}
	},
	{
		.name = "setbuf",
		.conv = CC_SYSTEM_V,
		.type_name = "void",
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
				.name = ""
			}
		}
	},
	{
		.name = "setvbuf",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
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
			},
			{
				.name = ""
			}
		}
	},
	{
		.name = "tmpfile",
		.conv = CC_SYSTEM_V,
		.type_name = "ptr",
		.params = {
			{
				.name = ""
			}
		}
	},
	{
		.name = "tmpnam",
		.conv = CC_SYSTEM_V,
		.type_name = "ptr",
		.params = {
			{
				.name = "str",
				.type_name = "ptr",
				.modifiers = CDM_POINTER,
				.ref_type_name = "sz",
				.direction = PDIR_IN
			},
			{
				.name = ""
			}
		}
	},
/* FIXME: Prototype when vararg funcs are supported by the engine */
#if 0

/*
 *
 *	int fprintf(FILE *stream, const char *format, ...)
 *	int printf(const char *format, ...)
 *	int sprintf(char *str, const char *format, ...)
 *	int vfprintf(FILE *stream, const char *format, va_list arg)
 *	int vprintf(const char *format, va_list arg)
 *	int vsprintf(char *str, const char *format, va_list arg)
 *	int fscanf(FILE *stream, const char *format, ...)
 *	int scanf(const char *format, ...)
 *	int sscanf(const char *str, const char *format, ...)
 */
#endif
	{
		.name = "fgetc",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
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
				.name = ""
			}
		}
	},
	{
		.name = "fgets",
		.conv = CC_SYSTEM_V,
		.type_name = "ptr",
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
			},
			{
				.name = ""
			}
		}
	},
	{
		.name = "fputc",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
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
			},
			{
				.name = ""
			}
		}
	},
	{
		.name = "fputs",
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
				.name = ""
			}
		}
	},
	{
		.name = "getc",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
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
				.name = ""
			}
		}
	},
	{
		.name = "getchar",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params = {
			{
				.name = ""
			}
		}
	},
	{
		.name = "gets",
		.conv = CC_SYSTEM_V,
		.type_name = "ptr",
		.params = {
			{
				.name = "str",
				.type_name = "ptr",
				.modifiers = CDM_POINTER,
				.ref_type_name = "sz",
				.direction = PDIR_OUT
			},
			{
				.name = ""
			}
		}
	},
	{
		.name = "putc",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
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
			},
			{
				.name = ""
			}
		}
	},
	{
		.name = "putchar",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
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
			},
			{
				.name = ""
			}
		}
	},
	{
		.name = "puts",
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
		.name = "ungetc",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
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
			},
			{
				.name = ""
			}
		}
	},
	{
		.name = "perror",
		.conv = CC_SYSTEM_V,
		.type_name = "void",
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
};
