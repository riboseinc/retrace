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
 * PPC64 ELFv2 frame captured by the trampoline (TODO.impl/20).
 *
 * Layout MUST stay byte-for-byte in sync with the offsets defined in
 * arch_spec_top.S (see OFFS_* constants).
 *
 * Order:
 *   back chain (== entry sp: stdu stores it at [r1] for us),
 *   control fields (call_real_flag, real_impl, ret_val, ret_is_fp),
 *   saved integer arg regs  r3..r10,
 *   saved FP arg regs       f1..f8 (64-bit doubles),
 *   saved link register (LR).
 *
 * Total size: 192 bytes (16-byte aligned).
 */
struct WrapperPpc64Frame {
	/* sp at entry (the back-chain word stdu stores);
	 * ELFv2 stack args (params 9+) live in the caller's
	 * parameter save area, which begins at orig_sp + 32
	 */
	unsigned long orig_sp;
	/* 8: entry-mode flag the asm writes (1 = global entry --
	 * the tail restores the caller's TOC from slot 24; 0 =
	 * local entry -- r2 is already ours and must pass
	 * through untouched). Our own .so's internal calls bind
	 * to the LOCAL entry, where r11/r12 are garbage: reading
	 * the caller TOC unconditionally there fed trash into r2
	 * and crashed every GOT load downstream (the parson
	 * json_value_init_string TOC fault, TODO.impl/20 r20).
	 */
	int entry_global;
	int _pad_cr;
	unsigned long _abi_lr;     /* 16: CALLEE-owned LR slot -- do not use */
	unsigned long _pad_toc;    /* 24: ELFv2 TOC-save slot; asm stores caller TOC */

	/* control fields -- written by the intercept logic, read by asm */
	long call_real_flag;
	void *real_impl;
	union {
		long ret_val_long;    /* integer/pointer return */
		double ret_val_fp;    /* FP return (double bits) */
	} ret_val;
	int ret_is_fp;
	int _pad0;

	/* saved integer argument registers r3..r10 in ascending order */
	unsigned long real_r3;
	unsigned long real_r4;
	unsigned long real_r5;
	unsigned long real_r6;
	unsigned long real_r7;
	unsigned long real_r8;
	unsigned long real_r9;
	unsigned long real_r10;

	/* saved FP argument registers f1..f8 (64-bit doubles) */
	unsigned long real_f1;
	unsigned long real_f2;
	unsigned long real_f3;
	unsigned long real_f4;
	unsigned long real_f5;
	unsigned long real_f6;
	unsigned long real_f7;
	unsigned long real_f8;

	/* original caller's LR -- private, past the ABI-volatile zone */
	unsigned long saved_lr;
	unsigned long _tail_pad;   /* keeps sizeof == 208 (16-aligned) */
};

#include <stddef.h>
_Static_assert(offsetof(struct WrapperPpc64Frame, entry_global) == 8,
	"ppc64 frame: entry-mode flag rides the CR-save slot");
_Static_assert(offsetof(struct WrapperPpc64Frame, _abi_lr) == 16,
	"ppc64 frame: SP+16 is the callee-owned ABI LR slot");
_Static_assert(offsetof(struct WrapperPpc64Frame, call_real_flag) == 32,
	"ppc64 frame: control fields must start at 32");
_Static_assert(offsetof(struct WrapperPpc64Frame, real_r3) == 64,
	"ppc64 frame: r3 must start at 64");
_Static_assert(offsetof(struct WrapperPpc64Frame, real_f1) == 128,
	"ppc64 frame: f1 must start at 128");
_Static_assert(offsetof(struct WrapperPpc64Frame, saved_lr) == 192,
	"ppc64 frame: saved caller LR must sit past the ABI zone");
_Static_assert(sizeof(struct WrapperPpc64Frame) == 208,
	"ppc64 frame: total must stay 208 bytes");

/*
 * Helper: read the i-th integer/pointer argument out of the frame.
 * ELFv2 puts the first 8 integer/pointer args in r3..r10; beyond
 * that, the caller's parameter save area at orig_sp + 32.
 */
static unsigned long wrapper_frame_get_arg(
	const struct WrapperPpc64Frame *frame, int idx)
{
	switch (idx) {
	case 0: return frame->real_r3;
	case 1: return frame->real_r4;
	case 2: return frame->real_r5;
	case 3: return frame->real_r6;
	case 4: return frame->real_r7;
	case 5: return frame->real_r8;
	case 6: return frame->real_r9;
	case 7: return frame->real_r10;
	default:
		/* the ELFv2 parameter save area starts at caller_sp+32
		 * (back chain, CR save, LR save, TOC save precede it)
		 */
		return *(unsigned long *)(frame->orig_sp + 32 +
			sizeof(void *) * (idx - 8));
	}
}

static intptr_t retrace_as_trampoline_call_real(
	void *arch_spec_ctx, const void *real_impl,
	const struct FuncParam params[],
	int params_cnt)
{
	(void) arch_spec_ctx;
	return retrace_as_call_real_dispatch(real_impl, params, params_cnt);
}

void retrace_as_abort(void *arch_spec_ctx, long ret_val)
{
	struct WrapperPpc64Frame *frame = arch_spec_ctx;

	frame->call_real_flag = 0;
	frame->ret_val.ret_val_long = ret_val;
	frame->ret_is_fp = 0;
}

static void retrace_as_trampoline_sched_real(void *arch_spec_ctx, void *real_impl)
{
	struct WrapperPpc64Frame *frame = arch_spec_ctx;

	frame->call_real_flag = 1;
	frame->real_impl = real_impl;
}

static int retrace_as_trampoline_setup_params(
	void *arch_spec_ctx,
	const struct FuncPrototype *proto,
	struct FuncParam params[],
	int *params_cnt)
{
	int i;
	int printf_params;
	int param_idx;
	const struct WrapperPpc64Frame *frame = arch_spec_ctx;
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

		/* setup value: ELFv2 puts integer/pointer args in
		 * r3..r10; beyond that, the caller's parameter save
		 * area at orig_sp + 32
		 */
		params[param_idx].val =
			(intptr_t) wrapper_frame_get_arg(frame, param_idx);
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
			(intptr_t) wrapper_frame_get_arg(frame, param_idx);
	}

	*params_cnt = param_idx;
	return 1;
}

void retrace_as_intercept_done(void *arch_spec_ctx, long ret_val)
{
	struct WrapperPpc64Frame *frame = arch_spec_ctx;

	frame->ret_val.ret_val_long = ret_val;
	frame->ret_is_fp = 0;
	frame->call_real_flag = 0;
}

static void retrace_as_trampoline_cancel_sched_real(void *arch_spec_ctx)
{
	((struct WrapperPpc64Frame *) arch_spec_ctx)->call_real_flag = 0;
}

static void retrace_as_trampoline_set_ret_val(void *arch_spec_ctx, intptr_t ret_val)
{
	struct WrapperPpc64Frame *frame = arch_spec_ctx;

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

#include "real_linkmap.h"

/*
 * Resolve the real (next-in-search-order) implementation of a libc symbol.
 *
 * Modern glibc (>= 2.34) no longer exports the GLIBC_PRIVATE _dl_sym symbol
 * from ld.so. We use dlsym(RTLD_NEXT, ...) instead. Since dlsym is NOT
 * intercepted on ppc64le Linux (see funcs_symbols.S), there is no recursion
 * risk when resolving symbols through the public dlsym path.
 *
 * The RTLD_NEXT law (TODO.impl/13): dlsym from a preloaded library only
 * searches its own dependency chain -- host-loaded providers are invisible,
 * so this falls back to the link-map walk (real_linkmap.c).
 */
void *retrace_as_get_real_safe(const char *real_impl)
{
	void *p = dlsym(RTLD_NEXT, real_impl);

	/*
	 * Pointer sanity at the resolver boundary: a real
	 * implementation lives in a mapped module, never below the
	 * mmap floor. Under qemu-ppc64le an RTLD_NEXT lookup run
	 * from inside a constructor has been observed to return a
	 * low garbage value (0x22325c); dispatching it meant the
	 * trampoline tail jumped straight into unmapped memory.
	 * Treat such results as failures and fall through to the
	 * per-handle link-map walk (a different, handle-based
	 * loader path).
	 */
	if (p != NULL && (unsigned long) p >= 0x100000000UL)
		return p;
	return retrace_as_real_from_linkmap(real_impl);
}

/*
 * The default as-ops table (ADR-0016): the trampoline frame.
 * Published for the shared dispatcher in as_ops.c; the engine
 * reaches it through retrace_as_ops_get() when no lane override
 * is installed.
 */
const struct retrace_as_ops retrace_as_ops_default = {
	.sched_real = retrace_as_trampoline_sched_real,
	.cancel_sched_real = retrace_as_trampoline_cancel_sched_real,
	.set_ret_val = retrace_as_trampoline_set_ret_val,
	.setup_params = retrace_as_trampoline_setup_params,
	.call_real = retrace_as_trampoline_call_real
};
