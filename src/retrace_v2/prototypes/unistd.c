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

retrace_func_define_prototypes(unistd) = {
	{
		.name = "access",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params = {
			{
				.name = "pathname",
				.type_name = "ptr",
				.modifiers = CDM_POINTER | CDM_CONST,
				.ref_type_name = "sz",
				.direction = PDIR_IN
			},
			{
				.name = "mode",
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
		.name = "alarm",
		.conv = CC_SYSTEM_V,
		.type_name = "unsigned int",
		.params = {
			{
				.name = "seconds",
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
		.name = "brk",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params = {
			{
				.name = "addr",
				.type_name = "ptr",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_OUT
			},
			{
				.name = ""
			}
		}
	},
	{
		.name = "chdir",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params = {
			{
				.name = "path",
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
		.name = "chroot",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params = {
			{
				.name = "path",
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
		.name = "chown",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params = {
			{
				.name = "pathname",
				.type_name = "ptr",
				.modifiers = CDM_POINTER | CDM_CONST,
				.ref_type_name = "sz",
				.direction = PDIR_IN
			},
			{
				.name = "owner",
				.type_name = "uid_t",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "group",
				.type_name = "gid_t",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = ""
			}
		}
	},
	{
		.name = "close",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params = {
			{
				.name = "fd",
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
		.name = "confstr",
		.conv = CC_SYSTEM_V,
		.type_name = "size_t",
		.params = {
			{
				.name = "name",
				.type_name = "int",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "buf",
				.type_name = "ptr",
				.modifiers = CDM_POINTER,
				.ref_type_name = "sz",
				.array_cnt_param = "len",
				.direction = PDIR_OUT
			},
			{
				.name = "len",
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
		.name = "crypt",
		.conv = CC_SYSTEM_V,
		.type_name = "ptr",
		.params = {
			{
				.name = "key",
				.type_name = "ptr",
				.modifiers = CDM_POINTER | CDM_CONST,
				.ref_type_name = "sz",
				.direction = PDIR_IN
			},
			{
				.name = "salt",
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
		.name = "ctermid",
		.conv = CC_SYSTEM_V,
		.type_name = "ptr",
		.params = {
			{
				.name = "s",
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
		.name = "cuserid",
		.conv = CC_SYSTEM_V,
		.type_name = "ptr",
		.params = {
			{
				.name = "string",
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
		.name = "dup",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params = {
			{
				.name = "oldfd",
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
		.name = "dup2",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params = {
			{
				.name = "oldfd",
				.type_name = "int",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "newfd",
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
		.name = "encrypt",
		.conv = CC_SYSTEM_V,
		.type_name = "void",
		.params = {
			{
				.name = "block",
				.type_name = "ptr",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_INOUT
			},
			{
				.name = "edflag",
				.type_name = "int",
				.modifiers = CDM_NOMOD,
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
 *
 *		int          execl(const char *, const char *, ...);
 *		int          execle(const char *, const char *, ...);
 *		int          execlp(const char *, const char *, ...);
 */
#endif
	{
		.name = "execv",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params = {
			{
				.name = "path",
				.type_name = "ptr",
				.modifiers = CDM_POINTER | CDM_CONST,
				.ref_type_name = "sz",
				.direction = PDIR_IN
			},
			{
				.name = "argv",
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
		.name = "execve",
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
				.name = "argv",
				.type_name = "ptr",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "envp",
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
		.name = "execvp",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params = {
			{
				.name = "file",
				.type_name = "ptr",
				.modifiers = CDM_POINTER | CDM_CONST,
				.ref_type_name = "sz",
				.direction = PDIR_IN
			},
			{
				.name = "argv",
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
		.name = "_exit",
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
		.name = "fchown",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params = {
			{
				.name = "fd",
				.type_name = "int",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "owner",
				.type_name = "uid_t",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "group",
				.type_name = "gid_t",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = ""
			}
		}
	},
	{
		.name = "fchdir",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params = {
			{
				.name = "fd",
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
		.name = "fork",
		.conv = CC_SYSTEM_V,
		.type_name = "pid_t",
		.params = {
			{
				.name = ""
			}
		}
	},
	{
		.name = "fpathconf",
		.conv = CC_SYSTEM_V,
		.type_name = "long int",
		.params = {
			{
				.name = "fd",
				.type_name = "int",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "name",
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
		.name = "fsync",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params = {
			{
				.name = "fd",
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
		.name = "ftruncate",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params = {
			{
				.name = "fd",
				.type_name = "int",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "length",
				.type_name = "off_t",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = ""
			}
		}
	},
	{
		.name = "getcwd",
		.conv = CC_SYSTEM_V,
		.type_name = "ptr",
		.params = {
			{
				.name = "buf",
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
				.name = ""
			}
		}
	},
	{
		.name = "getdtablesize",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params = {
			{
				.name = ""
			}
		}
	},
	{
		.name = "getegid",
		.conv = CC_SYSTEM_V,
		.type_name = "gid_t",
		.params = {
			{
				.name = ""
			}
		}
	},
	{
		.name = "geteuid",
		.conv = CC_SYSTEM_V,
		.type_name = "uid_t",
		.params = {
			{
				.name = ""
			}
		}
	},
	{
		.name = "getgid",
		.conv = CC_SYSTEM_V,
		.type_name = "gid_t",
		.params = {
			{
				.name = ""
			}
		}
	},
	{
		.name = "getgroups",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params = {
			{
				.name = "size",
				.type_name = "size_t",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "list",
				.type_name = "gid_t",
				.modifiers = CDM_POINTER | CDM_ARRAY,
				.direction = PDIR_OUT,
				.array_cnt_param = "size"
			},
			{
				.name = ""
			}
		}
	},
	{
		.name = "gethostid",
		.conv = CC_SYSTEM_V,
		.type_name = "long",
		.params = {
			{
				.name = ""
			}
		}
	},
	{
		.name = "getlogin",
		.conv = CC_SYSTEM_V,
		.type_name = "ptr",
		.params = {
			{
				.name = ""
			}
		}
	},
	{
		.name = "getlogin_r",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params = {
			{
				.name = "buf",
				.type_name = "ptr",
				.modifiers = CDM_POINTER,
				.direction = PDIR_OUT
			},
			{
				.name = "bufsize",
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
		.name = "getopt",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params = {
			{
				.name = "argc",
				.type_name = "int",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "argv",
				.type_name = "ptr",
				.modifiers = CDM_ARRAY,
				.ref_type_name = "ptr",
				.direction = PDIR_IN,
				.array_cnt_param = "argc"
			},
			{
				.name = "optstring",
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
		.name = "getpagesize",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params = {
			{
				.name = ""
			}
		}
	},
	{
		.name = "getpass",
		.conv = CC_SYSTEM_V,
		.type_name = "ptr",
		.params = {
			{
				.name = "prompt",
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
		.name = "getpgid",
		.conv = CC_SYSTEM_V,
		.type_name = "pid_t",
		.params = {
			{
				.name = "pid",
				.type_name = "pid_t",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = ""
			}
		}
	},
	{
		.name = "getpgrp",
		.conv = CC_SYSTEM_V,
		.type_name = "pid_t",
		.params = {
			{
				.name = ""
			}
		}
	},
	{
		.name = "getpid",
		.conv = CC_SYSTEM_V,
		.type_name = "pid_t",
		.params = {
			{
				.name = ""
			}
		}
	},
	{
		.name = "getppid",
		.conv = CC_SYSTEM_V,
		.type_name = "pid_t",
		.params = {
			{
				.name = ""
			}
		}
	},
	{
		.name = "getsid",
		.conv = CC_SYSTEM_V,
		.type_name = "pid_t",
		.params = {
			{
				.name = "pid",
				.type_name = "pid_t",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = ""
			}
		}
	},
	{
		.name = "getuid",
		.conv = CC_SYSTEM_V,
		.type_name = "uid_t",
		.params = {
			{
				.name = ""
			}
		}
	},
	{
		.name = "getwd",
		.conv = CC_SYSTEM_V,
		.type_name = "ptr",
		.params = {
			{
				.name = "buf",
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
		.name = "isatty",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params = {
			{
				.name = "fd",
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
		.name = "lchown",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params = {
			{
				.name = "path",
				.type_name = "ptr",
				.modifiers = CDM_POINTER | CDM_CONST,
				.ref_type_name = "sz",
				.direction = PDIR_IN
			},
			{
				.name = "owner",
				.type_name = "uid_t",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "group",
				.type_name = "gid_t",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = ""
			}
		}
	},
	{
		.name = "link",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params = {
			{
				.name = "oldpath",
				.type_name = "ptr",
				.modifiers = CDM_POINTER | CDM_CONST,
				.ref_type_name = "sz",
				.direction = PDIR_IN
			},
			{
				.name = "newpath",
				.type_name = "ptr",
				.modifiers = CDM_POINTER | CDM_CONST,
				.ref_type_name = "sz",
				.direction = PDIR_IN
			},
			{
				.name = "owner",
				.type_name = "uid_t",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = ""
			}
		}
	},
	{
		.name = "lockf",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params = {
			{
				.name = "fd",
				.type_name = "int",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "cmd",
				.type_name = "int",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "len",
				.type_name = "off_t",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = ""
			}
		}
	},
	{
		.name = "lseek",
		.conv = CC_SYSTEM_V,
		.type_name = "off_t",
		.params = {
			{
				.name = "fd",
				.type_name = "int",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "len",
				.type_name = "off_t",
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
		.name = "nice",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params = {
			{
				.name = "fd",
				.type_name = "int",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "inc",
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
		.name = "pathconf",
		.conv = CC_SYSTEM_V,
		.type_name = "long int",
		.params = {
			{
				.name = "path",
				.type_name = "ptr",
				.modifiers = CDM_POINTER | CDM_CONST,
				.ref_type_name = "sz",
				.direction = PDIR_IN
			},
			{
				.name = "name",
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
		.name = "pause",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params = {
			{
				.name = ""
			}
		}
	},
	{
		.name = "pipe",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params = {
			{
				.name = "pipefd",
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
		.name = "pread",
		.conv = CC_SYSTEM_V,
		.type_name = "ssize_t",
		.params = {
			{
				.name = "fd",
				.type_name = "int",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "buf",
				.type_name = "ptr",
				.modifiers = CDM_POINTER | CDM_ARRAY,
				.ref_type_name = "char",
				.direction = PDIR_OUT,
				.array_cnt_param = "count"
			},
			{
				.name = "count",
				.type_name = "size_t",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "offset",
				.type_name = "off_t",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = ""
			}
		}
	},
	{
		.name = "pthread_atfork",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params = {
			{
				.name = "prepare",
				.type_name = "ptr",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "parent",
				.type_name = "ptr",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "child",
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
		.name = "pwrite",
		.conv = CC_SYSTEM_V,
		.type_name = "ssize_t",
		.params = {
			{
				.name = "fd",
				.type_name = "int",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "buf",
				.type_name = "ptr",
				.modifiers = CDM_POINTER | CDM_ARRAY,
				.ref_type_name = "char",
				.direction = PDIR_IN,
				.array_cnt_param = "count"
			},
			{
				.name = "count",
				.type_name = "size_t",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "offset",
				.type_name = "off_t",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = ""
			}
		}
	},
	{
		.name = "read",
		.conv = CC_SYSTEM_V,
		.type_name = "ssize_t",
		.params = {
			{
				.name = "fd",
				.type_name = "int",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "buf",
				.type_name = "ptr",
				.modifiers = CDM_POINTER | CDM_ARRAY,
				.ref_type_name = "char",
				.direction = PDIR_OUT,
				.array_cnt_param = "count"
			},
			{
				.name = "count",
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
		.name = "readlink",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params = {
			{
				.name = "path",
				.type_name = "ptr",
				.modifiers = CDM_POINTER | CDM_CONST,
				.ref_type_name = "sz",
				.direction = PDIR_IN

			},
			{
				.name = "buf",
				.type_name = "ptr",
				.modifiers = CDM_POINTER,
				.ref_type_name = "sz",
				.direction = PDIR_OUT,
			},
			{
				.name = "bufsiz",
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
		.name = "rmdir",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params = {
			{
				.name = "pathname",
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
		.name = "sbrk",
		.conv = CC_SYSTEM_V,
		.type_name = "ptr",
		.params = {
			{
				.name = "increment",
				.type_name = "intptr_t",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = ""
			}
		}
	},
	{
		.name = "setgid",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params = {
			{
				.name = "gid",
				.type_name = "gid_t",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = ""
			}
		}
	},
	{
		.name = "setpgid",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params = {
			{
				.name = "pid",
				.type_name = "pid_t",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "pgid",
				.type_name = "pid_t",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = ""
			}
		}
	},
	{
		.name = "setpgrp",
		.conv = CC_SYSTEM_V,
		.type_name = "pid_t",
		.params = {
			{
				.name = ""
			}
		}
	},
	{
		.name = "setregid",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params = {
			{
				.name = "rgid",
				.type_name = "gid_t",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "egid",
				.type_name = "gid_t",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = ""
			}
		}
	},
	{
		.name = "setreuid",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params = {
			{
				.name = "ruid",
				.type_name = "uid_t",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "euid",
				.type_name = "uid_t",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = ""
			}
		}
	},
	{
		.name = "setsid",
		.conv = CC_SYSTEM_V,
		.type_name = "pid_t",
		.params = {
			{
				.name = ""
			}
		}
	},
	{
		.name = "setuid",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params = {
			{
				.name = "uid",
				.type_name = "uid_t",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = ""
			}
		}
	},
	{
		.name = "sleep",
		.conv = CC_SYSTEM_V,
		.type_name = "unsigned int ",
		.params = {
			{
				.name = "seconds",
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
		.name = "swab",
		.conv = CC_SYSTEM_V,
		.type_name = "void",
		.params = {
			{
				.name = "from",
				.type_name = "ptr",
				.modifiers = CDM_POINTER | CDM_CONST | CDM_ARRAY,
				.ref_type_name = "char",
				.direction = PDIR_IN,
				.array_cnt_param = "n"
			},
			{
				.name = "to",
				.type_name = "ptr",
				.modifiers = CDM_POINTER | CDM_ARRAY,
				.ref_type_name = "char",
				.direction = PDIR_OUT,
				.array_cnt_param = "n"
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
		.name = "symlink",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params = {
			{
				.name = "path1",
				.type_name = "ptr",
				.modifiers = CDM_POINTER | CDM_CONST,
				.ref_type_name = "sz",
				.direction = PDIR_IN,
			},
			{
				.name = "path2",
				.type_name = "ptr",
				.modifiers = CDM_POINTER | CDM_CONST,
				.ref_type_name = "sz",
				.direction = PDIR_IN,
			},
			{
				.name = ""
			}
		}
	},
	{
		.name = "sync",
		.conv = CC_SYSTEM_V,
		.type_name = "void",
		.params = {
			{
				.name = ""
			}
		}
	},
	{
		.name = "sysconf",
		.conv = CC_SYSTEM_V,
		.type_name = "long int",
		.params = {
			{
				.name = "name",
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
		.name = "tcgetpgrp",
		.conv = CC_SYSTEM_V,
		.type_name = "pid_t",
		.params = {
			{
				.name = "fd",
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
		.name = "truncate",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params = {
			{
				.name = "path",
				.type_name = "ptr",
				.modifiers = CDM_POINTER | CDM_CONST,
				.ref_type_name = "sz",
				.direction = PDIR_IN
			},
			{
				.name = "length",
				.type_name = "off_t",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = ""
			}
		}
	},
	{
		.name = "ttyname",
		.conv = CC_SYSTEM_V,
		.type_name = "ptr",
		.params = {
			{
				.name = "fd",
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
		.name = "ttyname_r",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params = {
			{
				.name = "fd",
				.type_name = "int",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "buf",
				.type_name = "ptr",
				.modifiers = CDM_POINTER,
				.ref_type_name = "sz",
				.direction = PDIR_OUT
			},
			{
				.name = "buflen",
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
		.name = "ualarm",
		.conv = CC_SYSTEM_V,
		.type_name = "useconds_t",
		.params = {
			{
				.name = "usecs",
				.type_name = "useconds_t",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "interval",
				.type_name = "useconds_t",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = ""
			}
		}
	},
	{
		.name = "unlink",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params = {
			{
				.name = "pathname",
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
		.name = "usleep",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params = {
			{
				.name = "usec",
				.type_name = "useconds_t",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = ""
			}
		}
	},
	{
		.name = "vfork",
		.conv = CC_SYSTEM_V,
		.type_name = "pid_t",
		.params = {
			{
				.name = ""
			}
		}
	},
	{
		.name = "write",
		.conv = CC_SYSTEM_V,
		.type_name = "ssize_t",
		.params = {
			{
				.name = "fd",
				.type_name = "int",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "buf",
				.type_name = "ptr",
				.modifiers = CDM_POINTER | CDM_ARRAY | CDM_CONST,
				.ref_type_name = "char",
				.direction = PDIR_IN,
				.array_cnt_param = "count"
			},
			{
				.name = "count",
				.type_name = "size_t",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = ""
			}
		}
	}
};
