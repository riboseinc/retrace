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

struct WrapperSystemVFrame {
	/* this flag will cause the assembly portion to call the real impl */
	long call_real_flag;

	/* In case call_real_flag is 1,
	 * the assembly portion will jmp to this address
	 */
	void *real_impl;

	/* return value for the function,
	 * used in case call_real_flag == 0
	 */
	long ret_val;

	/*
	 * FP varargs registers (xmm0..xmm7). Touched only by the asm
	 * trampoline — saved on entry, restored before the Path A
	 * tail-call. Required so printf("%f", ...) round-trips when the
	 * engine defers to Path A. Issue #478.
	 */
	unsigned char _xmm_save[128];

	/* original values of the param regs,
	 * as seen by the assembly portion
	 */
	long real_r9;
	long real_r8;
	long real_rcx;
	long real_rdx;
	long real_rsi;
	long real_rdi;
	long real_rsp;
};

struct RealImpls {
	char* (*getsectdata)(
	   const char *segname,
	   const char *sectname,
	   unsigned long *size);
};

struct AsThreadContext {
	printf_domain_t domain;

	/*
	 * this is to emulate linux printf mechanics, 0 means unknown type
	 * values are PA_* or 0 for unknown
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

	/* try to create if does not exist */
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

		/* reset context data */
		retrace_real_impls.memset(thread_ctx, 0, sizeof(*thread_ctx));

		thread_ctx->domain = new_printf_domain();
		if (thread_ctx->domain == NULL) {
			log_err("failed to create printf domain");

			retrace_real_impls.rc_tss_delete(as_thread_ctx_key);
			retrace_real_impls.free(thread_ctx);
			return NULL;
		}

		/* register every possible format
		 * a little brutal, but it will ensure that no parameter is missed
		 */
		for (char j = 'a'; j <= 'z'; j++)
			register_printf_domain_function(thread_ctx->domain,
				j,
				as_render,
				as_arginfo_function, (void *) thread_ctx);

		for (char i = 'A'; i <= 'Z'; i++)
			register_printf_domain_function(thread_ctx->domain,
				i,
				as_render,
				as_arginfo_function, (void *) thread_ctx);
	}

	return thread_ctx;
}

#ifdef RETRACE_V2_USE_CALC_PARAMS
static int as_calc_params_specifiers(const char *fmt_string)
{
	int i = 0;
	int cnt = 0;
	int perc_cnt;

	while (fmt_string[i]) {
		if (fmt_string[i] == '%') {
			perc_cnt = 1;
			i++;
			while (fmt_string[i] == '%') {
				perc_cnt++;
				i++;
			}

			if (perc_cnt % 2) {
				if (fmt_string[i])
					cnt++;
			}
		} else {
			i++;
		}
	}
	return cnt;
}
#endif

long retrace_as_call_real(const void *real_impl,
	const struct FuncParam params[],
	int params_cnt)
{
	return retrace_as_call_real_dispatch(real_impl, params, params_cnt);
}

void retrace_as_abort(void *arch_spec_ctx, long ret_val)
{
	struct WrapperSystemVFrame *wrapper_frame_top;

	wrapper_frame_top = arch_spec_ctx;
	wrapper_frame_top->call_real_flag = 0;
	wrapper_frame_top->ret_val = ret_val;
}

void retrace_as_sched_real(void *arch_spec_ctx, void *real_impl)
{
	struct WrapperSystemVFrame *wrapper_frame_top;

	wrapper_frame_top = arch_spec_ctx;
	wrapper_frame_top->call_real_flag = 1;
	wrapper_frame_top->real_impl = real_impl;
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
	int params_on_stack;
	struct WrapperSystemVFrame *wrapper_frame_top;
	const struct DataType *dt;
	struct AsThreadContext *as_ctx;
	printf_comp_t pc;
	int use_unk_dt;

	/* check whether there is enough space for prototyped params */
	if (*params_cnt < proto->params_cnt) {
		log_err("too many prototyped params for '%s', no space for %d more",
			proto->name,
			(*params_cnt - proto->params_cnt) * -1);
		return 0;
	}

	/* set up prototyped params */
	wrapper_frame_top = (struct WrapperSystemVFrame *) arch_spec_ctx;
	params_on_stack = proto->params_cnt - 6;

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

		/* setup value */
		if (param_idx < 6) {
			/* get param from reg */
			switch (param_idx) {
			case 0:
				params[param_idx].val =
					wrapper_frame_top->real_rdi;
				break;
			case 1:
				params[param_idx].val =
					wrapper_frame_top->real_rsi;
				break;
			case 2:
				params[param_idx].val =
					wrapper_frame_top->real_rdx;
				break;
			case 3:
				params[param_idx].val =
					wrapper_frame_top->real_rcx;
				break;
			case 4:
				params[param_idx].val =
					wrapper_frame_top->real_r8;
				break;
			case 5:
				params[param_idx].val =
					wrapper_frame_top->real_r9;
				break;
			}
		} else {
			/* get param from stack */

			/* assume sizeof(void*) == sizeof(long) */
			params[param_idx].val =
				(wrapper_frame_top->real_rsp +
					sizeof(void *) * (params_on_stack - param_idx));
		}
	}

	/* set up varargs params */
	if (proto->fmt == FAT_NOVARARGS) {
		/* we`re done */
		*params_cnt = proto->params_cnt;
		return 1;
	}

	/* check whether format is supported */
	if ((proto->fmt != FAT_PRINTF) &&
		(proto->fmt != FAT_SCANF)) {

		log_err("varargs format '%d' is not supported for func '%s'",
			proto->fmt, proto->name);
		return 0;
	}

	/* calculate the expected number of params - disabled
	 * Do not use as_calc_params_specifiers since it is not perfect,
	 * it is better to rely on provided parsing facilities
	 */
#ifdef RETRACE_V2_USE_CALC_PARAMS
	printf_params =
		as_calc_params_specifiers(
			(const char *) params[proto->fmt_param_idx].val);
	if (!printf_params) {
		*params_cnt = proto->params_cnt;
		return 1;
	}
#endif

	as_ctx = as_get_thread_context();
	if (as_ctx == NULL) {
		log_err("failed to get as_ctx for func '%s'", proto->name);
		return 0;
	}

	as_ctx->printf_args_cnt = 0;
	/* callbacks will setup param types */
	pc = new_printf_comp(as_ctx->domain,
		NULL,
		(const char *) params[proto->fmt_param_idx].val);
	if (pc == NULL) {
		log_err("new_printf_comp failed for func '%s'", proto->name);
		return 0;
	}
	free_printf_comp(pc);

#ifdef RETRACE_V2_USE_CALC_PARAMS
	if (printf_params != as_ctx->printf_args_cnt) {
		log_warn("problem parsing params format string for func '%s'",
			proto->name);

		/* set up printf_params as unknown so we won't crash */
		use_unk_dt = 1;
	} else
		use_unk_dt = 0;
#else
	printf_params = as_ctx->printf_args_cnt;
	use_unk_dt = 0;
#endif

	/*
	 * FP-detection bail: if any vararg is float/double, our integer-
	 * only dispatch can't place it in xmm0..xmm7. Return 0 to make
	 * the engine skip action processing and let the asm trampoline
	 * tail-call real with all original regs (now including xmm0..7)
	 * intact. The call works correctly; users lose log_params
	 * visibility for that specific call.
	 * See TODO.complete/01-float-varargs-aarch64.md (#478).
	 */
	for (i = 0; i < printf_params; i++) {
		int basic = as_ctx->printf_args_types[i] & ~PA_FLAG_MASK;

		if (basic == PA_FLOAT || basic == PA_DOUBLE) {
			log_dbg(
				"FP vararg in '%s' -- deferring to asm Path A",
				proto->name);
			*params_cnt = proto->params_cnt;
			return 0;
		}
	}

	for (i = 0; i != printf_params; i++, param_idx++) {
		/* prep param meta */
		if (use_unk_dt)
			dt = retrace_datatype_get_unk_dt();
		else
			dt = retrace_datatype_printf_to_dt(as_ctx->printf_args_types[i]);

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
		/* FIXME: How to know the direction of the param? */
		params[param_idx].param_meta.direction = PDIR_IN;

		/* fixup pointer type to string*/
		if ((as_ctx->printf_args_types[i] & ~PA_FLAG_MASK) == PA_STRING) {
			params[param_idx].param_meta.modifiers |= CDM_POINTER;
			retrace_real_impls.strcpy(
				params[param_idx].param_meta.ref_type_name,
				"sz");
		}

		/* setup datatype */
		params[param_idx].data_type = dt;

		/* setup value */
		if (param_idx < 6) {
			/* get param from reg */
			switch (param_idx) {
			case 0:
				params[param_idx].val =
					wrapper_frame_top->real_rdi;
				break;
			case 1:
				params[param_idx].val =
					wrapper_frame_top->real_rsi;
				break;
			case 2:
				params[param_idx].val =
					wrapper_frame_top->real_rdx;
				break;
			case 3:
				params[param_idx].val =
					wrapper_frame_top->real_rcx;
				break;
			case 4:
				params[param_idx].val =
					wrapper_frame_top->real_r8;
				break;
			case 5:
				params[param_idx].val =
					wrapper_frame_top->real_r9;
				break;
			}
		} else {
			/* get param from stack */

			/* assume sizeof(void*) == sizeof(long) */
			params[param_idx].val =
				(wrapper_frame_top->real_rsp +
					sizeof(void *) * (params_on_stack - param_idx));
		}
	}

	*params_cnt = param_idx;
	return 1;
}

void retrace_as_intercept_done(void *arch_spec_ctx,
	long ret_val)
{
	((struct WrapperSystemVFrame *) arch_spec_ctx)->ret_val = ret_val;

	((struct WrapperSystemVFrame *) arch_spec_ctx)->call_real_flag = 0;
}

void retrace_as_cancel_sched_real(void *arch_spec_ctx)
{
	((struct WrapperSystemVFrame *) arch_spec_ctx)->call_real_flag = 0;
}

void retrace_as_set_ret_val(void *arch_spec_ctx,
	long ret_val)
{
	((struct WrapperSystemVFrame *) arch_spec_ctx)->ret_val = ret_val;
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

	/* This has to be aligned with __retrace_rimpls */
	struct __retrace_rimpls {
		const char func_name[MAXLEN_FUNC_NAME];
		void *real_impl;
	} *p, *eos;

	retrace_as_get_section_info("__DATA", "__retrace_rimpls", &p, &size);

	eos = (struct __retrace_rimpls *) (((char *) p) + size);
	while (p && p != eos) {
		i = 0;
		while (p->func_name[i] &&
			p->func_name[i] == real_impl[i]) {
			i++;
		}

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
