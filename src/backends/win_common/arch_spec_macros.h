/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

#ifndef ARCH_SPEC_MACROS_H_
#define ARCH_SPEC_MACROS_H_

/*
 * PE section registration (TODO.windows/04-05).
 *
 * PE section names are 8 chars and the linker gives no
 * start/stop symbols, so the POSIX trick (section walk via
 * __start_/_section$start$) has no equivalent. Instead each
 * registry ROLE gets a short-named PE section which the linker
 * merges across translation units (the same contiguity ELF
 * gives), and section_walk.c finds it in the module's own PE
 * headers at init time. No constructors: works under MSVC and
 * MinGW alike.
 *
 *   .rtrA  actions       (__retrace_acts)
 *   .rtrF  prototypes    (__retrace_funcs)
 *   .rtrD  data types    (__retrace_dt)
 */

#define RETRACE_WIN_PE_REGISTRY 1

#include "section_walk.h"

#if defined(_MSC_VER)

__pragma(section(".rtrA", read))
__pragma(section(".rtrF", read))
__pragma(section(".rtrD", read))

/*
 * `role` must be a single string literal (".rtrA"): MSVC's
 * __declspec(allocate) rejects concatenated literals. `name` is
 * the base symbol (no brackets). The /include: anchor forces the
 * linker to keep the otherwise-unreferenced static -- without it
 * /OPT:REF strips the array and the whole section vanishes.
 */
/* external linkage: /include: resolves only public symbols */
#define retrace_win_declare_(role, type, name) \
	__pragma(comment(linker, "/include:" #name)) \
	__declspec(allocate(role)) \
	__declspec(align(1)) const type name[]

#else /* MinGW GCC / clang: COFF sections via attributes */

#define retrace_win_declare_(role, type, name) \
	static const type name[] __attribute__((used, \
		section(role), aligned(1)))

#endif

/*
 * The walkers keep the portable 4-arg call shape; the key is the
 * POSIX seg+sec spelling so core code never branches on the
 * platform.
 */
#define retrace_as_get_section_info(seg_name, sec_name, addr_ptr, \
				    size_ptr) \
	retrace_win_get_registry(seg_name sec_name, \
				 (void **)(addr_ptr), size_ptr)

#endif /* ARCH_SPEC_MACROS_H_ */
