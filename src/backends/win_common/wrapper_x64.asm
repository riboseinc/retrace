; Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
;
; BSD-2-Clause license -- see LICENSE for details.
;
; Windows x64 assembly wrappers (TODO.windows/05), MASM dialect
; (ml64). wrapper_x64.S is the GNU-as twin -- identical
; semantics; see there for the full stack-layout rationale.
;
; 120-byte allocation: 32 shadow + 80 frame. Frame base rsp+32.
;
; Label-free: each wrapper passes its INDEX into the C-side
; name table (hook_targets.c, same order) to retrace_win_enter
; as a pure immediate. ml64 and llvm-ml disagree on label
; addressing (lea/mov OFFSET encodings); they agree on
; immediates and extern calls.

EXTERN      retrace_win_enter:PROC

WRAPPER_ENTRY_WIN_X64 MACRO fname, idx
LOCAL synth
        PUBLIC  retrace_wrap_&fname&
retrace_wrap_&fname& PROC
        sub     rsp, 120

        mov     QWORD PTR [rsp+56], r9          ; frame+24
        mov     QWORD PTR [rsp+64], r8          ; frame+32
        mov     QWORD PTR [rsp+72], rcx         ; frame+40
        mov     QWORD PTR [rsp+80], rdx         ; frame+48
        mov     QWORD PTR [rsp+88], rsi         ; frame+56
        mov     QWORD PTR [rsp+96], rdi         ; frame+64
        lea     rax, [rsp+120]
        mov     QWORD PTR [rsp+104], rax        ; frame+72

        mov     QWORD PTR [rsp+32], 0           ; call_real_flag
        mov     QWORD PTR [rsp+40], 0           ; real_impl
        mov     QWORD PTR [rsp+48], 0           ; ret_val

        mov     ecx, idx
        lea     rdx, [rsp+32]
        call    retrace_win_enter

        cmp     QWORD PTR [rsp+32], 1
        jne     synth

        mov     r9,  QWORD PTR [rsp+56]
        mov     r8,  QWORD PTR [rsp+64]
        mov     rcx, QWORD PTR [rsp+72]
        mov     rdx, QWORD PTR [rsp+80]
        mov     r11, QWORD PTR [rsp+40]
        add     rsp, 120
        jmp     r11

synth:
        mov     rax, QWORD PTR [rsp+48]
        add     rsp, 120
        ret
retrace_wrap_&fname& ENDP
        ENDM

.code

        WRAPPER_ENTRY_WIN_X64 fopen, 0
        WRAPPER_ENTRY_WIN_X64 open, 6
        WRAPPER_ENTRY_WIN_X64 close, 7
        WRAPPER_ENTRY_WIN_X64 read, 8
        WRAPPER_ENTRY_WIN_X64 write, 9
        WRAPPER_ENTRY_WIN_X64 lseek, 10
        WRAPPER_ENTRY_WIN_X64 stat, 11
        WRAPPER_ENTRY_WIN_X64 unlink, 12
        WRAPPER_ENTRY_WIN_X64 remove, 13
        WRAPPER_ENTRY_WIN_X64 rename, 14
        WRAPPER_ENTRY_WIN_X64 rmdir, 15
        WRAPPER_ENTRY_WIN_X64 NtCreateFile, 1
        WRAPPER_ENTRY_WIN_X64 NtOpenFile, 2
        WRAPPER_ENTRY_WIN_X64 NtQueryAttributesFile, 3
        WRAPPER_ENTRY_WIN_X64 NtClose, 4
        WRAPPER_ENTRY_WIN_X64 LdrLoadDll, 5

END
