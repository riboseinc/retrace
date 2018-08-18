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
		.params_cnt = 2,
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
			}
		}
	},
	{
		.name = "alarm",
		.conv = CC_SYSTEM_V,
		.type_name = "unsigned int",
		.params_cnt = 1,
		.params = {
			{
				.name = "seconds",
				.type_name = "unsigned int",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "brk",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 1,
		.params = {
			{
				.name = "addr",
				.type_name = "ptr",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_OUT
			}
		}
	},
	{
		.name = "chdir",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 1,
		.params = {
			{
				.name = "path",
				.type_name = "ptr",
				.modifiers = CDM_POINTER | CDM_CONST,
				.ref_type_name = "sz",
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "chroot",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 1,
		.params = {
			{
				.name = "path",
				.type_name = "ptr",
				.modifiers = CDM_POINTER | CDM_CONST,
				.ref_type_name = "sz",
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "chown",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 3,
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
			}
		}
	},
	{
		.name = "close",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
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
		.name = "confstr",
		.conv = CC_SYSTEM_V,
		.type_name = "size_t",
		.params_cnt = 3,
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
			}
		}
	},
	{
		.name = "crypt",
		.conv = CC_SYSTEM_V,
		.type_name = "ptr",
		.params_cnt = 2,
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
			}
		}
	},
	{
		.name = "ctermid",
		.conv = CC_SYSTEM_V,
		.type_name = "ptr",
		.params_cnt = 1,
		.params = {
			{
				.name = "s",
				.type_name = "ptr",
				.modifiers = CDM_POINTER,
				.ref_type_name = "sz",
				.direction = PDIR_OUT
			}
		}
	},
	{
		.name = "cuserid",
		.conv = CC_SYSTEM_V,
		.type_name = "ptr",
		.params_cnt = 1,
		.params = {
			{
				.name = "string",
				.type_name = "ptr",
				.modifiers = CDM_POINTER,
				.ref_type_name = "sz",
				.direction = PDIR_OUT
			}
		}
	},
	{
		.name = "dup",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 1,
		.params = {
			{
				.name = "oldfd",
				.type_name = "int",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "dup2",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 2,
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
			}
		}
	},
	{
		.name = "encrypt",
		.conv = CC_SYSTEM_V,
		.type_name = "void",
		.params_cnt = 2,
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
		.params_cnt = 2,
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
			}
		}
	},
	{
		.name = "execve",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
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
			}
		}
	},
	{
		.name = "execvp",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 2,
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
			}
		}
	},
	{
		.name = "_exit",
		.conv = CC_SYSTEM_V,
		.type_name = "void",
		.params_cnt = 1,
		.params = {
			{
				.name = "status",
				.type_name = "int",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "fchown",
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
			}
		}
	},
	{
		.name = "fchdir",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
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
		.name = "fork",
		.conv = CC_SYSTEM_V,
		.type_name = "pid_t",
		.params_cnt = 0,
		.params = {
		}
	},
	{
		.name = "fpathconf",
		.conv = CC_SYSTEM_V,
		.type_name = "long int",
		.params_cnt = 2,
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
			}
		}
	},
	{
		.name = "fsync",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
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
		.name = "ftruncate",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 2,
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
			}
		}
	},
	{
		.name = "getcwd",
		.conv = CC_SYSTEM_V,
		.type_name = "ptr",
		.params_cnt = 2,
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
			}
		}
	},
	{
		.name = "getdtablesize",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 0,
		.params = {
		}
	},
	{
		.name = "getegid",
		.conv = CC_SYSTEM_V,
		.type_name = "gid_t",
		.params_cnt = 0,
		.params = {
		}
	},
	{
		.name = "geteuid",
		.conv = CC_SYSTEM_V,
		.type_name = "uid_t",
		.params_cnt = 0,
		.params = {
		}
	},
	{
		.name = "getgid",
		.conv = CC_SYSTEM_V,
		.type_name = "gid_t",
		.params_cnt = 0,
		.params = {
		}
	},
	{
		.name = "getgroups",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 2,
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
			}
		}
	},
	{
		.name = "gethostid",
		.conv = CC_SYSTEM_V,
		.type_name = "long",
		.params_cnt = 0,
		.params = {
		}
	},
	{
		.name = "getlogin",
		.conv = CC_SYSTEM_V,
		.type_name = "ptr",
		.params_cnt = 0,
		.params = {
		}
	},
	{
		.name = "getlogin_r",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 2,
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
			}
		}
	},
	{
		.name = "getopt",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 3,
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
			}
		}
	},
	{
		.name = "getpagesize",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 0,
		.params = {
		}
	},
	{
		.name = "getpass",
		.conv = CC_SYSTEM_V,
		.type_name = "ptr",
		.params_cnt = 1,
		.params = {
			{
				.name = "prompt",
				.type_name = "ptr",
				.modifiers = CDM_POINTER | CDM_CONST,
				.ref_type_name = "sz",
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "getpgid",
		.conv = CC_SYSTEM_V,
		.type_name = "pid_t",
		.params_cnt = 1,
		.params = {
			{
				.name = "pid",
				.type_name = "pid_t",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "getpgrp",
		.conv = CC_SYSTEM_V,
		.type_name = "pid_t",
		.params_cnt = 0,
		.params = {
		}
	},
	{
		.name = "getpid",
		.conv = CC_SYSTEM_V,
		.type_name = "pid_t",
		.params_cnt = 0,
		.params = {
		}
	},
	{
		.name = "getppid",
		.conv = CC_SYSTEM_V,
		.type_name = "pid_t",
		.params_cnt = 0,
		.params = {
		}
	},
	{
		.name = "getsid",
		.conv = CC_SYSTEM_V,
		.type_name = "pid_t",
		.params_cnt = 1,
		.params = {
			{
				.name = "pid",
				.type_name = "pid_t",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "getuid",
		.conv = CC_SYSTEM_V,
		.type_name = "uid_t",
		.params_cnt = 0,
		.params = {
		}
	},
	{
		.name = "getwd",
		.conv = CC_SYSTEM_V,
		.type_name = "ptr",
		.params_cnt = 1,
		.params = {
			{
				.name = "buf",
				.type_name = "ptr",
				.modifiers = CDM_POINTER,
				.ref_type_name = "sz",
				.direction = PDIR_OUT
			}
		}
	},
	{
		.name = "isatty",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
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
		.name = "lchown",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 3,
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
			}
		}
	},
	{
		.name = "link",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 3,
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
			}
		}
	},
	{
		.name = "lockf",
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
			}
		}
	},
	{
		.name = "lseek",
		.conv = CC_SYSTEM_V,
		.type_name = "off_t",
		.params_cnt = 3,
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
			}
		}
	},
	{
		.name = "nice",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 2,
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
			}
		}
	},
	{
		.name = "pathconf",
		.conv = CC_SYSTEM_V,
		.type_name = "long int",
		.params_cnt = 2,
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
			}
		}
	},
	{
		.name = "pause",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 0,
		.params = {
		}
	},
	{
		.name = "pipe",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 1,
		.params = {
			{
				.name = "pipefd",
				.type_name = "ptr",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "pread",
		.conv = CC_SYSTEM_V,
		.type_name = "ssize_t",
		.params_cnt = 4,
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
			}
		}
	},
	{
		.name = "pthread_atfork",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 3,
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
			}
		}
	},
	{
		.name = "pwrite",
		.conv = CC_SYSTEM_V,
		.type_name = "ssize_t",
		.params_cnt = 4,
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
			}
		}
	},
	{
		.name = "read",
		.conv = CC_SYSTEM_V,
		.type_name = "ssize_t",
		.params_cnt = 3,
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
			}
		}
	},
	{
		.name = "readlink",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 3,
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
			}
		}
	},
	{
		.name = "rmdir",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 1,
		.params = {
			{
				.name = "pathname",
				.type_name = "ptr",
				.modifiers = CDM_POINTER | CDM_CONST,
				.ref_type_name = "sz",
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "sbrk",
		.conv = CC_SYSTEM_V,
		.type_name = "ptr",
		.params_cnt = 1,
		.params = {
			{
				.name = "increment",
				.type_name = "intptr_t",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "setgid",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 1,
		.params = {
			{
				.name = "gid",
				.type_name = "gid_t",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "setpgid",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 2,
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
			}
		}
	},
	{
		.name = "setpgrp",
		.conv = CC_SYSTEM_V,
		.type_name = "pid_t",
		.params_cnt = 0,
		.params = {
		}
	},
	{
		.name = "setregid",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 2,
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
			}
		}
	},
	{
		.name = "setreuid",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 2,
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
			}
		}
	},
	{
		.name = "setsid",
		.conv = CC_SYSTEM_V,
		.type_name = "pid_t",
		.params_cnt = 0,
		.params = {
		}
	},
	{
		.name = "setuid",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 1,
		.params = {
			{
				.name = "uid",
				.type_name = "uid_t",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "sleep",
		.conv = CC_SYSTEM_V,
		.type_name = "unsigned int ",
		.params_cnt = 1,
		.params = {
			{
				.name = "seconds",
				.type_name = "unsigned int",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "swab",
		.conv = CC_SYSTEM_V,
		.type_name = "void",
		.params_cnt = 3,
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
			}
		}
	},
	{
		.name = "symlink",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 2,
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
			}
		}
	},
	{
		.name = "sync",
		.conv = CC_SYSTEM_V,
		.type_name = "void",
		.params_cnt = 0,
		.params = {
		}
	},
	{
		.name = "sysconf",
		.conv = CC_SYSTEM_V,
		.type_name = "long int",
		.params_cnt = 1,
		.params = {
			{
				.name = "name",
				.type_name = "int",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "tcgetpgrp",
		.conv = CC_SYSTEM_V,
		.type_name = "pid_t",
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
		.name = "truncate",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 2,
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
			}
		}
	},
	{
		.name = "ttyname",
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
		.name = "ttyname_r",
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
			}
		}
	},
	{
		.name = "ualarm",
		.conv = CC_SYSTEM_V,
		.type_name = "useconds_t",
		.params_cnt = 2,
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
			}
		}
	},
	{
		.name = "unlink",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 1,
		.params = {
			{
				.name = "pathname",
				.type_name = "ptr",
				.modifiers = CDM_POINTER | CDM_CONST,
				.ref_type_name = "sz",
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "usleep",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 1,
		.params = {
			{
				.name = "usec",
				.type_name = "useconds_t",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "vfork",
		.conv = CC_SYSTEM_V,
		.type_name = "pid_t",
		.params_cnt = 0,
		.params = {
		}
	},
	{
		.name = "write",
		.conv = CC_SYSTEM_V,
		.type_name = "ssize_t",
		.params_cnt = 3,
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
			}
		}
	}
};
