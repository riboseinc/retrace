# 30 — System-wide file-access tracing with eBPF

## Problem

You need to see every `openat` / `close` on the entire system —
every container, every daemon, every short-lived subprocess — not
just one binary. `LD_PRELOAD` can't do this; it hooks one process
at a time, and the target has to be dynamically linked.

`retrace-ebpf` is a BPF program that hooks `sys_enter_openat` and
`sys_enter_close` tracepoints in the Linux kernel. Every syscall
on the system fires; filter to a target PID and emit retrace-JSON
to stdout. Output is byte-compatible with `retrace run`, so every
downstream tool (audit, diff, replay) works unchanged.

**Observation only.** eBPF runs in kernel context and cannot modify
arguments or skip calls. If you need intervention (mock a return
value, deny a path, fuzz a buffer), use the `LD_PRELOAD` backend
or the Frida bridge — not eBPF.

## Config

No retrace config needed — the BPF program is the config. It
hardcodes the syscalls to hook (`openat`, `close`). The Python
loader converts kernel events to retrace JSON.

To extend to more syscalls, edit the BPF source (see Variations).

## Invocation

### 1. Build the BPF program

```sh
$ clang -target bpf -O2 -g -c ebpf-bridge/retrace-ebpf.bpf.c \
    -o retrace-ebpf.bpf.o
```

Requires `clang` with BPF target support, plus `libbpf` headers
(`linux/bpf.h`, `bpf/bpf_helpers.h`). On Ubuntu: `apt install
clang libbpf-dev linux-headers-$(uname -r)`.

### 2. Run the loader

```sh
$ sudo python3 ebpf-bridge/retrace-ebpf-loader ./your-binary
```

`sudo` is required: BPF programs need `CAP_BPF` (or `CAP_SYS_ADMIN`
on older kernels). The loader attaches the tracepoint, spawns your
binary, and writes events to stdout as they arrive.

### 3. Pipe into any retrace tool

```sh
$ sudo python3 ebpf-bridge/retrace-ebpf-loader ./your-binary \
    > /tmp/trace.json
$ retrace-audit --policy share/policies/baseline.json \
    --trace /tmp/trace.json
```

## Expected output

One JSON entry per syscall, in retrace's standard format:

```json
[
{"time":1786269481,"module":"FUNCS","severity":"INFO",
 "message":{"func":"openat","args":[257,18446744073709551615,0,0],"ret_val":3}},
{"time":1786269481,"module":"FUNCS","severity":"INFO",
 "message":{"func":"close","args":[3],"ret_val":0}},
...
]
```

Args are raw integers today (the syscall's register values). See
Caveats for the path-resolution TODO.

## Variations

### Trace a specific container

The loader filters by the spawned process's PID. To trace everything
inside a Docker container, wrap the container start:

```sh
$ sudo python3 ebpf-bridge/retrace-ebpf-loader -- \
    docker run --rm -it my-image /app/server
```

The loader's filter follows child processes (it tracks PID
transitions through fork/clone), so all subprocesses are captured.

### Add more syscalls

Edit `ebpf-bridge/retrace-ebpf.bpf.c` and add a new tracepoint
SEC + a new `trace_xxx` function:

```c
SEC("tracepoint/syscalls/sys_enter_read")
int trace_read(struct trace_event_raw_read *ctx)
{
    struct event_t e = {};

    e.pid = bpf_get_current_pid_tgid() >> 32;
    e.syscall_id = 3;  /* read */
    e.args[0] = ctx->fd;
    e.args[1] = ctx->buf;
    e.args[2] = ctx->count;
    e.ts_ns = bpf_ktime_get_ns();
    bpf_perf_event_output(ctx, &events, BPF_F_CURRENT_CPU,
        &e, sizeof(e));
    return 0;
}
```

Update `retrace-ebpf-loader` to map syscall_id 3 → `"read"` in the
JSON output.

### Resolve the `openat` path arg (advanced)

The MVP emits raw integer args. To resolve the `char *pathname`
to a string, use `bpf_probe_read_user_str` in the BPF program:

```c
char path[256];
bpf_probe_read_user_str(path, sizeof(path), (void *)ctx->filename);
```

This requires a larger `event_t` struct and per-syscall handling —
left as a TODO in the MVP. See TODO.complete/29 for the roadmap.

## Caveats

- **Linux x86-64 only.** eBPF kernel surface is architecture-specific.
- **Observation, not intervention.** eBPF cannot modify arguments
  or skip calls. Use `LD_PRELOAD` or Frida for mocking / fuzzing /
  redirecting.
- **Requires root** (or `CAP_BPF`). Not suitable for unprivileged
  users; deploy with care in production.
- **Raw args only in the MVP.** Pointer args (like `openat`'s
  pathname) appear as raw addresses until you add per-syscall
  string dereference (see Variations).
- **Tracepoint ABI dependency.** The `tracepoint/syscalls/*`
  tracepoints are stable across kernel versions, but the argument
  struct layout (`trace_event_raw_openat`) can change between
  kernel releases. Pin to a tested kernel or use BTF-powered CO-RE.

## See also

- Recipe 29 — Frida bridge (per-process observation when you can't
  go through `LD_PRELOAD`, including iOS and static binaries).
- Recipe 01 — Trace every libc call (the `LD_PRELOAD` equivalent
  of this recipe, single-process, with rich args).
- Recipe 24 — Audit a trace (works unchanged on eBPF output).
