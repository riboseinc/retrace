/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Per-arch function table for MIPS n64 (TODO.impl/19). The
 * common function list lives in funcs_symbols.def (DRY --
 * shared by every backend); this file binds RETRACE_WRAP to
 * the MIPS n64 trampoline macro and includes it. The
 * inventory-conformance test enforces the whole .def on this
 * backend too (the v2.36.0 linux-aarch64 drift lesson).
 */

/* #include (not .include) so this file AND the trampoline
 * core pass through cpp together: the frame constants are
 * #defines, and this gas (2.42 mips) folds cpp literals but
 * not `.set` symbols into instruction immediates.
 */
#include "arch_spec_top.S"

#define RETRACE_WRAP WRAPPER_ENTRY_MIPS64
#include "funcs_symbols.def"
#undef RETRACE_WRAP

	/* The engine's entry address as link-time data: the
	 * trampoline loads it PC-relatively (the same-section
	 * local-delta %hi/%lo pattern the name strings use). A
	 * hidden absolute data word -- NOT a GOT form: ld rejects
	 * _gp_disp from an ASM TU, and a cross-TU delta cannot be
	 * folded. The pointee is hidden (engine.h), so the word
	 * resolves statically with no dynamic relocation.
	 */
	.text
	.align  3
retrace_engine_wrapper_addr:
	.dword  retrace_engine_wrapper
