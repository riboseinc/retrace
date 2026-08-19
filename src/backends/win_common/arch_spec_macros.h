/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

#ifndef ARCH_SPEC_MACROS_H_
#define ARCH_SPEC_MACROS_H_

/*
 * PE section registration (TODO.windows/04).
 *
 * PE section names are 8 chars and the linker gives no
 * start/stop symbols, so the POSIX trick (section walk via
 * __start_/_section$start$) has no direct equivalent. Staging:
 *
 *   item 04 (this header): registry variables land in one
 *   shared short section (.retrc) so the core compiles and
 *   links under MSVC and MinGW; the walkers report EMPTY
 *   registries (the engine and logger run; nothing is
 *   registered yet -- nothing is hooked either).
 *
 *   item 05 (first wrapper): a role-aware constructor registry
 *   (each TU appends {data, size, role} at load) replaces the
 *   section walk, wired with the first live hook so CI proves
 *   real registration, not an empty stub.
 */

#if defined(_MSC_VER)

__pragma(section(".retrc", read))

#define retrace_as_define_var_in_sec(type, name, seg_name, sec_name) \
	__pragma(comment(user, seg_name sec_name)) \
	__declspec(allocate(".retrc")) __declspec(align(1)) static type name

/*
 * MinGW GCC / clang: COFF supports long section names, so the
 * ELF spelling works verbatim on PE.
 */
#else

#define retrace_as_define_var_in_sec(type, name, seg_name, sec_name) \
	static type name __attribute__((used, \
		section(seg_name sec_name), aligned(1)))

#endif

/*
 * The walkers keep the portable 4-arg call shape. PE has no
 * start/stop symbols, so the walk itself reports EMPTY this
 * slice (staging note at the top).
 */
#define retrace_as_get_section_info(seg_name, sec_name, addr_ptr, size_ptr) \
do { \
	(void)sizeof(seg_name sec_name); \
	*(addr_ptr) = (void *)0; \
	*(size_ptr) = 0; \
} while (0)

#endif /* ARCH_SPEC_MACROS_H_ */
