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
 * Sys V x86-64 trampoline.
 *
 * Frame layout (offsets relative to %rsp at engine-entry time, i.e.
 * after the final `subq $24` for the control fields):
 *
 *     0   call_real_flag    (engine writes 1 to defer to asm Path A)
 *     8   real_impl         (address of real libc function)
 *    16   ret_val           (engine writes synthesized return here)
 *    24   xmm0              (128 bytes; xmm0..xmm7, 16 bytes each)
 *    40   xmm1
 *    56   xmm2
 *    72   xmm3
 *    88   xmm4
 *   104   xmm5
 *   120   xmm6
 *   136   xmm7
 *   152   saved r9          (7 integer arg regs + original rsp)
 *   160   saved r8
 *   168   saved rcx
 *   176   saved rdx
 *   184   saved rsi
 *   192   saved rdi
 *   200   saved rsp value
 *   208   return address    (pushed by caller's `call`)
 *
 * The xmm save area is required for variadic functions (printf etc):
 * the Sys V ABI passes FP varargs in xmm0..xmm7, which are caller-
 * saved and would be clobbered by the engine call. Path A restores
 * them before tail-calling real_impl. Issue #478.
 */

.set OFFS_CALL_REAL_FLAG, 0
.set OFFS_REAL_IMPL, 8
.set OFFS_RET_VAL, 16

.macro WRAPPER_ENTRY_SYSTEM_V func_name
.text
.global \func_name\()_retrace_wrapper
.global \func_name\()
.type \func_name\()_retrace_wrapper, @function
.type \func_name\(), @function

\func_name\():
\func_name\()_retrace_wrapper:
# Build WrapperSystemVFrame from low to high. Push order is reverse:
# %rsp first (so it ends up at the highest address).
pushq %rsp
pushq %rdi
pushq %rsi
pushq %rdx
pushq %rcx
pushq %r8
pushq %r9

# Reserve and fill xmm save area. xmm0..xmm7 hold FP varargs per the
# Sys V ABI; caller-saved, so the engine call would clobber them.
subq $128, %rsp
movdqu %xmm0, 0(%rsp)
movdqu %xmm1, 16(%rsp)
movdqu %xmm2, 32(%rsp)
movdqu %xmm3, 48(%rsp)
movdqu %xmm4, 64(%rsp)
movdqu %xmm5, 80(%rsp)
movdqu %xmm6, 96(%rsp)
movdqu %xmm7, 112(%rsp)

# Reserve control fields.
subq $24, %rsp

# Zero control fields; the engine fills them.
movq $0, OFFS_CALL_REAL_FLAG(%rsp)
movq $0, OFFS_REAL_IMPL(%rsp)
movq $0, OFFS_RET_VAL(%rsp)

# Function name as first arg.
leaq \func_name\()_real_name(%rip), %rdi

# WrapperSystemVFrame pointer as second arg.
movq %rsp, %rsi

# Align rsp on 16 before the call (pushq sequence left rsp at 8 mod 16;
# 56 + 128 + 24 = 208 bytes added; alignment unchanged at 8 mod 16).
subq $8, %rsp

movabs $retrace_engine_wrapper, %rax
call *%rax

# Back to WrapperSystemVFrame.
addq $8, %rsp

# Check call_real_flag.
cmpq $0, OFFS_CALL_REAL_FLAG(%rsp)
jz \func_name\()_no_real

# Path A: tail-call real. Stash real_impl in %rax first; the
# restorations below change %rsp, so a stack-relative load would need
# a magic offset.
movq OFFS_REAL_IMPL(%rsp), %rax

# Drop control fields (24 bytes); rsp now at xmm save area.
addq $24, %rsp

# Restore FP arg regs.
movdqu 0(%rsp), %xmm0
movdqu 16(%rsp), %xmm1
movdqu 32(%rsp), %xmm2
movdqu 48(%rsp), %xmm3
movdqu 64(%rsp), %xmm4
movdqu 80(%rsp), %xmm5
movdqu 96(%rsp), %xmm6
movdqu 112(%rsp), %xmm7
addq $128, %rsp

# Restore integer arg regs and original rsp.
popq %r9
popq %r8
popq %rcx
popq %rdx
popq %rsi
popq %rdi
popq %rsp

# Tail-call real; it returns to the original caller.
jmpq *%rax

\func_name\()_no_real:
# Path B: synthesized return value (integer return only).
movq OFFS_RET_VAL(%rsp), %rax
addq $(24 + 128 + 56), %rsp
retq

.data
\func_name\()_real_name:
.string "\func_name"
.endm
