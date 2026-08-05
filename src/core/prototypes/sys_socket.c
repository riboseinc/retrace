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
 * Network socket prototypes. Covers the POSIX sockets API used
 * by HTTP clients, DNS resolvers, and similar user-space
 * networking code. MECE with unistd.c (file I/O) and stdio.c
 * (std I/O): this file owns only socket-specific functions.
 *
 * Phase: v2.2.0 (TODO.complete/15-network-functions).
 *
 * Design notes:
 *   - Parameters use the project's type names: int for scalar,
 *     ptr for pointer (with CDM_POINTER), sz for size_t.
 *   - sockaddr* parameters are declared as `ptr` since the
 *     sockaddr types vary by address family (sockaddr_in,
 *     sockaddr_in6, sockaddr_un). Higher-level actions
 *     (sandbox) inspect the pointed-to bytes via raw reads.
 *   - ssize_t return values are declared as int (the engine
 *     treats them uniformly as 64-bit on the wire).
 */

#include "funcs.h"

retrace_func_define_prototypes(sys_socket) = {
	{
		.name = "socket",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 3,
		.params = {
			{
				.name = "domain",
				.type_name = "int",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "type",
				.type_name = "int",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "protocol",
				.type_name = "int",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "connect",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 3,
		.params = {
			{
				.name = "sockfd",
				.type_name = "int",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "addr",
				.type_name = "ptr",
				.modifiers = CDM_POINTER,
				.ref_type_name = "void",
				.direction = PDIR_IN
			},
			{
				.name = "addrlen",
				.type_name = "int",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "bind",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 3,
		.params = {
			{
				.name = "sockfd",
				.type_name = "int",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "addr",
				.type_name = "ptr",
				.modifiers = CDM_POINTER,
				.ref_type_name = "void",
				.direction = PDIR_IN
			},
			{
				.name = "addrlen",
				.type_name = "int",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "listen",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 2,
		.params = {
			{
				.name = "sockfd",
				.type_name = "int",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "backlog",
				.type_name = "int",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "accept",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 3,
		.params = {
			{
				.name = "sockfd",
				.type_name = "int",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "addr",
				.type_name = "ptr",
				.modifiers = CDM_POINTER,
				.ref_type_name = "void",
				.direction = PDIR_OUT
			},
			{
				.name = "addrlen",
				.type_name = "ptr",
				.modifiers = CDM_POINTER,
				.ref_type_name = "int",
				.direction = PDIR_INOUT
			}
		}
	},
	{
		.name = "send",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 4,
		.params = {
			{
				.name = "sockfd",
				.type_name = "int",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "buf",
				.type_name = "ptr",
				.modifiers = CDM_POINTER,
				.ref_type_name = "void",
				.direction = PDIR_IN
			},
			{
				.name = "len",
				.type_name = "sz",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "flags",
				.type_name = "int",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "recv",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 4,
		.params = {
			{
				.name = "sockfd",
				.type_name = "int",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "buf",
				.type_name = "ptr",
				.modifiers = CDM_POINTER,
				.ref_type_name = "void",
				.direction = PDIR_OUT
			},
			{
				.name = "len",
				.type_name = "sz",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "flags",
				.type_name = "int",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "sendto",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 6,
		.params = {
			{
				.name = "sockfd",
				.type_name = "int",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "buf",
				.type_name = "ptr",
				.modifiers = CDM_POINTER,
				.ref_type_name = "void",
				.direction = PDIR_IN
			},
			{
				.name = "len",
				.type_name = "sz",
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
				.name = "dest_addr",
				.type_name = "ptr",
				.modifiers = CDM_POINTER,
				.ref_type_name = "void",
				.direction = PDIR_IN
			},
			{
				.name = "addrlen",
				.type_name = "int",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "recvfrom",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 6,
		.params = {
			{
				.name = "sockfd",
				.type_name = "int",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "buf",
				.type_name = "ptr",
				.modifiers = CDM_POINTER,
				.ref_type_name = "void",
				.direction = PDIR_OUT
			},
			{
				.name = "len",
				.type_name = "sz",
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
				.name = "src_addr",
				.type_name = "ptr",
				.modifiers = CDM_POINTER,
				.ref_type_name = "void",
				.direction = PDIR_OUT
			},
			{
				.name = "addrlen",
				.type_name = "ptr",
				.modifiers = CDM_POINTER,
				.ref_type_name = "int",
				.direction = PDIR_INOUT
			}
		}
	},
	{
		.name = "setsockopt",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 4,
		.params = {
			{
				.name = "sockfd",
				.type_name = "int",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "level",
				.type_name = "int",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "optname",
				.type_name = "int",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "optval",
				.type_name = "ptr",
				.modifiers = CDM_POINTER,
				.ref_type_name = "void",
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "getsockopt",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 4,
		.params = {
			{
				.name = "sockfd",
				.type_name = "int",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "level",
				.type_name = "int",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "optname",
				.type_name = "int",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "optval",
				.type_name = "ptr",
				.modifiers = CDM_POINTER,
				.ref_type_name = "void",
				.direction = PDIR_OUT
			}
		}
	}
};
