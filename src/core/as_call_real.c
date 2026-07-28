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
 * Shared implementation of retrace_as_call_real.
 *
 * Per-backend arch_spec_bottom.c delegates here. The hand-written asm
 * version was brittle and segfaulted on modern dynamic linkers
 * (glibc >= 2.34, macOS, BSDs); plain C function pointer calls let
 * the compiler handle the ABI and work across all SysV-ABI arches
 * (x86_64, aarch64). Supports 0..6 args; the rare >6-arg case is
 * currently not implemented (no known libc symbol needs it).
 */

#include "real_impls.h"
#include "engine.h"

long retrace_as_call_real_dispatch(const void *real_impl,
				   const struct FuncParam params[],
				   int params_cnt)
{
	long *vals;
	long ret_val;
	int i;

	vals = (long *)retrace_real_impls.malloc(sizeof(long) * params_cnt);
	if (vals == NULL && params_cnt > 0)
		return -1;

	for (i = 0; i < params_cnt; i++)
		vals[i] = params[i].val;

	switch (params_cnt) {
	case 0: {
		typedef long (*fn_t)(void);

		ret_val = ((fn_t)real_impl)();
		break;
	}
	case 1: {
		typedef long (*fn_t)(long);

		ret_val = ((fn_t)real_impl)(vals[0]);
		break;
	}
	case 2: {
		typedef long (*fn_t)(long, long);

		ret_val = ((fn_t)real_impl)(vals[0], vals[1]);
		break;
	}
	case 3: {
		typedef long (*fn_t)(long, long, long);

		ret_val = ((fn_t)real_impl)(vals[0], vals[1], vals[2]);
		break;
	}
	case 4: {
		typedef long (*fn_t)(long, long, long, long);

		ret_val = ((fn_t)real_impl)(vals[0], vals[1], vals[2],
					    vals[3]);
		break;
	}
	case 5: {
		typedef long (*fn_t)(long, long, long, long, long);

		ret_val = ((fn_t)real_impl)(vals[0], vals[1], vals[2],
					    vals[3], vals[4]);
		break;
	}
	case 6: {
		typedef long (*fn_t)(long, long, long, long, long, long);

		ret_val = ((fn_t)real_impl)(vals[0], vals[1], vals[2],
					    vals[3], vals[4], vals[5]);
		break;
	}
	default:
		ret_val = -1;
		break;
	}

	retrace_real_impls.free(vals);
	return ret_val;
}

/*
 * Variadic-aware dispatch. Used by ia_call_real for functions with
 * proto->fmt != FAT_NOVARARGS (currently printf).
 *
 * Casting real_impl to a variadic-typed function pointer makes the
 * compiler emit the correct variadic ABI for the host arch:
 *
 *   - Apple AArch64: variadic args pushed on the stack (NOT in x1..x7)
 *   - Linux/BSD AArch64 (AAPCS64): variadic args in x1..x7, then stack
 *   - x86-64 (Sys V / Darwin x86-64): variadic args in rsi..r9, then stack
 *
 * real_impl is a void* with the static type lost; the variadic typedef
 * restores the "..." info that the compiler needs at the call site.
 * The callee (real printf, real fprintf, etc.) reads variadic args from
 * wherever its compiler put them per the same ABI.
 *
 * named_count is proto->params_cnt -- the number of NAMED arguments the
 * prototype declares (printf: 1 for fmt; fprintf-style: 2 for stream+fmt).
 */
long retrace_as_call_real_variadic(const void *real_impl,
				   const struct FuncParam params[],
				   int params_cnt,
				   int named_count)
{
	long *vals;
	long ret_val;
	int i;

	if (named_count > params_cnt)
		named_count = params_cnt;

	vals = (long *)retrace_real_impls.malloc(sizeof(long) * params_cnt);
	if (vals == NULL && params_cnt > 0)
		return -1;

	for (i = 0; i < params_cnt; i++)
		vals[i] = params[i].val;

	/* The typedef has exactly `named_count` named parameters followed
	 * by `...`; everything passed beyond named_count is variadic. The
	 * compiler emits the host arch's variadic ABI for the call:
	 *
	 *   Apple AArch64    -> variadic args pushed to stack
	 *   AAPCS64 ARM64    -> variadic args in regs then stack
	 *   x86-64 Sys V     -> variadic args in regs then stack
	 *
	 * If params_cnt == named_count there are no variadic args to pass;
	 * the same variadic typedef still compiles and the compiler emits a
	 * regular call.
	 */
	switch (named_count) {
	case 1: {
		typedef long (*fn_t)(long, ...);

		switch (params_cnt) {
		case 0:
			/* nothing to call */
			ret_val = -1;
			break;
		case 1:
			ret_val = ((fn_t) real_impl)(vals[0]);
			break;
		case 2:
			ret_val = ((fn_t) real_impl)(vals[0], vals[1]);
			break;
		case 3:
			ret_val = ((fn_t) real_impl)(
				vals[0], vals[1], vals[2]);
			break;
		case 4:
			ret_val = ((fn_t) real_impl)(vals[0], vals[1],
				vals[2], vals[3]);
			break;
		case 5:
			ret_val = ((fn_t) real_impl)(vals[0], vals[1],
				vals[2], vals[3], vals[4]);
			break;
		case 6:
			ret_val = ((fn_t) real_impl)(vals[0], vals[1],
				vals[2], vals[3], vals[4], vals[5]);
			break;
		default:
			ret_val = -1;
			break;
		}
		break;
	}

	case 2: {
		typedef long (*fn_t)(long, long, ...);

		switch (params_cnt) {
		case 0:
		case 1:
			ret_val = -1;
			break;
		case 2:
			ret_val = ((fn_t) real_impl)(vals[0], vals[1]);
			break;
		case 3:
			ret_val = ((fn_t) real_impl)(
				vals[0], vals[1], vals[2]);
			break;
		case 4:
			ret_val = ((fn_t) real_impl)(vals[0], vals[1],
				vals[2], vals[3]);
			break;
		case 5:
			ret_val = ((fn_t) real_impl)(vals[0], vals[1],
				vals[2], vals[3], vals[4]);
			break;
		case 6:
			ret_val = ((fn_t) real_impl)(vals[0], vals[1],
				vals[2], vals[3], vals[4], vals[5]);
			break;
		default:
			ret_val = -1;
			break;
		}
		break;
	}

	default:
		/* named_count out of range; should not happen for printf
		 * (1) or fprintf-style (2) prototypes we currently model.
		 */
		ret_val = -1;
		break;
	}

	retrace_real_impls.free(vals);
	return ret_val;
}
