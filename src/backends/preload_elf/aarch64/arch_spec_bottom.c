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
#define _GNU_SOURCE
#include <dlfcn.h>

#include "engine.h"
#include "real_impls.h"
#include "logger.h"
#include "printf_compat.h"

/*
 * AArch64 PCS frame captured by the trampoline.
 *
 * Layout MUST stay byte-for-byte in sync with the offsets defined in
 * arch_spec_top.S (see OFFS_* constants).
 *
 * Order:
 *   control fields (call_real_flag, real_impl, ret_val, ret_is_fp),
 *   saved integer arg regs  x0..x7,
 *   saved SIMD/FP arg regs   v0..v7 (each 128-bit),
 *   saved link register (x30),
 *   saved entry sp (lowest stack arg, used to walk params 9+).
 *
 * Total size: 256 bytes (16-byte aligned for AAPCS call boundaries).
 */
struct WrapperAArch64Frame {
	/* control fields -- written by the intercept logic, read by asm */
	long call_real_flag;
	void *real_impl;
	union {
		long ret_val_long;        /* integer/pointer return */
		unsigned char ret_val_fp[16]; /* full 128-bit FP/SIMD return */
	} ret_val;
	int ret_is_fp;
	int _pad0;

	/* saved integer argument registers x0..x7, in ascending order */
	unsigned long real_x0;
	unsigned long real_x1;
	unsigned long real_x2;
	unsigned long real_x3;
	unsigned long real_x4;
	unsigned long real_x5;
	unsigned long real_x6;
	unsigned long real_x7;

	/* Explicit padding so the SIMD region lands on a 16-byte boundary
	 * (matching the offset constants in arch_spec_top.S). The C struct
	 * layout already pads here naturally, but documenting it keeps the
	 * asm and C in lockstep.
	 */
	unsigned long _pad_simd_align;

	/* saved SIMD/FP argument registers v0..v7 (each 16 bytes) */
	unsigned char real_v0[16];
	unsigned char real_v1[16];
	unsigned char real_v2[16];
	unsigned char real_v3[16];
	unsigned char real_v4[16];
	unsigned char real_v5[16];
	unsigned char real_v6[16];
	unsigned char real_v7[16];

	/* link register at entry */
	unsigned long real_lr;

	/* sp at entry; stack args (params 9+) live at orig_sp + 8*(i-8) */
	unsigned long orig_sp;
};

/*
 * Helper: read the i-th integer/pointer argument out of the frame.
 * AArch64 puts the first 8 integer/pointer args in x0..x7 and the rest
 * on the stack starting at orig_sp.
 */
static unsigned long wrapper_frame_get_arg(
	const struct WrapperAArch64Frame *frame, int idx)
{
	switch (idx) {
	case 0: return frame->real_x0;
	case 1: return frame->real_x1;
	case 2: return frame->real_x2;
	case 3: return frame->real_x3;
	case 4: return frame->real_x4;
	case 5: return frame->real_x5;
	case 6: return frame->real_x6;
	case 7: return frame->real_x7;
	default:
		/* stack args begin at orig_sp + 8*(idx-8); orig_sp already
		 * points at the lowest stack arg, so params 9+ live above it
		 */
		return *(unsigned long *)(frame->orig_sp + sizeof(void *) * (idx - 8));
	}
}

long retrace_as_call_real(const void *real_impl,
	const struct FuncParam params[],
	int params_cnt)
{
	return retrace_as_call_real_dispatch(real_impl, params, params_cnt);
}

void retrace_as_abort(void *arch_spec_ctx, long ret_val)
{
	struct WrapperAArch64Frame *frame = arch_spec_ctx;

	frame->call_real_flag = 0;
	frame->ret_val.ret_val_long = ret_val;
	frame->ret_is_fp = 0;
}

void retrace_as_sched_real(void *arch_spec_ctx, void *real_impl)
{
	struct WrapperAArch64Frame *frame = arch_spec_ctx;

	frame->call_real_flag = 1;
	frame->real_impl = real_impl;
}

int retrace_as_setup_params(
	void *arch_spec_ctx,
	const struct FuncPrototype *proto,
	struct FuncParam params[],
	int *params_cnt)
{
	int i;
	int printf_params;
	int param_idx;
	const struct WrapperAArch64Frame *frame = arch_spec_ctx;
	const struct DataType *dt;
	int *types;

	/* check whether there is enough space for prototyped params */
	if (*params_cnt < proto->params_cnt) {
		log_err("too many prototyped params for '%s', no space for %d more",
			proto->name,
			(*params_cnt - proto->params_cnt) * -1);
		return 0;
	}

	for (param_idx = 0; param_idx != proto->params_cnt; param_idx++) {
		/* reset param */
		retrace_real_impls.memset(&params[param_idx].param_meta,
				0,
				sizeof(struct ParamMeta));

		/* setup meta */
		retrace_real_impls.memcpy(&params[param_idx].param_meta,
				&proto->params[param_idx],
				sizeof(struct ParamMeta));

		/* setup datatype */
		params[param_idx].data_type =
			retrace_datatype_get(proto->params[param_idx].type_name);

		/* setup value: AArch64 puts integer/pointer args in x0..x7,
		 * anything beyond goes on the stack starting at orig_sp
		 */
		params[param_idx].val =
			(long) wrapper_frame_get_arg(frame, param_idx);
	}

	/* set up varargs params */
	if (proto->fmt == FAT_NOVARARGS) {
		*params_cnt = proto->params_cnt;
		return 1;
	}

	if ((proto->fmt != FAT_PRINTF) &&
		(proto->fmt != FAT_SCANF)) {

		log_err("varargs format '%d' is not supported for func '%s'",
			proto->fmt, proto->name);
		return 0;
	}

	printf_params =
		parse_printf_format(
			(const char *) params[proto->fmt_param_idx].val,
			0,
			NULL);

	if (!printf_params) {
		*params_cnt = proto->params_cnt;
		return 1;
	}

	types = (int *)
		retrace_real_impls.malloc(sizeof(int) * printf_params);

	printf_params =
		parse_printf_format(
			(const char *) params[proto->fmt_param_idx].val,
			printf_params,
			types);

	for (i = 0; i != printf_params; i++, param_idx++) {
		/* prep param meta */
		dt = retrace_datatype_printf_to_dt(types[i]);

		/* reset param */
		retrace_real_impls.memset(&params[param_idx].param_meta,
			0,
			sizeof(struct ParamMeta));

		/* setup meta */
		retrace_real_impls.real_snprintf(params[param_idx].param_meta.name,
			sizeof(params[param_idx].param_meta.name),
			"vararg%02d",
			i);

		retrace_real_impls.strcpy(params[param_idx].param_meta.type_name,
			dt->name);

		params[param_idx].param_meta.modifiers = CDM_NOMOD;
		params[param_idx].param_meta.direction = PDIR_IN;

		if ((types[i] & ~PA_FLAG_MASK) == PA_STRING) {
			params[param_idx].param_meta.modifiers |= CDM_POINTER;
			retrace_real_impls.strcpy(
				params[param_idx].param_meta.ref_type_name,
				"sz");
		}

		params[param_idx].data_type = dt;

		params[param_idx].val =
			(long) wrapper_frame_get_arg(frame, param_idx);
	}

	*params_cnt = param_idx;
	return 1;
}

void retrace_as_intercept_done(void *arch_spec_ctx, long ret_val)
{
	struct WrapperAArch64Frame *frame = arch_spec_ctx;

	frame->ret_val.ret_val_long = ret_val;
	frame->ret_is_fp = 0;
	frame->call_real_flag = 0;
}

void retrace_as_cancel_sched_real(void *arch_spec_ctx)
{
	((struct WrapperAArch64Frame *) arch_spec_ctx)->call_real_flag = 0;
}

void retrace_as_set_ret_val(void *arch_spec_ctx, long ret_val)
{
	struct WrapperAArch64Frame *frame = arch_spec_ctx;

	frame->ret_val.ret_val_long = ret_val;
	frame->ret_is_fp = 0;
}

int retrace_as_init(void)
{
	return 0;
}

int retrace_as_init_late(void)
{
	return 0;
}

extern void *_dl_sym(void *handle, const char *symbol, const void *rtraddr);

/*
 * Resolve the real (next-in-search-order) implementation of a libc symbol.
 *
 * Modern glibc (>= 2.34) no longer exports the GLIBC_PRIVATE _dl_sym symbol
 * from ld.so. We use dlsym(RTLD_NEXT, ...) instead. Since dlsym is NOT
 * intercepted on aarch64 Linux (see funcs_symbols.S), there is no recursion
 * risk when resolving symbols through the public dlsym path.
 *
 * On older glibc (< 2.34) that still exports _dl_sym, the x86_64 backend
 * uses that instead to bypass the PLT. The aarch64 backend targets modern
 * glibc (>= 2.34) where _dl_sym is unavailable.
 */
void *retrace_as_get_real_safe(const char *real_impl)
{
	return dlsym(RTLD_NEXT, real_impl);
}
