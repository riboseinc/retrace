/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

#ifndef RETRACE_PROFILER_CAPABILITY_H_
#define RETRACE_PROFILER_CAPABILITY_H_

#include <stddef.h>

/*
 * Static capability scan (TODO.windows/08): what a binary CAN
 * do even if a profiling run never executed it. Evidence for
 * risk, not proof of execution:
 *
 *   - raw syscall gadgets in executable code: x86-64 "0f 05",
 *     i386 "cd 80", arm64 "svc #0" (d4 00 00 01). Hand-rolled
 *     syscall() wrappers and static-libc loops show up here.
 *   - PE: ntdll.dll imports (NtCreateFile, LdrLoadDll, ...) --
 *     direct kernel-layer calls that skip ucrt entirely.
 *
 * Executable code only (ELF PT_LOAD exec segments, Mach-O
 * __TEXT sections, PE sections with IMAGE_SCN_MEM_EXECUTE):
 * data sections are excluded so the gadget counts are not
 * diluted by embedded blobs. Counts are CAPABILITY evidence.
 */

struct ProfCapability {
	size_t syscall_gadgets;   /* exec-segment matches */
	int has_syscall_gadget;   /* gadget count > 0 */
	size_t ntdll_imports;     /* PE: imports from ntdll */
	char ntdll_names[16][24]; /* first N imported names */
};

void prof_capability_init(struct ProfCapability *c);

/*
 * Scan the binary at path (ELF, Mach-O, or PE; format detected
 * by magic). Returns 0 on success (unrecognized formats leave
 * the capability struct zeroed and return 0 -- nothing scanned,
 * nothing claimed), -1 on I/O error.
 */
int prof_capability_scan(const char *path, struct ProfCapability *c);

#endif /* RETRACE_PROFILER_CAPABILITY_H_ */
