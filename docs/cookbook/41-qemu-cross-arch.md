# 41 — Cross-arch detonation: arm64 samples on an x64 farm

## Problem

The sample is arm64; your detonation farm is x64. Rebuilding
the sample is exactly what you must not do (the binary IS the
evidence), and the aarch64 library retrace already ships was
never documented for this flow.

## Config

Cross-build the guest-arch library once on the farm host:

```sh
apt-get install -y gcc-aarch64-linux-gnu qemu-user-static cmake ninja-build

cat > xtool.cmake <<'EOF'
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)
EOF

cmake -S retrace -B build-arm64 -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=$PWD/xtool.cmake \
  -DRETRACE_BUILD_TESTS=OFF -DRETRACE_BUILD_EXAMPLES=OFF
cmake --build build-arm64 --target retrace_v2
# build-arm64/src/v2/libretrace.so  (aarch64 ELF)
```

## Invocation

The farm shape: x86_64 host, qemu-user, arm64 guest. Env
inherits normally — `LD_PRELOAD` points at the **arm64**
library; `-L` names the guest sysroot the cross toolchain
installed (without it qemu cannot find the guest loader):

```sh
LD_PRELOAD=$PWD/build-arm64/src/v2/libretrace.so \
RETRACE_JSON_CONFIG=trace-open.json \
qemu-aarch64-static -L /usr/aarch64-linux-gnu ./arm64-sample
```

`trace-open.json`:

```json
{
  "intercept_scripts": [
    { "func_name": "open",
      "actions": [ { "action_name": "log_params" },
                   { "action_name": "call_real" } ] }
  ]
}
```

Verified output (x86_64 host, Rosetta container, arm64 sample):

```json
{ "func": "open", "*path": ["/etc/hostname"], ... }
```

## Caveats (honest ones)

- **qemu's own syscalls are not the guest's**: the kernel lane
  (`retrace-ebpf-agent`) sees QEMU's process making x86_64
  syscalls — the libc-layer evidence above is the truth for
  guest behavior; grade kernel drift accordingly (or skip the
  kernel lane for emulated runs).
- Fault injection (`memory_fuzz`, `fail_first`), replay
  (`RETRACE_REPLAY_OUT`/`IN`), and redaction all work — they
  live in the guest's libc layer, which the preload owns.
- The sample must be dynamically linked (a static arm64 binary
  has no interposable libc — the ptrace lane is that story).
- musl-based samples want the alpine cross toolchain's sysroot
  at `-L` instead (the CI's alpine legs build exactly that).

## Notes

Under Docker: run the x86_64 container with `--platform
linux/amd64` (Rosetta) — everything above works unchanged,
which is how this recipe was verified.
