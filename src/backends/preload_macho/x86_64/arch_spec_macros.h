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

#ifndef ARCH_SPEC_MACROS_H_
#define ARCH_SPEC_MACROS_H_

#include <mach-o/getsect.h>
#include <mach-o/dyld.h>

#define retrace_as_define_var_in_sec(type, name, seg_name, sec_name) \
	static type name __attribute__((used, section(seg_name","sec_name)))

/*
 * Resolve section bounds via getsectiondata() instead of the
 * section$start$ / section$end$ magic linker symbols.
 *
 * On x86_64 macOS, the magic symbols resolve to the SAME address for
 * start and end (size = 0) when accessed from inside a dylib that
 * didn't itself define the section via the magic-symbol mechanism.
 * The result is that retrace_as_get_real_safe() walks zero entries
 * and returns NULL for every libc symbol; init fails on the first
 * dlopen lookup, retrace_inited stays 0, and the destructor's
 * retrace_real_impls.printf(...) jumps to NULL (issue #452).
 *
 * getsectiondata() is the official API and works correctly on both
 * Apple Silicon and Intel. We scan loaded images for the one that
 * actually defines the section (libretrace.dylib) so the lookup is
 * correct regardless of which image calls us.
 */
static inline void retrace_as_get_section_data(const char *seg_name,
	const char *sec_name, void **addr_ptr, unsigned long *size_ptr)
{
	uint32_t i;

	*size_ptr = 0;
	*addr_ptr = NULL;

	for (i = 0; i < _dyld_image_count(); i++) {
		const struct mach_header_64 *hdr =
			(const struct mach_header_64 *)
				_dyld_get_image_header(i);
		unsigned long sz = 0;
		void *a = getsectiondata(hdr, seg_name, sec_name, &sz);

		if (a != NULL && sz > 0) {
			*addr_ptr = a;
			*size_ptr = sz;
			return;
		}
	}
}

/*
 * Wraps retrace_as_get_section_data with a void* cast so callers can
 * pass any T** for addr_ptr without a type-mismatch warning (C is
 * strict about T** vs void** even though it accepts T* vs void*).
 */
#define retrace_as_get_section_info(seg_name, sec_name, addr_ptr, size_ptr) \
	do { \
		void *_addr; \
		unsigned long _size; \
		retrace_as_get_section_data(seg_name, sec_name, &_addr, &_size); \
		*(addr_ptr) = _addr; \
		*(size_ptr) = _size; \
	} while (0)

#endif
