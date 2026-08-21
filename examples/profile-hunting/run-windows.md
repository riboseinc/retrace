# profile-hunting on Windows

The recipe-34 flow (profile → tailor → jail) on Windows, using
the live hooks (v2.13.0+). POSIX users: see `run-posix.sh`.

## Prerequisites

Build the Windows tree (MSVC or MSYS2/MinGW):

```bat
cmake -B build && cmake --build build --config Release
```

You need `build\tools\retrace-win-run.exe`, `retrace.dll`, and
`retrace-profile.exe`.

## 1. Capture (the claims)

One-shot (v2.15.0+): `retrace-profile capture` finds
`retrace.dll` and `retrace-win-run.exe` next to itself and runs
the whole flow — default config, trace, profile — in one
command:

```bat
build\tools\retrace-profile capture -o profile.json --jail-out jail.json -- app.exe
```

Manual equivalent (any version):

```bat
set RETRACE_LOGGER_DEF_ENA=1
set RETRACE_LOGGER_DEF_STDOUT_ENA=0
set RETRACE_LOGGER_DEF_FN=outside.json
build\tools\retrace-win-run app.exe
```

The ucrt layer hooks `fopen`, `_open`, `_close`, `_read`,
`_write`, `_lseek`, `_stat`, `_unlink`, `_remove`, `_rename`,
`_rmdir` — a normal C program's file traffic lands in
`outside.json`.

To also see Win32-direct callers (apps that never touch the
CRT), set `RETRACE_WIN_NTDLL=1` — the ntdll set
(`NtCreateFile`, `NtOpenFile`, `NtQueryAttributesFile`,
`NtClose`, `LdrLoadDll`) comes online. Read [the Windows
guide](../../docs/windows.md) first: ntdll hooks are opt-in
because AV/EDR watches that layer.

## 2. Profile

```bat
build\tools\retrace-profile --libc outside.json --binary app.exe -o profile.json
build\tools\retrace-profile validate profile.json
```

Kernel truth (procmon): capture a CSV of the same run, then

```bat
build\tools\retrace-procmon2retrace capture.csv --pid <pid> -o kernel.json
build\tools\retrace-profile --libc outside.json --kernel kernel.json -o profile.json
```

`kernel_only` accesses are the sub-libc surface (see recipe 34).

## 3. Jail

From an existing profile (v2.15.0+) — the "update the jail"
step after a `retrace-profile diff` verdict:

```bat
build\tools\retrace-profile jail profile.json --inside inside.json -o jail.json
set RETRACE_JSON_CONFIG=jail.json
build\tools\retrace-win-run app.exe
```

Or straight from the trace:

```bat
build\tools\retrace-profile --libc outside.json --inside inside.json --jail-out jail.json
set RETRACE_JSON_CONFIG=jail.json
build\tools\retrace-win-run app.exe
```

Undeclared reads die with `EACCES` before libc executes them.
For Win32-direct callers run the jail with
`RETRACE_WIN_NTDLL=1`.
