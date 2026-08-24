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
| OpenBSD/NetBSD | `./run-openbsd.sh [build-dir]` | ktrace/kdump |
| Windows | `run-windows.bat [build-dir]` (VS prompt) | ETW script (admin) or procmon CSV |

Note: Windows `capture` writes full profiles since v2.32.0
(two silent failures fixed: scripted `call_real` never ran for
>6-argument ntdll functions, and the capture launcher never
told the child where to write its trace).

What you should see: the diff reports `+ new-feature.dat`;
the jailed run still prints `declared:` but the undeclared read
no longer happens (denied before libc executes; a denied
pointer-returning call yields NULL, an int call yields -1).

Every artifact these steps produce -- the trace, the profile
doc, the drift report, the jail config -- is annotated in
[docs/reports.md](../../docs/reports.md); the per-platform
requirements and honest limitations are in
[docs/platforms.md](../../docs/platforms.md).
