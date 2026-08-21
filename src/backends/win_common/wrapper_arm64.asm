; Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
;
; BSD-2-Clause license -- see LICENSE for details.
;
; Windows arm64 assembly wrappers (TODO.trace-profile/08),
; armasm64 dialect (MSVC-arm64). wrapper_arm64.S is the GNU-as
; twin -- identical semantics; see there for the AAPCS64 frame
; rationale (112-byte frame, x0-x7 at [sp+24..80], x30 at +88,
; entry sp at +96).
;
; Label-free: each wrapper passes its INDEX into the C-side
; name table (hook_targets.c, same order) to retrace_win_enter
; as a pure immediate -- label addressing is where assemblers
; disagree.
;
; Deliberately macro-free: armasm64's MACRO prototype form is
; the one dialect surface we cannot validate locally, so the 16
; entries are written out (the gas twin keeps the macro).

    IMPORT      retrace_win_enter

    AREA        |.text|, CODE, READONLY

    EXPORT      retrace_wrap_fopen
retrace_wrap_fopen
    sub         sp, sp, #112
    stp         x0, x1, [sp, #24]
    stp         x2, x3, [sp, #40]
    stp         x4, x5, [sp, #56]
    stp         x6, x7, [sp, #72]
    str         x30, [sp, #88]
    add         x9, sp, #112
    str         x9, [sp, #96]
    stp         xzr, xzr, [sp, #0]
    str         xzr, [sp, #16]
    mov         w0, #0
    mov         x1, sp
    bl          retrace_win_enter
    ldr         x9, [sp, #0]
    cbnz        x9, fopen_call

    ; synthesized return: x0 = ret_val, x30 restored
    ldr         x0, [sp, #16]
    ldr         x30, [sp, #88]
    add         sp, sp, #112
    ret

fopen_call
    ; tail-jump the trampoline with the original registers;
    ; the user's return address rides in x30 and [entry sp]
    ldp         x0, x1, [sp, #24]
    ldp         x2, x3, [sp, #40]
    ldp         x4, x5, [sp, #56]
    ldp         x6, x7, [sp, #72]
    ldr         x30, [sp, #88]
    ldr         x9, [sp, #8]
    add         sp, sp, #112
    br          x9

    EXPORT      retrace_wrap_NtCreateFile
retrace_wrap_NtCreateFile
    sub         sp, sp, #112
    stp         x0, x1, [sp, #24]
    stp         x2, x3, [sp, #40]
    stp         x4, x5, [sp, #56]
    stp         x6, x7, [sp, #72]
    str         x30, [sp, #88]
    add         x9, sp, #112
    str         x9, [sp, #96]
    stp         xzr, xzr, [sp, #0]
    str         xzr, [sp, #16]
    mov         w0, #1
    mov         x1, sp
    bl          retrace_win_enter
    ldr         x9, [sp, #0]
    cbnz        x9, NtCreateFile_call

    ; synthesized return: x0 = ret_val, x30 restored
    ldr         x0, [sp, #16]
    ldr         x30, [sp, #88]
    add         sp, sp, #112
    ret

NtCreateFile_call
    ; tail-jump the trampoline with the original registers;
    ; the user's return address rides in x30 and [entry sp]
    ldp         x0, x1, [sp, #24]
    ldp         x2, x3, [sp, #40]
    ldp         x4, x5, [sp, #56]
    ldp         x6, x7, [sp, #72]
    ldr         x30, [sp, #88]
    ldr         x9, [sp, #8]
    add         sp, sp, #112
    br          x9

    EXPORT      retrace_wrap_NtOpenFile
retrace_wrap_NtOpenFile
    sub         sp, sp, #112
    stp         x0, x1, [sp, #24]
    stp         x2, x3, [sp, #40]
    stp         x4, x5, [sp, #56]
    stp         x6, x7, [sp, #72]
    str         x30, [sp, #88]
    add         x9, sp, #112
    str         x9, [sp, #96]
    stp         xzr, xzr, [sp, #0]
    str         xzr, [sp, #16]
    mov         w0, #2
    mov         x1, sp
    bl          retrace_win_enter
    ldr         x9, [sp, #0]
    cbnz        x9, NtOpenFile_call

    ; synthesized return: x0 = ret_val, x30 restored
    ldr         x0, [sp, #16]
    ldr         x30, [sp, #88]
    add         sp, sp, #112
    ret

NtOpenFile_call
    ; tail-jump the trampoline with the original registers;
    ; the user's return address rides in x30 and [entry sp]
    ldp         x0, x1, [sp, #24]
    ldp         x2, x3, [sp, #40]
    ldp         x4, x5, [sp, #56]
    ldp         x6, x7, [sp, #72]
    ldr         x30, [sp, #88]
    ldr         x9, [sp, #8]
    add         sp, sp, #112
    br          x9

    EXPORT      retrace_wrap_NtQueryAttributesFile
retrace_wrap_NtQueryAttributesFile
    sub         sp, sp, #112
    stp         x0, x1, [sp, #24]
    stp         x2, x3, [sp, #40]
    stp         x4, x5, [sp, #56]
    stp         x6, x7, [sp, #72]
    str         x30, [sp, #88]
    add         x9, sp, #112
    str         x9, [sp, #96]
    stp         xzr, xzr, [sp, #0]
    str         xzr, [sp, #16]
    mov         w0, #3
    mov         x1, sp
    bl          retrace_win_enter
    ldr         x9, [sp, #0]
    cbnz        x9, NtQueryAttributesFile_call

    ; synthesized return: x0 = ret_val, x30 restored
    ldr         x0, [sp, #16]
    ldr         x30, [sp, #88]
    add         sp, sp, #112
    ret

NtQueryAttributesFile_call
    ; tail-jump the trampoline with the original registers;
    ; the user's return address rides in x30 and [entry sp]
    ldp         x0, x1, [sp, #24]
    ldp         x2, x3, [sp, #40]
    ldp         x4, x5, [sp, #56]
    ldp         x6, x7, [sp, #72]
    ldr         x30, [sp, #88]
    ldr         x9, [sp, #8]
    add         sp, sp, #112
    br          x9

    EXPORT      retrace_wrap_NtClose
retrace_wrap_NtClose
    sub         sp, sp, #112
    stp         x0, x1, [sp, #24]
    stp         x2, x3, [sp, #40]
    stp         x4, x5, [sp, #56]
    stp         x6, x7, [sp, #72]
    str         x30, [sp, #88]
    add         x9, sp, #112
    str         x9, [sp, #96]
    stp         xzr, xzr, [sp, #0]
    str         xzr, [sp, #16]
    mov         w0, #4
    mov         x1, sp
    bl          retrace_win_enter
    ldr         x9, [sp, #0]
    cbnz        x9, NtClose_call

    ; synthesized return: x0 = ret_val, x30 restored
    ldr         x0, [sp, #16]
    ldr         x30, [sp, #88]
    add         sp, sp, #112
    ret

NtClose_call
    ; tail-jump the trampoline with the original registers;
    ; the user's return address rides in x30 and [entry sp]
    ldp         x0, x1, [sp, #24]
    ldp         x2, x3, [sp, #40]
    ldp         x4, x5, [sp, #56]
    ldp         x6, x7, [sp, #72]
    ldr         x30, [sp, #88]
    ldr         x9, [sp, #8]
    add         sp, sp, #112
    br          x9

    EXPORT      retrace_wrap_LdrLoadDll
retrace_wrap_LdrLoadDll
    sub         sp, sp, #112
    stp         x0, x1, [sp, #24]
    stp         x2, x3, [sp, #40]
    stp         x4, x5, [sp, #56]
    stp         x6, x7, [sp, #72]
    str         x30, [sp, #88]
    add         x9, sp, #112
    str         x9, [sp, #96]
    stp         xzr, xzr, [sp, #0]
    str         xzr, [sp, #16]
    mov         w0, #5
    mov         x1, sp
    bl          retrace_win_enter
    ldr         x9, [sp, #0]
    cbnz        x9, LdrLoadDll_call

    ; synthesized return: x0 = ret_val, x30 restored
    ldr         x0, [sp, #16]
    ldr         x30, [sp, #88]
    add         sp, sp, #112
    ret

LdrLoadDll_call
    ; tail-jump the trampoline with the original registers;
    ; the user's return address rides in x30 and [entry sp]
    ldp         x0, x1, [sp, #24]
    ldp         x2, x3, [sp, #40]
    ldp         x4, x5, [sp, #56]
    ldp         x6, x7, [sp, #72]
    ldr         x30, [sp, #88]
    ldr         x9, [sp, #8]
    add         sp, sp, #112
    br          x9

    EXPORT      retrace_wrap_open
retrace_wrap_open
    sub         sp, sp, #112
    stp         x0, x1, [sp, #24]
    stp         x2, x3, [sp, #40]
    stp         x4, x5, [sp, #56]
    stp         x6, x7, [sp, #72]
    str         x30, [sp, #88]
    add         x9, sp, #112
    str         x9, [sp, #96]
    stp         xzr, xzr, [sp, #0]
    str         xzr, [sp, #16]
    mov         w0, #6
    mov         x1, sp
    bl          retrace_win_enter
    ldr         x9, [sp, #0]
    cbnz        x9, open_call

    ; synthesized return: x0 = ret_val, x30 restored
    ldr         x0, [sp, #16]
    ldr         x30, [sp, #88]
    add         sp, sp, #112
    ret

open_call
    ; tail-jump the trampoline with the original registers;
    ; the user's return address rides in x30 and [entry sp]
    ldp         x0, x1, [sp, #24]
    ldp         x2, x3, [sp, #40]
    ldp         x4, x5, [sp, #56]
    ldp         x6, x7, [sp, #72]
    ldr         x30, [sp, #88]
    ldr         x9, [sp, #8]
    add         sp, sp, #112
    br          x9

    EXPORT      retrace_wrap_close
retrace_wrap_close
    sub         sp, sp, #112
    stp         x0, x1, [sp, #24]
    stp         x2, x3, [sp, #40]
    stp         x4, x5, [sp, #56]
    stp         x6, x7, [sp, #72]
    str         x30, [sp, #88]
    add         x9, sp, #112
    str         x9, [sp, #96]
    stp         xzr, xzr, [sp, #0]
    str         xzr, [sp, #16]
    mov         w0, #7
    mov         x1, sp
    bl          retrace_win_enter
    ldr         x9, [sp, #0]
    cbnz        x9, close_call

    ; synthesized return: x0 = ret_val, x30 restored
    ldr         x0, [sp, #16]
    ldr         x30, [sp, #88]
    add         sp, sp, #112
    ret

close_call
    ; tail-jump the trampoline with the original registers;
    ; the user's return address rides in x30 and [entry sp]
    ldp         x0, x1, [sp, #24]
    ldp         x2, x3, [sp, #40]
    ldp         x4, x5, [sp, #56]
    ldp         x6, x7, [sp, #72]
    ldr         x30, [sp, #88]
    ldr         x9, [sp, #8]
    add         sp, sp, #112
    br          x9

    EXPORT      retrace_wrap_read
retrace_wrap_read
    sub         sp, sp, #112
    stp         x0, x1, [sp, #24]
    stp         x2, x3, [sp, #40]
    stp         x4, x5, [sp, #56]
    stp         x6, x7, [sp, #72]
    str         x30, [sp, #88]
    add         x9, sp, #112
    str         x9, [sp, #96]
    stp         xzr, xzr, [sp, #0]
    str         xzr, [sp, #16]
    mov         w0, #8
    mov         x1, sp
    bl          retrace_win_enter
    ldr         x9, [sp, #0]
    cbnz        x9, read_call

    ; synthesized return: x0 = ret_val, x30 restored
    ldr         x0, [sp, #16]
    ldr         x30, [sp, #88]
    add         sp, sp, #112
    ret

read_call
    ; tail-jump the trampoline with the original registers;
    ; the user's return address rides in x30 and [entry sp]
    ldp         x0, x1, [sp, #24]
    ldp         x2, x3, [sp, #40]
    ldp         x4, x5, [sp, #56]
    ldp         x6, x7, [sp, #72]
    ldr         x30, [sp, #88]
    ldr         x9, [sp, #8]
    add         sp, sp, #112
    br          x9

    EXPORT      retrace_wrap_write
retrace_wrap_write
    sub         sp, sp, #112
    stp         x0, x1, [sp, #24]
    stp         x2, x3, [sp, #40]
    stp         x4, x5, [sp, #56]
    stp         x6, x7, [sp, #72]
    str         x30, [sp, #88]
    add         x9, sp, #112
    str         x9, [sp, #96]
    stp         xzr, xzr, [sp, #0]
    str         xzr, [sp, #16]
    mov         w0, #9
    mov         x1, sp
    bl          retrace_win_enter
    ldr         x9, [sp, #0]
    cbnz        x9, write_call

    ; synthesized return: x0 = ret_val, x30 restored
    ldr         x0, [sp, #16]
    ldr         x30, [sp, #88]
    add         sp, sp, #112
    ret

write_call
    ; tail-jump the trampoline with the original registers;
    ; the user's return address rides in x30 and [entry sp]
    ldp         x0, x1, [sp, #24]
    ldp         x2, x3, [sp, #40]
    ldp         x4, x5, [sp, #56]
    ldp         x6, x7, [sp, #72]
    ldr         x30, [sp, #88]
    ldr         x9, [sp, #8]
    add         sp, sp, #112
    br          x9

    EXPORT      retrace_wrap_lseek
retrace_wrap_lseek
    sub         sp, sp, #112
    stp         x0, x1, [sp, #24]
    stp         x2, x3, [sp, #40]
    stp         x4, x5, [sp, #56]
    stp         x6, x7, [sp, #72]
    str         x30, [sp, #88]
    add         x9, sp, #112
    str         x9, [sp, #96]
    stp         xzr, xzr, [sp, #0]
    str         xzr, [sp, #16]
    mov         w0, #10
    mov         x1, sp
    bl          retrace_win_enter
    ldr         x9, [sp, #0]
    cbnz        x9, lseek_call

    ; synthesized return: x0 = ret_val, x30 restored
    ldr         x0, [sp, #16]
    ldr         x30, [sp, #88]
    add         sp, sp, #112
    ret

lseek_call
    ; tail-jump the trampoline with the original registers;
    ; the user's return address rides in x30 and [entry sp]
    ldp         x0, x1, [sp, #24]
    ldp         x2, x3, [sp, #40]
    ldp         x4, x5, [sp, #56]
    ldp         x6, x7, [sp, #72]
    ldr         x30, [sp, #88]
    ldr         x9, [sp, #8]
    add         sp, sp, #112
    br          x9

    EXPORT      retrace_wrap_stat
retrace_wrap_stat
    sub         sp, sp, #112
    stp         x0, x1, [sp, #24]
    stp         x2, x3, [sp, #40]
    stp         x4, x5, [sp, #56]
    stp         x6, x7, [sp, #72]
    str         x30, [sp, #88]
    add         x9, sp, #112
    str         x9, [sp, #96]
    stp         xzr, xzr, [sp, #0]
    str         xzr, [sp, #16]
    mov         w0, #11
    mov         x1, sp
    bl          retrace_win_enter
    ldr         x9, [sp, #0]
    cbnz        x9, stat_call

    ; synthesized return: x0 = ret_val, x30 restored
    ldr         x0, [sp, #16]
    ldr         x30, [sp, #88]
    add         sp, sp, #112
    ret

stat_call
    ; tail-jump the trampoline with the original registers;
    ; the user's return address rides in x30 and [entry sp]
    ldp         x0, x1, [sp, #24]
    ldp         x2, x3, [sp, #40]
    ldp         x4, x5, [sp, #56]
    ldp         x6, x7, [sp, #72]
    ldr         x30, [sp, #88]
    ldr         x9, [sp, #8]
    add         sp, sp, #112
    br          x9

    EXPORT      retrace_wrap_unlink
retrace_wrap_unlink
    sub         sp, sp, #112
    stp         x0, x1, [sp, #24]
    stp         x2, x3, [sp, #40]
    stp         x4, x5, [sp, #56]
    stp         x6, x7, [sp, #72]
    str         x30, [sp, #88]
    add         x9, sp, #112
    str         x9, [sp, #96]
    stp         xzr, xzr, [sp, #0]
    str         xzr, [sp, #16]
    mov         w0, #12
    mov         x1, sp
    bl          retrace_win_enter
    ldr         x9, [sp, #0]
    cbnz        x9, unlink_call

    ; synthesized return: x0 = ret_val, x30 restored
    ldr         x0, [sp, #16]
    ldr         x30, [sp, #88]
    add         sp, sp, #112
    ret

unlink_call
    ; tail-jump the trampoline with the original registers;
    ; the user's return address rides in x30 and [entry sp]
    ldp         x0, x1, [sp, #24]
    ldp         x2, x3, [sp, #40]
    ldp         x4, x5, [sp, #56]
    ldp         x6, x7, [sp, #72]
    ldr         x30, [sp, #88]
    ldr         x9, [sp, #8]
    add         sp, sp, #112
    br          x9

    EXPORT      retrace_wrap_remove
retrace_wrap_remove
    sub         sp, sp, #112
    stp         x0, x1, [sp, #24]
    stp         x2, x3, [sp, #40]
    stp         x4, x5, [sp, #56]
    stp         x6, x7, [sp, #72]
    str         x30, [sp, #88]
    add         x9, sp, #112
    str         x9, [sp, #96]
    stp         xzr, xzr, [sp, #0]
    str         xzr, [sp, #16]
    mov         w0, #13
    mov         x1, sp
    bl          retrace_win_enter
    ldr         x9, [sp, #0]
    cbnz        x9, remove_call

    ; synthesized return: x0 = ret_val, x30 restored
    ldr         x0, [sp, #16]
    ldr         x30, [sp, #88]
    add         sp, sp, #112
    ret

remove_call
    ; tail-jump the trampoline with the original registers;
    ; the user's return address rides in x30 and [entry sp]
    ldp         x0, x1, [sp, #24]
    ldp         x2, x3, [sp, #40]
    ldp         x4, x5, [sp, #56]
    ldp         x6, x7, [sp, #72]
    ldr         x30, [sp, #88]
    ldr         x9, [sp, #8]
    add         sp, sp, #112
    br          x9

    EXPORT      retrace_wrap_rename
retrace_wrap_rename
    sub         sp, sp, #112
    stp         x0, x1, [sp, #24]
    stp         x2, x3, [sp, #40]
    stp         x4, x5, [sp, #56]
    stp         x6, x7, [sp, #72]
    str         x30, [sp, #88]
    add         x9, sp, #112
    str         x9, [sp, #96]
    stp         xzr, xzr, [sp, #0]
    str         xzr, [sp, #16]
    mov         w0, #14
    mov         x1, sp
    bl          retrace_win_enter
    ldr         x9, [sp, #0]
    cbnz        x9, rename_call

    ; synthesized return: x0 = ret_val, x30 restored
    ldr         x0, [sp, #16]
    ldr         x30, [sp, #88]
    add         sp, sp, #112
    ret

rename_call
    ; tail-jump the trampoline with the original registers;
    ; the user's return address rides in x30 and [entry sp]
    ldp         x0, x1, [sp, #24]
    ldp         x2, x3, [sp, #40]
    ldp         x4, x5, [sp, #56]
    ldp         x6, x7, [sp, #72]
    ldr         x30, [sp, #88]
    ldr         x9, [sp, #8]
    add         sp, sp, #112
    br          x9

    EXPORT      retrace_wrap_rmdir
retrace_wrap_rmdir
    sub         sp, sp, #112
    stp         x0, x1, [sp, #24]
    stp         x2, x3, [sp, #40]
    stp         x4, x5, [sp, #56]
    stp         x6, x7, [sp, #72]
    str         x30, [sp, #88]
    add         x9, sp, #112
    str         x9, [sp, #96]
    stp         xzr, xzr, [sp, #0]
    str         xzr, [sp, #16]
    mov         w0, #15
    mov         x1, sp
    bl          retrace_win_enter
    ldr         x9, [sp, #0]
    cbnz        x9, rmdir_call

    ; synthesized return: x0 = ret_val, x30 restored
    ldr         x0, [sp, #16]
    ldr         x30, [sp, #88]
    add         sp, sp, #112
    ret

rmdir_call
    ; tail-jump the trampoline with the original registers;
    ; the user's return address rides in x30 and [entry sp]
    ldp         x0, x1, [sp, #24]
    ldp         x2, x3, [sp, #40]
    ldp         x4, x5, [sp, #56]
    ldp         x6, x7, [sp, #72]
    ldr         x30, [sp, #88]
    ldr         x9, [sp, #8]
    add         sp, sp, #112
    br          x9

    END
