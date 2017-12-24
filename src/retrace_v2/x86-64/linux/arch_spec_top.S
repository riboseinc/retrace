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
#prep WrapperSystemVFrame_t struct starts on this rsp, alignment needed
pushq %rsp
pushq %rdi
pushq %rsi
pushq %rdx
pushq %rcx
pushq %r8
pushq %r9

# for call_real_flag, real_impl, ret_val
subq $24, %rsp

#pass function name as first param
leaq \func_name\()_real_name(%rip), %rdi

#pass WrapperSystemVFrame_t as second param
movq %rsp, %rsi

#align rsp on 16
subq $8, %rsp

#call arch-unaware intercept wrapper
movabs $retrace_engine_wrapper, %rax
call %rax

#set back rsp to WrapperSystemVFrame_t
addq $8, %rsp

#check call_real_flag, 1 marks the call is needed
cmpq $0, OFFS_CALL_REAL_FLAG(%rsp)
jz \func_name\()_no_real

#load registers and restore stack, jmp to real
#note params values may be modified
addq $24, %rsp
popq %r9
popq %r8
popq %rcx
popq %rdx
popq %rsi
popq %rdi
popq %rsp

#jump to real, the real will return to the caller
jmp -72(%rsp)

\func_name\()_no_real:
#TODO write back params?

# set return value. TODO: support long long return
movq OFFS_RET_VAL(%rsp), %rax
addq $80, %rsp
retq

.data
\func_name\()_real_name:
.string "\func_name"
.endm

#TODO: Change it to real-time override using elf lib
WRAPPER_ENTRY_SYSTEM_V getenv
WRAPPER_ENTRY_SYSTEM_V write
WRAPPER_ENTRY_SYSTEM_V writev
#WRAPPER_ENTRY_SYSTEM_V dummy_func5
#WRAPPER_ENTRY_SYSTEM_V dummy_func6
#WRAPPER_ENTRY_SYSTEM_V dummy_func7
#WRAPPER_ENTRY_SYSTEM_V dummy_func0_no_ret
#WRAPPER_ENTRY_SYSTEM_V dummy_func0


