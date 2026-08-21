/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Windows trampoline contract (TODO.windows/05). The assembly
 * wrappers (wrapper_x64.S / wrapper_x64.asm) build a
 * WrapperWinX64Frame on the stack and call retrace_engine_wrapper
 * with (rcx = func name, rdx = frame). This file implements the
 * retrace_as_* contract over that frame.
 *
 * Microsoft x64 ABI: integer/pointer args in rcx, rdx, r8, r9;
 * args 5+ on the stack above the 32-byte shadow space
 * ([entry_rsp + 0x28 + 8*(i-4)]). The frame stores the ENTRY rsp
 * (pointing at the return address).
 *
 * real-impl resolution: a HOOKED function's "real" implementation
 * is the hook's trampoline (relocated prologue + jump back into
 * the body) -- resolving via GetProcAddress would return the
 * patched bytes and re-enter the wrapper forever. hook_targets.c
 * publishes the name->trampoline map; get_real_safe consults it
 * first, then falls back to ucrtbase for everything else.
 */

#include "posix_compat.h"

#include "arch_spec.h"
#include "engine.h"
#include "frame.h"
#include "hook.h"
#include "hook_targets.h"
#include "logger.h"
#include "real_impls.h"

#include <stddef.h>

#if defined(_M_ARM64) || defined(__aarch64__)

#define WRAPPER_FRAME struct WrapperWinArm64Frame

#else /* x64 */

#define WRAPPER_FRAME struct WrapperWinX64Frame

#endif /* arch */

void retrace_as_sched_real(void *arch_spec_ctx, void *real_impl)
{
	WRAPPER_FRAME *frame = arch_spec_ctx;

	frame->call_real_flag = 1;
	frame->real_impl = real_impl;
}

void retrace_as_cancel_sched_real(void *arch_spec_ctx)
{
	((WRAPPER_FRAME *)arch_spec_ctx)->call_real_flag = 0;
}

void retrace_as_set_ret_val(void *arch_spec_ctx, intptr_t ret_val)
{
	((WRAPPER_FRAME *)arch_spec_ctx)->ret_val =
		(int64_t)ret_val;
}

void *retrace_as_get_real_safe(const char *real_impl)
{
	void *trampoline;

	/* hooked? the trampoline is the real implementation */
	trampoline = retrace_win_trampoline_for(real_impl);
	if (trampoline != NULL)
		return trampoline;

	/* otherwise resolve from the CRT directly (ucrt on modern
	 * Windows; msvcrt covers legacy MinGW non-UCRT builds)
	 */
	{
		HMODULE crt = GetModuleHandleA("ucrtbase.dll");

		if (crt == NULL)
			crt = GetModuleHandleA("msvcrt.dll");
		if (crt == NULL)
			return NULL;
		return (void *)GetProcAddress(crt, real_impl);
	}
}

int retrace_as_init(void)
{
	return 0;
}

int retrace_as_init_late(void)
{
	return 0;
}

/*
 * Read one argument value. Register params store values (both
 * frames), so params[i].val is uniform; stack params are read
 * from the caller's frame at the ABI-correct offset.
 *
 * Both frames lead with call_real_flag / real_impl / ret_val
 * at identical offsets, so the frame writers are shared via the
 * WRAPPER_FRAME alias.
 */
#if defined(_M_ARM64) || defined(__aarch64__)

static uint64_t win_arg(const struct WrapperWinArm64Frame *frame,
			int i)
{
	if (i >= 0 && i <= 7)
		return frame->x[i];

	/*
	 * AAPCS64: stack args start at [entry sp] (the hook jmp
	 * pushed nothing)
	 */
	return *(const uint64_t *)(frame->sp +
				   8 * (size_t)(i - 8));
}

#else /* x64 */

static uint64_t win_arg(const struct WrapperWinX64Frame *frame,
			int i)
{
	const uint64_t *stack_arg;

	switch (i) {
	case 0:
		return frame->rcx;
	case 1:
		return frame->rdx;
	case 2:
		return frame->r8;
	case 3:
		return frame->r9;
	default:
		/*
		 * entry rsp points at the return address; args 5+
		 * start after the 32-byte shadow space
		 */
		stack_arg = (const uint64_t *)(frame->rsp + 0x28 +
					       8 * (size_t)(i - 4));
		return *stack_arg;
	}
}

#endif /* arch */

int retrace_as_setup_params(void *arch_spec_ctx,
	const struct FuncPrototype *proto, struct FuncParam params[],
	int *params_cnt)
{
	WRAPPER_FRAME *frame = arch_spec_ctx;
	int param_idx;

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

		/* setup value */
		params[param_idx].val =
			(intptr_t)win_arg(frame, param_idx);
	}

	/*
	 * Varargs: the v1 Windows hook set (fopen, the ntdll file
	 * API) is non-variadic. Decline so the engine skips action
	 * processing and the asm tail-jumps the original regs.
	 */
	if (proto->fmt != FAT_NOVARARGS) {
		log_err("varargs format '%d' is not supported for func '%s'",
			proto->fmt, proto->name);
		return 0;
	}

	*params_cnt = proto->params_cnt;
	return 1;
}

intptr_t retrace_as_call_real(const void *real_impl,
	const struct FuncParam params[], int params_cnt)
{
	return retrace_as_call_real_dispatch(real_impl, params,
		params_cnt);
}
