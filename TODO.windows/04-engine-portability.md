# 04 — engine portability: v2 core builds on Windows

Track B, slice 1 of the functional chain. Status: done (v2.11.0) —
ships as v2.11.0 (sub-slices below executed together; each is
locally verified by POSIX suite 77/77 + x86_64-w64-mingw32
cross-compile, then CI).

## Why
`RETRACE_BUILD_V2` is disabled on Windows (CMakeLists.txt:121-126):
engine.c and the core assume POSIX. Until the core builds there,
items 05-06 have nothing to dispatch into. This is THE blocker
for "retrace works everywhere".

## POSIX-ism inventory (2026-08-19, src/core/*)
| File | POSIX deps | Windows answer |
|------|-----------|----------------|
| real_impls.{c,h} | dlsym/RTLD_NEXT, dlfcn.h | GetProcAddress after LDR walk; the struct itself is portable |
| log_ring.c | pthread TSS (per-thread ring) | FlsAlloc (fiber-local storage) |
| log_flusher.c | pthread_create/mutex/cond | CreateThread/SRWLOCK/CONDITION_VARIABLE |
| call_hash.c | pthread self + TSS | GetCurrentThreadId + FlsAlloc |
| dlopen_guard.c | dladdr, link_map walk | LdrEnumerateLoadedModules; dladdr → RtlPcToFileHeader |
| caller_cache.c | backtrace()/dladdr | CaptureStackBackTrace + RtlPcToFileHeader |
| thread_context.c | pthread TSS | FlsAlloc |
| main.c | constructor attr | DllMain already calls register_* (pattern exists) |
| engine.c | none found (pure C + parson) | — |
| sockaddr_inspect.c | arpa/inet | WSA (ws2_32) |

## What (executed slices, v2.11.0)
1. **Portability shim** `src/core/posix_compat.h` — the ONLY
   place platform threads/symbols are touched: rc_mutex_t
   (pthread_mutex | SRWLOCK), rc_tss_t (pthread_key | FlsAlloc,
   both destructor-capable), rc_thread_t, rc_thread_self/
   create/join, rc_getpid/rc_gettid, rc_dladdr (Windows: module
   base+name via RtlPcToFileHeader; symbol name NULL — module
   matching only, documented), rc_backtrace
   (CaptureStackBackTrace). Consumers migrate pthread_* ->
   rc_* THROUGH retrace_real_impls (reentrancy guard preserved:
   the struct fields are retyped+renamed, resolution per
   platform).
2. **real_impls on Windows**: pre-hook resolution assigns plain
   CRT/Win32 (safe: nothing is hooked yet); the dlopen
   bootstrap is POSIX-only (guarded). Item 05 replaces
   resolution with hook trampolines.
3. **PE section macros** `src/backends/win_common/
   arch_spec_macros.h` — #pragma data_seg + __declspec(allocate)
   equivalents of the ELF/Mach-O registry sections; core
   CMake gains the WINDOWS arch-include branch.
4. **sockaddr_inspect on winsock2** (ws2_32; no WSAStartup
   needed for ntohs/inet_ntop on Win10+).
5. **CMake**: RETRACE_BUILD_V2 builds retrace_core on Windows;
   MSVC/MinGW legs + msys.yml compile it; logger self-test
   runs on the Windows runners.

## Acceptance
- [x] Windows CI legs build the full v2 core (compile + link,
      MSVC and MinGW); existing tests pass on Linux/macOS/BSD
      unchanged (shim is a no-op there).
- [x] trace-load unit test RUNS GREEN on the Windows CI legs.
- [ ] logger-fmt scenarios run on MSVC: compile+link are clean
      but the binary segfaults at runtime (0x00s, no output) --
      needs a Windows box or a debugger leg; pinned as the FIRST
      acceptance task of item 05 (works under MinGW-w64, so the
      fault is MSVC-specific: ctor absence, buffered-stdout loss,
      or the OpenSSL/vcpkg static-init interplay are candidates).
- [x] Local: x86_64-w64-mingw32 cross-compile of retrace_core
      (+ tests + backends DLL).
- [x] `grep -rn "pthread_" src/core/` hits only posix_compat.h
      (and real_impls.c resolution comments).

## Examples parity (user directive, 2026-08-19)
Every example under examples/ must gain a Windows counterpart
(or a documented platform note when the mechanism is inherently
POSIX): the demos are the first thing users copy. Track: dns-
fuzz, getenv-fuzzing, http-server-overflow, id-redirection,
net-fuzzing, stringinject, unsafe-system. Windows legs run them
through `retrace run`-equivalent injection (CreateRemoteThread)
once item 05 lands the first hook; until then the examples build
as plain targets on the Windows CI legs. Also add one NEW
example: escape-hunting (recipe 33 end-to-end: inside stream +
retrace-correlate), POSIX now, Windows via procmon2retrace.
