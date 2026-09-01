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
| Windows x64 + arm64 (MSVC + MinGW) | injected DLL (`retrace-win-run`) | `retrace-profile capture --` | ETW script (admin) or procmon CSV, ntdll layer | yes |
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
products watch. Kernel truth, two scripted paths: (1) ETW —
from an elevated shell, `scripts/win/etw-capture.ps1 -Target
.\app.exe` starts a Microsoft-Windows-Kernel-File trace session,
runs the target, stops it, and extracts named file events to
raw jsonl; convert with `retrace-etw2retrace -o kernel.json
etw-events.jsonl`. (2) procmon — GUI capture, export CSV,
convert with `retrace-procmon2retrace` (zero-install path, no
admin needed). Honest limits: the ETW path needs admin, and
statically-linked (/MT) binaries have no CRT DLL to hook.
They launch and run cleanly under injection (CI-proven), and
their configs parse (CRLF bug fixed in v2.31.0). The ntdll
layer (`RETRACE_WIN_NTDLL=1`) observes them at the syscall
boundary -- crash-free since v2.31.1 (a round-4 CI bisect
removed the write-path hooks: hooking `NtWriteFile` recursed
the logger's own write path into the engine until stack death;
content-level read/write truth belongs to ETW/procmon). One
correctness item remains open: the `NtCreateFile` trampoline
breaks the hooked call's success path on current images (the
call returns failure; tracked in TODO.trace-profile/28) -- path
observation under it is unreliable until that lands. CRT-level argument mutation remains
impossible for static CRTs (no `fopen` symbol to interpose).
Breadcrumbs for your own debugging: `RETRACE_WIN_DIAG=1`.

## FreeBSD

```sh
retrace-profile capture -o profile.json -- ./app args
```

Kernel truth: `truss -f -o truss.log ./app`, then
`retrace-truss2retrace -o kernel.json truss.log`.

## Packaged apps (snap / flatpak / AppImage / containers)

Claims-vs-truth at the packaging layer: the DECLARED surface
(snapcraft.yaml plugs, flatpak finish-args) converts to the
declared-set format, and the observed profile grades against it
-- accesses outside the granted surface are confinement
violations.

```sh
retrace-snap2inside -o inside.json snapcraft.yaml
# personal-files/system-files plugs: the author-declared
# read:/write: path lists become accesses ($HOME expanded)
retrace-flatpak2inside -o inside.json manifest.json   # JSON form
retrace-profile capture -o profile.json -- ./app
retrace-profile --libc profile.json --inside inside.json
```

The jail can also be EXPORTED as container policy
(`retrace-profile harden profile.json -o compose.yaml`):
read_only root, cap_drop ALL, write-class paths as rw binds,
network off when the profile shows none. Runnable demo:
`examples/packaging-audit/`.

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

## Packages and release artifacts

Since v2.67.0 the release artifacts are built by the packaging
module (CPack over the install surface):

- **Every tarball and zip carries `bin/`** — retraced, retrace-ctl,
  retrace-enforce, the kernel-truth converters, and the profiler ship
  with the library and headers. (Earlier releases carried `lib/` and
  `include/` only.)
- **`.deb` packages** (`retrace-<version>-linux-x86_64.deb`,
  `-linux-aarch64.deb`) are built and validated on the Linux CI legs:

  ```sh
  sudo apt install ./retrace-2.67.0-linux-x86_64.deb
  ```

- **Checksums**: every asset ships a `.sha256` sidecar
  (`sha256sum -c retrace-2.67.0-linux-x86_64.tar.gz.sha256`).
- **RPM** is configured in `cmake/Packaging.cmake` for source builds
  (`cd build && cpack -G RPM`); the hosted CI runners carry no
  `rpmbuild`, so release-side rpm artifacts await a repository channel.
- **Building your own** from a checkout:

  ```sh
  cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
  cmake --build build
  (cd build && cpack -G "TGZ;DEB")
  ```

The OHOS cross-built artifact remains hand-staged by design: its
notices carry the self-signing disclaimer OHOS operators need.
