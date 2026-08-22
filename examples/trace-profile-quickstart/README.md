# trace-profile-quickstart: the whole loop, runnable

The core retrace story as one demo per platform
(TODO.trace-profile/16): **capture -> validate -> diff (the
upgrade drifted) -> jail -> the jailed run denies what the
profile did not declare**.

The target (`app.c`) reads a DECLARED file, an UNDECLARED file,
and `$HOME`; pass `upgraded` to also touch `new-feature.dat` --
the drift the diff must catch.

| Platform | Runner | Kernel truth |
|---|---|---|
| Linux | `./run-linux.sh [build-dir]` | strace (see recipe 34) |
| macOS | `./run-macos.sh [build-dir]` | dtruss (SIP off) |
| FreeBSD | `./run-freebsd.sh [build-dir]` | truss |
| Windows | `run-windows.bat [build-dir]` (VS prompt) | procmon (manual) |

What you should see: the diff reports `+ new-feature.dat`;
the jailed run still prints `declared:` but the undeclared read
no longer happens (denied before libc executes; a denied
pointer-returning call yields NULL, an int call yields -1).

Every artifact these steps produce -- the trace, the profile
doc, the drift report, the jail config -- is annotated in
[docs/reports.md](../../docs/reports.md); the per-platform
requirements and honest limitations are in
[docs/platforms.md](../../docs/platforms.md).
