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

#include <error.h>
#include "shim.h"

void
postcall_read_buf_fixup(struct postcall_read *call)
{
	call->args.buf.len = call->result != -1 ? call->result : 0;
	if (call->args.buf.len > 4096)
		call->args.buf.len = 4096;
}

void
precall_strncpy_src_fixup(struct precall_strncpy *call)
{
	call->args.src.len = call->args.n;
	if (call->args.src.len > 4096)
		call->args.src.len = 4096;
}

void
postcall_strncpy_fixup(struct postcall_strncpy *call)
{
	call->result.len = call->args.n;
	if (call->result.len > 4096)
		call->result.len = 4096;
}

void
postcall_strncpy_dest_fixup(struct postcall_strncpy *call)
{
	call->args.dest.len = call->args.n;
	if (call->args.dest.len > 4096)
		call->args.dest.len = 4096;
}

void
postcall_strncpy_src_fixup(struct postcall_strncpy *call)
{
	call->args.src.len = call->args.n;
	if (call->args.src.len > 4096)
		call->args.src.len = 4096;
}

void
postcall_readdir_r_entry_fixup(struct postcall_readdir_r *call)
{
	call->args.entry.len = sizeof(struct dirent);
}

void
precall_write_buf_fixup(struct precall_write *call)
{
	call->args.buf.len = call->args.count;
	if (call->args.buf.len > 4096)
		call->args.buf.len = 4096;
}

void
postcall_write_buf_fixup(struct postcall_write *call)
{
	call->args.buf.len = call->args.count;
	if (call->args.buf.len > 4096)
		call->args.buf.len = 4096;
}

void
precall_memmove_src_fixup(struct precall_memmove *call)
{
	call->args.src.len = call->args.n;
	if (call->args.src.len > 4096)
		call->args.src.len = 4096;
}

void
postcall_memmove_dest_fixup(struct postcall_memmove *call)
{
	call->args.dest.len = call->args.n;
	if (call->args.dest.len > 4096)
		call->args.dest.len = 4096;
}

void
postcall_memmove_src_fixup(struct postcall_memmove *call)
{
	call->args.src.len = call->args.n;
	if (call->args.src.len > 4096)
		call->args.src.len = 4096;
}

void
precall_memcpy_src_fixup(struct precall_memcpy *call)
{
	call->args.src.len = call->args.n;
	if (call->args.src.len > 4096)
		call->args.src.len = 4096;
}

void
postcall_memcpy_dest_fixup(struct postcall_memcpy *call)
{
	call->args.dest.len = call->args.n;
	if (call->args.dest.len > 4096)
		call->args.dest.len = 4096;
}

void
postcall_memcpy_src_fixup(struct postcall_memcpy *call)
{
	call->args.src.len = call->args.n;
	if (call->args.src.len > 4096)
		call->args.src.len = 4096;
}
