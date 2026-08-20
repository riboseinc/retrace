/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

#ifndef RETRACE_BACKENDS_WIN_COMMON_SECTION_WALK_H_
#define RETRACE_BACKENDS_WIN_COMMON_SECTION_WALK_H_

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * PE-section registry lookup (TODO.windows/05).
 *
 * PE has no linker-synthesized __start_/__stop_ symbols and MSVC
 * has no constructors, so the POSIX section walk cannot work
 * verbatim. Instead every registry array is allocated into a
 * short-named PE section (8-char COFF limit) which the LINKER
 * merges across translation units -- the same contiguity
 * guarantee ELF gives:
 *
 *   role              key (POSIX spelling)    PE section
 *   ----------------  ----------------------  ----------
 *   actions           "__DATA__retrace_acts"  .rtrA
 *   prototypes        "__DATA__retrace_funcs" .rtrF
 *   data types        "__DATA__retrace_dt"    .rtrD
 *
 * retrace_win_section_lookup() maps the POSIX key to the PE
 * section, walks the calling module's own PE headers, and
 * returns the merged array. Called from the walker macros at
 * init time only (no per-call cost, no reentrancy exposure).
 *
 * Returns 1 and fills *addr/*size on success; 0 when the module
 * or section was not found (empty registry -- the walker sees
 * zero entries, exactly like an absent ELF section).
 */
int retrace_win_section_lookup(const char *key,
			       void **addr,
			       unsigned long *size);

/*
 * Compacting lookup: returns a DENSE heap copy of the role's
 * registry (linker padding between contributions skipped), so
 * the POSIX walkers see one contiguous array. elem_size is the
 * role's element size; the result is cached per role.
 */
int retrace_win_registry_compact(const char *key, void **addr,
				 unsigned long *size, size_t elem_size);

/*
 * Walker entry point (maps the portable get_section_info):
 * compacting lookup keyed by the POSIX seg+sec spelling, with
 * the role's element size resolved internally.
 */
int retrace_win_get_registry(const char *key, void **addr,
			     unsigned long *size);

/* diagnostics: print every PE section of the calling module */
void retrace_win_dump_sections(void);

#ifdef __cplusplus
}
#endif

#endif /* RETRACE_BACKENDS_WIN_COMMON_SECTION_WALK_H_ */
