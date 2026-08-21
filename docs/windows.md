# Windows: injection, hooks, and the file-access jail

How retrace runs on Windows (v2.13.0+): there is no
`LD_PRELOAD`. retrace ships one injectable DLL (`retrace.dll`)
that inline-hooks the C library from scratch — no MinHook, no
Detours (ADR-0009, BSD-2 license purity).

## Quick start

```bat
:: build (MSVC or MSYS2/MinGW; vcpkg pulls OpenSSL + cmocka)
cmake -B build && cmake --build build --config Release

:: trace a program: the DLL is injected at process creation
set RETRACE_JSON_CONFIG=myconf.json
set RETRACE_LOGGER_DEF_ENA=1
set RETRACE_LOGGER_DEF_STDOUT_ENA=0
set RETRACE_LOGGER_DEF_FN=trace.json
set RETRACE_V2_LIB=build\bin\retrace.dll
build\tools\retrace-win-run myapp.exe
```

`retrace-win-run` (tools/win-run) does the injection dance
(one implementation in win_common/inject.c, shared with the
library's spawn backend):
`CreateProcess(... CREATE_SUSPENDED)` →
`VirtualAllocEx` + `WriteProcessMemory` (the DLL path) →
`CreateRemoteThread(LoadLibraryA)` → wait for `DLL_PROCESS_ATTACH`
(engine boot + hook installation inside the child) →
`ResumeThread`.

Inside the child, `DLL_PROCESS_ATTACH`:

1. **install hooks** (before boot — real-impl resolution must
   find each hook's trampoline, not the patched bytes),
2. **boot the engine** (registries, config, logger — MSVC has no
   constructors, so DllMain is the boot point),
3. every hooked call funnels: inline hook → assembly wrapper →
   `retrace_engine_wrapper` → your JSON config's actions →
   trampoline back to the real function.

## The hook layers

| Layer | Default | Functions | Notes |
|---|---|---|---|
| ucrt | ON | `fopen`, `_open`, `_close`, `_read`, `_write`, `_lseek`, `_stat`, `_unlink`, `_remove`, `_rename`, `_rmdir` | The CRT layer — a normal C program's file traffic (v2.14.0 expands the v1 `fopen`-only set) |
| ntdll | OPT-IN | `NtCreateFile`, `NtOpenFile`, `NtQueryAttributesFile`, `NtClose`, `LdrLoadDll` | One level deeper: catches Win32-direct callers that never touch the CRT |

### The ntdll layer (opt-in)

Apps that bypass the CRT are invisible to ucrt hooks — the
libsass importer calls `CreateFileW`/`GetFileAttributesW`
directly, which funnel into `NtCreateFile`/
`NtQueryAttributesFile`. Enable the ntdll set:

```bat
set RETRACE_WIN_NTDLL=1
```

Then name the functions in your config like any other:

```json
{ "intercept_scripts": [
  { "func_name": "NtCreateFile",
    "actions": [ { "action_name": "log_params" },
                 { "action_name": "call_real" } ] },
  { "func_name": "NtQueryAttributesFile",
    "actions": [ { "action_name": "log_params" },
                 { "action_name": "call_real" } ] } ] }
```

Paths decode from `OBJECT_ATTRIBUTES → ObjectName →
UNICODE_STRING` to UTF-8 (the `ntoa`/`ntus` decoders in
`src/core/datatypes/nt_decode.c`), so `retrace-profile` and
`retrace-correlate` consume them like any path.

**Why opt-in**: hooking ntdll is what AV/EDR products watch.
Default configs hook nothing at ntdll depth — only your explicit
`RETRACE_WIN_NTDLL=1` does. Use it on software you are authorized
to test.

`GetFullPathNameW` makes no syscall (pure kernelbase path
math) — there is nothing to hook there.

### Refuse-to-hook

The prologue decoder (`win_common/disasm_x64.c`) relocates only
whole, position-independent instructions. If a target's first
bytes contain a relative branch or RIP-relative addressing, the
hook is REFUSED (logged, skipped) and the function runs
uninstrumented — never mis-traced (ADR-0009).

### Runtime diagnostics

`RETRACE_WIN_DIAG=1` prints breadcrumb tags at every engine
dispatch and action step (pure Win32 I/O — no CRT, so it cannot
recurse through the hooks). The last tag before a crash names
the failing step; the wrapper layer prints per-function entry
counts (a runaway count = a trampoline loop). This is the
technique that cracked the v2.13-era MSVC crashes — two
three-release-old bugs (a trampoline that jumped back into its
own patch, and 32-bit `long` truncating every pointer argument
under LLP64).

## The file-access jail

The sandbox `allow_paths` jail (cookbook recipe 34) works on
Windows exactly as on POSIX, including the one-shot capture —
there is no preload on Windows, so `retrace-profile capture`
delegates the launch to `retrace-win-run` (found next to the
profiler, with `retrace.dll`):

```bat
retrace-profile capture --jail-out jail.json -- myapp.exe
retrace-profile jail candidate.json --inside inside.json -o jail.json
set RETRACE_JSON_CONFIG=jail.json
build\tools\retrace-win-run myapp.exe
```

For full jail coverage of Win32-direct callers, run the jail
WITH the ntdll layer (`RETRACE_WIN_NTDLL=1`) — the ucrt jail
only polices calls that go through the CRT.

## Claims vs truth (kernel layer)

The kernel-layer truth capture on Windows is procmon (ETW):
capture a CSV, convert with `retrace-procmon2retrace`, and feed
`retrace-profile --kernel` (see cookbook recipes 33-34 for the
full flows). With the ntdll layer enabled, retrace itself covers
the Win32-direct depth at the ntdll boundary.

## What runs where (v2.15.0)

| Capability | windows-x64 | windows-arm64 (MSVC) | windows-arm64 (MinGW/Clang) |
|---|---|---|---|
| Engine + registries (PE-section walk) | yes | yes | yes |
| ucrt/ntdll inline hooks + wrappers | yes (MASM) | yes (armasm64, TODO.trace-profile/08) | yes (gas dialect) |
| `retrace-profile capture` | yes (delegates to retrace-win-run) | yes | yes |
| Offline tools (profile, correlate, procmon2retrace, strace2retrace) | yes | yes | yes |
| ptrace attach | n/a (Linux backend) | n/a | n/a |

## See also

- [ADR-0009](adr/0009-from-scratch-windows-trampoline.md) — why
  from-scratch hooking
- [cookbook/34](cookbook/34-profile-and-jail.md) — profile →
  tailor → jail
- [architecture.md](architecture.md) — the Windows backend in
  the layered model
