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
 * Darwin x86-64 trampoline.
 *
 * Frame layout matches src/backends/preload_elf/x86_64/arch_spec_top.S;
 * see the commentary there. xmm0..xmm7 are saved for FP varargs (#478).
 * Differences from ELF: section directives (__TEXT/__DATA), Apple symbol
 * prefixes (_), local L labels, direct `call _retrace_engine_wrapper`.
 */

.set OFFS_CALL_REAL_FLAG, 0
.set OFFS_REAL_IMPL, 8
.set OFFS_RET_VAL, 16

#This has to equal to MAXLEN_FUNC_NAME from funcs.h
.set MAXLEN_FUNC_NAME, 64

.macro WRAPPER_ENTRY_SYSTEM_V func_name
.section __TEXT, __text
.global _\func_name\()_retrace_wrapper
_\func_name\()_retrace_wrapper:

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
leaq L\func_name\()_retrace_real_name(%rip), %rdi

# WrapperSystemVFrame pointer as second arg.
movq %rsp, %rsi

# Align rsp on 16 before the call.
subq $8, %rsp

call _retrace_engine_wrapper

# Back to WrapperSystemVFrame.
addq $8, %rsp

# Check call_real_flag.
cmpq $0, OFFS_CALL_REAL_FLAG(%rsp)
jz L\func_name\()_retrace_no_real

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

L\func_name\()_retrace_no_real:
# Path B: synthesized return value (integer return only).
movq OFFS_RET_VAL(%rsp), %rax
addq $(24 + 128 + 56), %rsp
retq

.section __DATA, __retrace_rimpls
L\func_name\()_retrace_real_name:
.string "\func_name"
L\func_name\()_retrace_real_name_size:
.set \func_name\()_SIZE, L\func_name\()_retrace_real_name_size - L\func_name\()_retrace_real_name
.zero MAXLEN_FUNC_NAME - \func_name\()_SIZE
.quad _\func_name

.section __DATA, __interpose
.quad _\func_name\()_retrace_wrapper
.quad _\func_name

.endm
