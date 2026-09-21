/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Per-arch function table for PPC64 little-endian (TODO.impl/20).
 * The common function list lives in /tmp/def-small (DRY --
 * shared by every backend); this file binds RETRACE_WRAP to the
 * ppc64 trampoline macro and includes it. The
 * inventory-conformance test enforces the whole .def on this
 * backend too (the v2.36.0 linux-aarch64 drift lesson).
 */

/* #include (not .include) so this file AND the trampoline core
 * pass through cpp together: the frame constants are #defines
 * and must fold textually into instruction immediates.
 */
#include "arch_spec_top.S"

#define RETRACE_WRAP WRAPPER_ENTRY_PPC64
#include "/tmp/def-small"
#undef RETRACE_WRAP
