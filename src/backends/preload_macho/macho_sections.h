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
 * Section-bounds lookup for Mach-O backends.
 *
 * The previous implementation used the magic linker symbols
 * section$start$__DATA$__SEC / section$end$__DATA$__SEC via __asm().
 * That works on Apple Silicon but returns size=0 on Intel macOS,
 * causing retrace_real_impls_init to silently fail (issue #479).
 *
 * The supported replacement is getsectiondata(), which has been
 * available since macOS 10.6 (well below our 10.12 minimum) and
 * works identically on Intel and Apple Silicon. We iterate every
 * loaded image because retrace itself is loaded via
 * DYLD_INSERT_LIBRARIES and is not necessarily image[0].
 */
#ifndef PRELOAD_MACHO_MACHO_SECTIONS_H_
#define PRELOAD_MACHO_MACHO_SECTIONS_H_

#include <mach-o/getsect.h>
#include <mach-o/dyld.h>

static inline void
retrace_macho_get_section(const char *seg_name, const char *sec_name,
			  void **addr_ptr, unsigned long *size_ptr)
{
	uint32_t i;
	uint32_t count;

	*size_ptr = 0;
	*addr_ptr = NULL;

	count = _dyld_image_count();
	for (i = 0; i < count; i++) {
		const struct mach_header_64 *hdr;
		unsigned long sz = 0;
		void *a;

		hdr = (const struct mach_header_64 *)
			_dyld_get_image_header(i);
		if (hdr == NULL)
			continue;

		a = getsectiondata(hdr, seg_name, sec_name, &sz);
		if (a != NULL && sz > 0) {
			*addr_ptr = a;
			*size_ptr = sz;
			return;
		}
	}
}

#endif
