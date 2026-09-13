/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * OpenSSL prototypes (TODO.impl/13): the TLS content lane.
 * SSL_write/SSL_read carry the PLAINTEXT buffers (the wire
 * bytes below TLS are what the evidence plane summarizes);
 * SSL_CTX_new is the keylog injection point (the real ctx's
 * callback slot). MECE with sys_socket.c (raw sockets) and
 * stdio.c: this file owns only the OpenSSL surface.
 */

#include "funcs.h"

retrace_func_define_prototypes(openssl) = {
	{
		.name = "SSL_CTX_new",
		.conv = CC_SYSTEM_V,
		.type_name = "ptr",
		.params_cnt = 1,
		.params = {
			{
				.name = "method",
				.type_name = "ptr",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "SSL_read",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 3,
		.params = {
			{
				.name = "ssl",
				.type_name = "ptr",
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
				.name = "num",
				.type_name = "sz",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "SSL_read_ex",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 4,
		.params = {
			{
				.name = "ssl",
				.type_name = "ptr",
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
				.name = "num",
				.type_name = "sz",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "readbytes",
				.type_name = "ptr",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_OUT
			}
		}
	},
	{
		.name = "SSL_write",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 3,
		.params = {
			{
				.name = "ssl",
				.type_name = "ptr",
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
				.name = "num",
				.type_name = "sz",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			}
		}
	},
	{
		.name = "SSL_write_ex",
		.conv = CC_SYSTEM_V,
		.type_name = "int",
		.params_cnt = 4,
		.params = {
			{
				.name = "ssl",
				.type_name = "ptr",
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
				.name = "num",
				.type_name = "sz",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_IN
			},
			{
				.name = "written",
				.type_name = "ptr",
				.modifiers = CDM_NOMOD,
				.direction = PDIR_OUT
			}
		}
	}
};
