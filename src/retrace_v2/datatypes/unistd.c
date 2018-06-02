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

#include "data_types.h"
#include "basic.h"

retrace_datatype_define_prototypes(unistd) = {
	{
		.name = "useconds_t",
		.struct_members[0] = {.name = ""},
		.to_sz = retrace_ulong_to_sz,
		.get_sz_size = retrace_ulong_get_sz_size,
		.to_size_t = retrace_ulong_to_size_t,
		.get_size = retrace_ulong_get_size
	},
	{
		.name = "gid_t",
		.struct_members[0] = {.name = ""},
		.to_sz = retrace_uint_to_sz,
		.get_sz_size = retrace_uint_get_sz_size,
		.to_size_t = retrace_uint_to_size_t,
		.get_size = retrace_uint_get_size
	},
	{
		.name = "pid_t",
		.struct_members[0] = {.name = ""},
		.to_sz = retrace_uint_to_sz,
		.get_sz_size = retrace_uint_get_sz_size,
		.to_size_t = retrace_uint_to_size_t,
		.get_size = retrace_uint_get_size
	},
	{
		.name = "uid_t",
		.struct_members[0] = {.name = ""},
		.to_sz = retrace_uint_to_sz,
		.get_sz_size = retrace_uint_get_sz_size,
		.to_size_t = retrace_uint_to_size_t,
		.get_size = retrace_uint_get_size
	},
	{
		.name = "off_t",
		.struct_members[0] = {.name = ""},
		/* FIXME: This is probably a bug, need to define spec funcs */
		.to_sz = retrace_int_to_sz,
		.get_sz_size = retrace_int_get_sz_size,
		.to_size_t = retrace_int_to_size_t,
		.get_size = retrace_int_get_size
	}
};
