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

#include <limits.h>
#include <printf.h>
#include <pthread.h>
#include <dlfcn.h>

#include "engine.h"
#include "real_impls.h"
#include "logger.h"

/* Mach-O builds include arch_spec_macros.h via src/backends/arch_spec.h */
#include "arch_spec_macros.h"

/*
 * AArch64 PCS frame captured by the trampoline. Layout MUST stay in sync
 * with the offsets defined in arch_spec_top.S (see OFFS_* constants).
 *
 * Total: 256 bytes (16-byte aligned for AAPCS call boundaries).
 */
struct WrapperAArch64Frame {
	long call_real_flag;
	void *real_impl;
	union {
		long ret_val_long;
		unsigned char ret_val_fp[16];
	} ret_val;
	int ret_is_fp;
	int _pad0;

	unsigned long real_x0;
	unsigned long real_x1;
	unsigned long real_x2;
	unsigned long real_x3;
	unsigned long real_x4;
	unsigned long real_x5;
	unsigned long real_x6;
	unsigned long real_x7;

	/* Aligns the SIMD region to a 16-byte boundary, matching the offset
	 * constants in arch_spec_top.S.
	 */
	unsigned long _pad_simd_align;

	unsigned char real_v0[16];
	unsigned char real_v1[16];
	unsigned char real_v2[16];
	unsigned char real_v3[16];
	unsigned char real_v4[16];
	unsigned char real_v5[16];
	unsigned char real_v6[16];
	unsigned char real_v7[16];

	unsigned long real_lr;
	unsigned long orig_sp;
};

struct AsThreadContext {
	printf_domain_t domain;

	/*
	 * macOS does not ship parse_printf_format(); the printf_domain
	 * callbacks populate this array with PA_* values (0 = unknown).
	 */
	int printf_args_types[ENGINE_MAXCOUNT_PARAMS];
	int printf_args_cnt;
};

static pthread_key_t as_thread_ctx_key;

static void as_thread_ctx_destructor(void *thread_ctx)
{
	free_printf_domain(((struct AsThreadContext *) thread_ctx)->domain);
	retrace_real_impls.free(thread_ctx);
	retrace_real_impls.rc_tss_delete(as_thread_ctx_key);
}

static int as_arginfo_function(const struct printf_info *__info,
		     size_t __n, int *__argtypes)
{
	struct AsThreadContext *ctx =
		(struct AsThreadContext *) __info->context;
	int n_args = 0;
	int type = 0;

	/* Width '*' is indicated by width == INT_MIN on Darwin/FreeBSD. */
	if (__info->width == INT_MIN) {
		if (ctx->printf_args_cnt < ENGINE_MAXCOUNT_PARAMS)
			ctx->printf_args_types[ctx->printf_args_cnt++] = PA_INT;
		n_args++;
	}

	/* Precision '*' is indicated by prec == INT_MIN. */
	if (__info->prec == INT_MIN) {
		if (ctx->printf_args_cnt < ENGINE_MAXCOUNT_PARAMS)
			ctx->printf_args_types[ctx->printf_args_cnt++] = PA_INT;
		n_args++;
	}

	switch (__info->spec) {
	case 'p':
		type = PA_POINTER;
		break;
	case 's':
		type = PA_STRING;
		break;
	case 'S':
		type = PA_WSTRING;
		break;
	case 'c':
		type = PA_CHAR;
		break;
	case 'g': case 'G':
	case 'a': case 'A':
	case 'e': case 'E':
	case 'f': case 'F':
		type = PA_DOUBLE;
		break;
	case 'x': case 'X':
	case 'u': case 'o':
	case 'd': case 'i':
		type = PA_INT;
		break;
	case '%':
		/* literal percent -- no conversion arg */
		return n_args;
	default:
		type = PA_INT;
		break;
	}

	if (__info->is_long_double)
		type |= PA_FLAG_LONG_LONG;
	else if (__info->is_short)
		type |= PA_FLAG_SHORT;
	else if (__info->is_long)
		type |= PA_FLAG_LONG;
	else if (__info->is_quad)
		type |= PA_FLAG_QUAD;
	else if (__info->is_intmax)
		type |= PA_FLAG_INTMAX;
	else if (__info->is_ptrdiff)
		type |= PA_FLAG_PTRDIFF;
	else if (__info->is_size)
		type |= PA_FLAG_SIZE;

	if (ctx->printf_args_cnt < ENGINE_MAXCOUNT_PARAMS)
		ctx->printf_args_types[ctx->printf_args_cnt++] = type;
	n_args++;

	/*
	 * Must return the number of arguments this conversion consumes.
	 * Returning 0 (the old code) made new_printf_comp stop processing
	 * subsequent conversions, so printf_args_cnt stayed incomplete
	 * and call_real only passed the format string -- crash in strlen.
	 */
	return n_args;
}

static int
	as_render(FILE *stream, const struct printf_info *info,
		const void *const *args)
{
	return 0;
}

static inline struct AsThreadContext *as_get_thread_context(void)
{
	struct AsThreadContext *thread_ctx;

	thread_ctx = (struct AsThreadContext *)
		retrace_real_impls.rc_tss_get(as_thread_ctx_key);

	if (thread_ctx == NULL) {
		thread_ctx = (struct AsThreadContext *)
			retrace_real_impls.malloc(sizeof(struct AsThreadContext));
		if (thread_ctx == NULL) {
			log_err("failed to malloc thread_ctx");
			return NULL;
		}

		if (retrace_real_impls.rc_tss_set(
			as_thread_ctx_key, thread_ctx)) {
			log_err("failed to set specific thread key");
			retrace_real_impls.free(thread_ctx);
			return NULL;
		}

		retrace_real_impls.memset(thread_ctx, 0, sizeof(*thread_ctx));

		thread_ctx->domain = new_printf_domain();
		if (thread_ctx->domain == NULL) {
			log_err("failed to create printf domain");
			retrace_real_impls.rc_tss_delete(as_thread_ctx_key);
			retrace_real_impls.free(thread_ctx);
			return NULL;
		}

		for (char j = 'a'; j <= 'z'; j++)
			register_printf_domain_function(thread_ctx->domain,
				j, as_render, as_arginfo_function,
				(void *) thread_ctx);

		for (char i = 'A'; i <= 'Z'; i++)
			register_printf_domain_function(thread_ctx->domain,
				i, as_render, as_arginfo_function,
				(void *) thread_ctx);
	}

	return thread_ctx;
}

/*
 * Apple AArch64 ABI distinguishes NAMED and VARIADIC args:
 *
 *   - Named args (idx < named_count): passed in x0..x7 (saved in
 *     frame->real_x0..real_x7). Beyond x7 they spill onto the caller's
 *     stack at orig_sp + 8*(idx-8).
 *   - Variadic args (idx >= named_count): Apple always pushes variadic
 *     args onto the caller's stack before the call. Variadic arg k is at
 *     orig_sp + 8*k (NOT in x1..x7 as AAPCS64 allows on Linux/BSD).
 *
 * This split lets the same struct + read path serve both named and
 * variadic args for any FAT_PRINTF-style function.
 */
static unsigned long wrapper_frame_get_arg(
	const struct WrapperAArch64Frame *frame, int idx, int named_count)
{
	if (idx < named_count) {
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
			return *(unsigned long *)(frame->orig_sp +
				sizeof(void *) * (idx - 8));
		}
	}

	return *(unsigned long *)(frame->orig_sp +
		sizeof(void *) * (idx - named_count));
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
	struct AsThreadContext *as_ctx;
	printf_comp_t pc;

	if (*params_cnt < proto->params_cnt) {
		log_err("too many prototyped params for '%s', no space for %d more",
			proto->name,
			(*params_cnt - proto->params_cnt) * -1);
		return 0;
	}

	for (param_idx = 0; param_idx != proto->params_cnt; param_idx++) {
		retrace_real_impls.memset(&params[param_idx].param_meta, 0,
			sizeof(struct ParamMeta));
		retrace_real_impls.memcpy(&params[param_idx].param_meta,
			&proto->params[param_idx], sizeof(struct ParamMeta));
		params[param_idx].data_type =
			retrace_datatype_get(proto->params[param_idx].type_name);
		params[param_idx].val =
			(long) wrapper_frame_get_arg(frame, param_idx,
				proto->params_cnt);
	}

	if (proto->fmt == FAT_NOVARARGS) {
		*params_cnt = proto->params_cnt;
		return 1;
	}

	if ((proto->fmt != FAT_PRINTF) && (proto->fmt != FAT_SCANF)) {
		log_err("varargs format '%d' is not supported for func '%s'",
			proto->fmt, proto->name);
		return 0;
	}

	as_ctx = as_get_thread_context();
	if (as_ctx == NULL) {
		log_err("failed to get as_ctx for func '%s'", proto->name);
		return 0;
	}

	as_ctx->printf_args_cnt = 0;
	pc = new_printf_comp(as_ctx->domain, NULL,
		(const char *) params[proto->fmt_param_idx].val);
	if (pc == NULL) {
		log_err("new_printf_comp failed for func '%s'", proto->name);
		return 0;
	}
	free_printf_comp(pc);

	printf_params = as_ctx->printf_args_cnt;

	/*
	 * FP-detection bail: if any vararg is a float/double, our integer-only
	 * dispatch can't place it in the correct register file (v0..v7 for
	 * AAPCS64, xmm0..xmm7 for Sys V x86-64, caller stack for Apple
	 * AArch64). Return 0 to make the engine skip action processing and
	 * let the asm trampoline tail-call real with all original regs/stack
	 * intact. The call works correctly; users lose log_params visibility
	 * for that specific call. See TODO.complete/01-float-varargs-aarch64.md.
	 */
	for (i = 0; i < printf_params; i++) {
		int basic = as_ctx->printf_args_types[i] & ~PA_FLAG_MASK;

		if (basic == PA_FLOAT || basic == PA_DOUBLE) {
			log_dbg(
				"FP vararg in '%s' -- deferring to asm Path A",
				proto->name);
			return 0;
		}
	}

	for (i = 0; i != printf_params; i++, param_idx++) {
		dt = retrace_datatype_printf_to_dt(as_ctx->printf_args_types[i]);

		retrace_real_impls.memset(&params[param_idx].param_meta, 0,
			sizeof(struct ParamMeta));
		retrace_real_impls.real_snprintf(params[param_idx].param_meta.name,
			sizeof(params[param_idx].param_meta.name),
			"vararg%02d", i);
		retrace_real_impls.strcpy(params[param_idx].param_meta.type_name,
			dt->name);
		params[param_idx].param_meta.modifiers = CDM_NOMOD;
		params[param_idx].param_meta.direction = PDIR_IN;

		if ((as_ctx->printf_args_types[i] & ~PA_FLAG_MASK) == PA_STRING) {
			params[param_idx].param_meta.modifiers |= CDM_POINTER;
			retrace_real_impls.strcpy(
				params[param_idx].param_meta.ref_type_name, "sz");
		}

		params[param_idx].data_type = dt;
		params[param_idx].val =
			(long) wrapper_frame_get_arg(frame, param_idx,
				proto->params_cnt);
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
	int key_res;

	key_res = retrace_real_impls.rc_tss_create(&as_thread_ctx_key,
		as_thread_ctx_destructor);

	if (key_res)
		log_err("failed to create pthread_key");

	return key_res;
}

void *retrace_as_get_real_safe(const char *real_impl)
{
	unsigned long size;
	int i;
	void *fallback;

	/* This has to be aligned with __retrace_rimpls entry emitted by
	 * arch_spec_top.S: a MAXLEN_FUNC_NAME-wide name field followed by
	 * a pointer to the real symbol.
	 */
	struct __retrace_rimpls {
		const char func_name[MAXLEN_FUNC_NAME];
		void *real_impl;
	} *p, *eos;

	retrace_as_get_section_info("__DATA", "__retrace_rimpls", &p, &size);

	eos = (struct __retrace_rimpls *) (((char *) p) + size);
	while (p && p != eos) {
		i = 0;
		while (p->func_name[i] && p->func_name[i] == real_impl[i])
			i++;

		if (p->func_name[i] == real_impl[i])
			return p->real_impl;

		p++;
	}

	/*
	 * Section-walk miss. On Intel macOS, ld64 silently drops the
	 * `.quad _<name>` reference in __retrace_rimpls for some libc
	 * symbols (malloc, realloc — issue #506). Fall back to
	 * dlsym(RTLD_NEXT, ...). dlsym is NOT interposed by retrace
	 * (no wrapper entry in funcs_symbols.S), so this is safe.
	 */
	fallback = dlsym(RTLD_NEXT, real_impl);
	return fallback;
}
