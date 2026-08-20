# 34 — Profile a binary, then jail it to the profile

## Problem

Recipe 33 answers "which touches bypassed my VFS?" This recipe
answers the two questions around it:

> What does this binary actually DO (the profile), and can I make
> that the ONLY thing it is allowed to do (the jail)?

`retrace-profile` (≥ 2.12.0) reduces any retrace trace to a
profile — functions called, filesystem accesses by class, env
vars read — grades it against a kernel-layer truth stream, and
statically scans the binary for raw-syscall capability. One flag
turns the result into a runnable jail config.

## The claims-vs-truth delta

Two captures of the same run tell you different things:

- **Libc layer** (retrace preload): what the binary *claims* via
  the normal API.
- **Kernel layer** (strace on Linux, procmon on Windows): what
  the kernel actually saw.

The delta is the risk headline. `kernel-only` accesses are
sub-libc: the dynamic loader, static syscalls, hand-rolled
`syscall()` gadgets — things a libc-boundary capture can never
see. A typical dynamic binary shows the loader's own opens
(`/etc/ld.so.cache`, `libc.so.6`) as kernel-only; that is
expected. A kernel-only open of *your data files* is the red
flag.

## Capture

Libc layer (POSIX; Windows uses the preload DLL):

```sh
RETRACE_JSON_CONFIG=trace-files.json \
RETRACE_LOGGER_DEF_ENA=1 RETRACE_LOGGER_DEF_STDOUT_ENA=0 \
RETRACE_LOGGER_DEF_FN=outside.json \
LD_PRELOAD=libretrace.so ./app        # DYLD_INSERT_LIBRARIES on macOS
```

Kernel layer:

- **Linux**: `strace -f -e trace=%file -o strace.log ./app`
  then `retrace-strace2retrace strace.log -o kernel.json`
- **Windows**: `retrace-win-run` injects `retrace.dll` and
  hooks the ucrt layer (`fopen`, growing set); set
  `RETRACE_WIN_NTDLL=1` to also hook the ntdll file API
  (`NtCreateFile`, `NtQueryAttributesFile`, ...) that
  Win32-direct callers use — see [windows.md](../windows.md).
  For kernel truth, capture in procmon (CSV), then
  `retrace-procmon2retrace capture.csv -o kernel.json`
  (recipe 33 has the procmon steps)
- **macOS**: guard non-system binaries — SIP blocks
  DYLD_INSERT for system binaries (which are protected anyway);
  user tools and third-party apps are traceable. For a kernel
  layer, `dtruss -f` (needs SIP-disabled boot args) or run the
  same binary under a Linux/Windows harness; without a kernel
  stream the profile's coverage header honestly says
  `kernel_layer: ABSENT`.

## Profile

```sh
retrace-profile --libc outside.json --kernel kernel.json \
    --binary ./app -o profile.json
```

`profile.json`:

```json
{
  "profile": {
    "entries": 26,
    "functions": [ { "name": "open", "count": 3 } ],
    "env":     [],
    "net":     [],
    "accesses": [
      { "path": "/vfs/entry.dat",   "class": "read",  "hits": 1 },
      { "path": "/vfs/leaked.dat",  "class": "read",  "hits": 1 }
    ]
  },
  "coverage": { "libc_layer": "captured", "kernel_layer": "captured" },
  "risk": {
    "agreed": 3, "libc_only": 0, "kernel_only": 4,
    "verdict": "SUBLIBC_ACCESS_FOUND",
    "kernel_only_accesses": [
      { "path": "/etc/ld.so.cache", "hits": 1 }
    ]
  },
  "static_capability": {
    "raw_syscall_gadgets": 0,
    "ntdll_imports": 0
  }
}
```

`static_capability` counts `syscall`/`svc` instruction gadgets
in the binary's executable segments (ELF, Mach-O, PE) and, on
PE, the imported ntdll entry points. Zero does not prove purity
(a gadget can hide behind computed addresses) but any non-zero
count means the binary can talk to the kernel without libc.

## One-shot capture

`retrace-profile capture` runs the whole capture step in one
command (POSIX):

```sh
retrace-profile capture -o profile.json --inside inside.json     --jail-out jail.json -- ./app args...
```

The built-in default config scopes tracing to the file/env/net
function set (set `RETRACE_JSON_CONFIG` to override). Windows:
see `examples/profile-hunting/run-windows.md`.

## Tailor

The profile is data: edit it, feed it to an audit, and when the
build changes, DIFF it — `retrace-profile diff baseline.json
candidate.json` reports new paths, class escalations
(read → write is the headline), and new functions; exit 1 when
drift exists (CI-able):

```
profile-diff: baseline 26 entries, candidate 31 entries
  + /vfs/telemetry.dat (added, write, 3 hits)
  ! /vfs/config.dat (read -> write)
  + fn fopen64 (new)
profile-diff: DRIFT FOUND
```

The jail step below is where the tailoring bites. Validate any
hand-edited profile against the contract with
`retrace-profile validate profile.json` (schema:
`share/profile-schema.json`).

## Jail

```sh
retrace-profile --libc outside.json --inside inside.json \
    --jail-out jail.json
```

`--inside` is the DECLARED set (a VFS materialize log like
recipe 33's `inside.json`, or a hand-written list) — the jail
allowlist must not come from the observed trace, or it would
allowlist its own escapes. Without `--inside` the jail
self-describes a known-good run (change detection).

`jail.json` (abridged) — one script per observed function,
deny-by-default, `call_real` for what passes:

```json
{ "intercept_scripts": [
  { "func_name": "open",
    "actions": [
      { "action_name": "sandbox",
        "action_params": { "allow_paths": [
          "/vfs/entry.dat", "/vfs/settings.dat" ] } },
      { "action_name": "call_real" }
    ] } ] }
```

Enforce:

```sh
RETRACE_JSON_CONFIG=jail.json \
LD_PRELOAD=libretrace.so ./app        # DYLD_INSERT_LIBRARIES on macOS
```

Windows: `set RETRACE_JSON_CONFIG=jail.json` then
`retrace-win-run myapp.exe` (with `RETRACE_WIN_NTDLL=1` for
Win32-direct callers).

Every observed-function call whose path is not on the list dies
with `EACCES` before libc executes it:

```
sandbox: DENIED '/vfs/leaked.dat' (not in allow_paths)
```

To extend the jail, add paths to `allow_paths` (prefix entries
end with `/`) or re-profile after adding the function to
coverage. Deny-list mode (recipe 20) and allow-list mode
compose: a path must pass both lists when both are given.

Runnable end-to-end: `examples/profile-hunting/run-posix.sh`.

## Layer honesty

A jail enforced at the libc boundary only jails calls that go
through libc. Pair it with the delta check (above) and the
capability scan: if `raw_syscall_gadgets` is non-zero or
`kernel_only` touches your data, the libc jail is not enough —
use a kernel-enforced sandbox (seccomp, sandbox-exec, AppContainer)
for that binary.
