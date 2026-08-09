# retrace-ebpf

An [eBPF](https://ebpf.io/) backend for retrace. Pairs with `retrace-audit`,
`retrace-diff`, `retrace-replay`, `retrace-to-otlp` -- every retrace tool that
reads JSON logs can consume eBPF-captured traces.

## Use cases

- You can't use `LD_PRELOAD` (static binary, container with restrictions).
- You want kernel-level syscall visibility (not just libc calls).
- You're already using eBPF infrastructure and want retrace's analysis.

## What this is

Two pieces:

- **`retrace-ebpf.bpf.c`** -- a BPF program (kernel-side) that hooks the
  `sys_enter_openat` and `sys_enter_close` tracepoints and emits one event
  per call via `bpf_perf_event_output`.
- **`retrace-ebpf-loader`** -- a Python script that loads the BPF object,
  attaches to the tracepoints, polls events, and writes them to stdout in
  retrace JSON format.

## Important: eBPF vs retrace's mutate-and-redirect model

retrace's headline feature is **mutate-and-redirect**: intercept a libc call,
rewrite its arguments, or replace its return value. eBPF **cannot do this** --
eBPF programs run in kernel context and can only observe syscalls, not modify
them. eBPF fits observation workloads; for mutation, use the regular LD_PRELOAD
backend.

## Usage

```sh
$ clang -target bpf -O2 -g -c retrace-ebpf.bpf.c -o retrace-ebpf.bpf.o
$ sudo python3 retrace-ebpf-loader ./your-binary > /tmp/trace.json
$ echo ']' >> /tmp/trace.json   # close the JSON array manually
$ retrace-audit --policy baseline.json --trace /tmp/trace.json
```

## Output format

Matches retrace's JSON array format. Each entry has `time`, `module`,
`severity`, and a `message` with `func`, `args` (integer array), `ret_val`.

## Limitations (MVP scope)

- Linux x86-64 only (eBPF kernel surface).
- Two syscalls (`openat`, `close`). Add more by following the pattern in `retrace-ebpf.bpf.c`.
- Args are raw integers. Dereferencing user pointers (e.g. `char *pathname`
  in `openat`) needs `bpf_probe_read_user_str` and per-syscall customization
  -- left as TODO.
- No filtering; every call from the target process is logged.
- The Python loader's attach loop is a skeleton. The actual `libbpf` Python
  bindings vary by distro; users port the standard `libbcc` examples to this
  script's structure.
- eBPF is observation-only (see above).

## See also

- TODO.complete/29 -- eBPF backend roadmap
- TODO.complete/28 -- Frida backend (cross-platform, userspace interceptor)
