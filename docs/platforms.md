# retrace on every platform

One page per question you actually have: how do I run this
HERE, where does kernel truth come from HERE, and what does
THIS platform not do. Output shapes are documented in
[reports.md](reports.md); the tool inventory in
[tools.md](tools.md).

| Platform | Interposition | One-command capture | Kernel truth | Jail |
|---|---|---|---|---|
| Linux x64/arm64 (glibc + musl/Alpine) | `LD_PRELOAD` | `retrace-profile capture --` | strace, ptrace attach | yes |
| macOS x64/arm64 | `DYLD_INSERT_LIBRARIES` (SIP limits) | `retrace-profile capture --` | dtrace/dtruss (SIP-off) | yes |
| Windows x64 + arm64 (MSVC + MinGW) | injected DLL (`retrace-win-run`) | `retrace-profile capture --` | procmon (manual), ntdll layer | yes |
| FreeBSD | `LD_PRELOAD` | `retrace-profile capture --` | truss | yes |
| OpenBSD / NetBSD | `LD_PRELOAD` (build) | — | ktrace converter: gap | yes |
| OHOS arm64 | preload via NDK | — | — | — |

## Linux

```sh
sudo cmake --install build          # or use the release tarball
retrace-profile capture -o profile.json -- ./app args
```

Kernel truth: `strace -f -e trace=%file -o strace.log ./app`
then `retrace-strace2retrace -o kernel.json strace.log`; grade
with `retrace-profile --libc trace.json --kernel kernel.json`.
Statically-linked targets: `retrace attach <pid>` (ptrace
backend) instead of the preload.

## macOS

```sh
retrace-profile capture -o profile.json -- ./app args
```

SIP strips `DYLD_INSERT_LIBRARIES` for system binaries —
`csrutil disable` in Recovery to trace those; ordinary
binaries work untouched. Kernel truth: `sudo dtruss -f ./app >
dtruss.log`, then `retrace-dtrace2retrace -o kernel.json
dtruss.log` (dtrace also needs SIP off).

## Windows

No preload exists — `retrace.dll` is injected by
`retrace-win-run` (suspended create + inject + resume; hooks
install and the engine boots inside the child before `main`).

```bat
build\tools\retrace-profile capture -o profile.json -- app.exe args
:: equivalent manual form:
build\tools\retrace-win-run [--lib retrace.dll] app.exe args
```

Hook layers (see [windows.md](windows.md) for the full guide):
ucrt file+env set and ws2_32 net set are default-on; the
ntdll depth (`NtCreateFile`, `NtWriteFile`, ... — Win32-direct
callers that never touch the CRT) is opt-in with
`RETRACE_WIN_NTDLL=1` because hooking ntdll is what AV/EDR
products watch. Kernel truth: capture with procmon, convert
the CSV with `retrace-procmon2retrace`. Honest limits: the
procmon step is manual (no scripted ETW yet), and statically-
linked binaries are not hookable (documented punt). Breadcrumbs
for your own debugging: `RETRACE_WIN_DIAG=1`.

## FreeBSD

```sh
retrace-profile capture -o profile.json -- ./app args
```

Kernel truth: `truss -f -o truss.log ./app`, then
`retrace-truss2retrace -o kernel.json truss.log`.

## The jail everywhere

```sh
retrace-profile jail candidate.json --inside declared.json -o jail.json
RETRACE_JSON_CONFIG=jail.json LD_PRELOAD=... ./app   # or DYLD_..., or retrace-win-run
```

An undeclared path fails with `EACCES` before libc executes.
The allowlist must come from the DECLARED set (`--inside`) —
the observed trace would allowlist its own escapes.

## Configuration

- `RETRACE_JSON_CONFIG` — the config/jail document (default:
  log_params + call_real for every function).
- `RETRACE_LOGGER_DEF_ENA`, `RETRACE_LOGGER_DEF_STDOUT_ENA`,
  `RETRACE_LOGGER_DEF_FN` — log output (a file is required for
  the offline tools).
- `RETRACE_WIN_NTDLL=1` — the opt-in ntdll depth (Windows).
- `RETRACE_WIN_DIAG=1` — Windows runtime breadcrumbs.
- Capture's built-in default config traces the file/env/net
  function set; an explicit `RETRACE_JSON_CONFIG` always wins.

See [configuration.md](configuration.md) for the full reference
and [cookbook/34](cookbook/34-profile-and-jail.md) for the
recipe-level walkthrough; the runnable loop lives in
`examples/trace-profile-quickstart/`.
