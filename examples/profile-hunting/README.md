# profile-hunting: capture, profile, jail

The cookbook recipe 34 flow as a runnable demo
(TODO.windows/08). The same packaged app as escape-hunting
(../escape-hunting/): a virtualized prefix where only the
declared files should be touched. Here the answer is a PROFILE
(what the binary does) and a JAIL (a runtime policy that lets
only the declared set through).

## Flow
| Step | Tool | Result |
|------|------|--------|
| 1. capture libc layer | retrace preload | outside.json |
| 2. capture kernel layer (optional, needs strace) | strace + retrace-strace2retrace | kernel.json |
| 3. profile | retrace-profile | profile.json (risk section when step 2 ran) |
| 4. jail | retrace-profile --jail-out | jail.json |
| 5. enforce | retrace preload + jail.json | leaked.dat DENIED |

## POSIX
```sh
./run-posix.sh <build-dir>
```
Needs a configured build tree. Without strace the kernel layer
is skipped (coverage header says ABSENT); with strace the
profile also grades the claims-vs-truth delta (the loader's own
opens show up as kernel-only -- sub-libc activity by design).

The jailed run prints:

```
read  <prefix>/entry.dat
read  <prefix>/settings.dat
miss  <prefix>/leaked.dat        <-- DENIED, not in allow_paths
```

## Windows
Same flow with `retrace-procmon2retrace` as the kernel layer
(see ../escape-hunting/run-windows.md for the procmon capture
steps). The jail config is identical: RETRACE_JSON_CONFIG plus
the preload DLL.

## macOS
Guard non-system binaries (user tools, third-party apps). SIP
blocks DYLD_INSERT for system binaries, which are protected
anyway; everything you would legitimately jail is traceable.
