# 36 — Live-stream OTLP spans from the traced process

## Problem

You want to see what your program is doing RIGHT NOW in an
OpenTelemetry-compatible dashboard (Grafana Tempo, Jaeger,
otelcol+anything, Honeycomb, ...). Polling the retrace JSON log
on disk and shipping it through `retrace-to-otlp` is a batch
workflow — you see what happened 30 seconds ago, not what's
happening right now.

`RETRACE_OTLP_ENDPOINT=URL` makes retrace emit OTLP protobuf
spans LIVE as the wrapped calls happen. The wrapper path is
unchanged; a tiny MPSC enqueue (cheap, lock-free, bounded) is
the only added work on the hot path. The actual network I/O
happens on a dedicated background thread that owns the
exporter's HTTP connection.

## Run

```sh
RETRACE_OTLP_ENDPOINT=http://localhost:4318 \
  RETRACE_LOGGER_DEF_ENA=1 \
  RETRACE_LOGGER_DEF_STDOUT_ENA=0 \
  RETRACE_LOGGER_FMT=jsonl \
  LD_PRELOAD=build/src/v2/libretrace.so \
    your-binary
```

(The `RETRACE_LOGGER_*` block is the standard lock-free logger
config; the only new variable is the endpoint URL.)

You'll see, at process exit:

```
retrace: otlp_live: emitted=623 sent=623 dropped_full=0 dropped_err=0 \
  endpoint=http://localhost:4318
```

## What lands in the collector

One span per traced call. Each span carries:

- `retrace.func` — intercepted function name (`malloc`,
  `connect`, `printf`, ...).
- `retrace.module` — engine module tag (`FUNCS`, `ACT`, ...).
- `retrace.severity` — log severity (when present).
- `retrace.pid`, `retrace.tid` — process / thread identity.
- `retrace.call_duration_us` — when the call went through
  `call_real` (the engine records the per-call duration).
- `retrace.ret_val` — the real return value (numeric).
- `retrace.params` — the JSON of the parsed parameter list.

Trace ID is the same for every span in a process (per-trace
"this process did all this"). Span ID is unique per call.

## Failure modes (by design)

- **Endpoint down** → the MPSC queue fills, the exporter drops
  with `dropped_full` counted. The traced target's behavior is
  NEVER blocked or crashed by the exporter side.
- **Permanent 4xx (404, etc.)** → the exporter marks the batch
  as failed with `dropped_err` counted. The exporter backs off
  with exponential delay and retries; eventual permanent loss
  is counted, never infinite hang.
- **otlp-c library OOM** → `otlp_exporter_emit` returns
  `OTLP_ERR_NOMEM`; the span is freed and dropped (no leak).
- **Process killed mid-stream** → the destructor is not called;
  any spans still in the MPSC queue are lost. By design:
  bounded memory, no temp files, no flush on disk.

## Wire shape

otlp-c posts OTLP/HTTP protobuf to `{endpoint}/v1/traces`
(default port 4318). The endpoint you set should be the
**base** URL — otlp-c appends `/v1/traces` itself. So set
`http://collector:4318`, NOT `http://collector:4318/v1/traces`.

Plain HTTP only. For TLS, terminate at a local otelcol sidecar
or set up a TLS-terminating reverse proxy in front of an
insecure endpoint — otlp-c is HTTP/1.1 with optional keep-alive.

## How it doesn't recurse (the NtWriteFile lesson)

Three things had to be right, in order:

1. **Permanent reentrance guard** on the exporter's own
   background thread: its send/connect/recv through the
   hooked libc must pass through to the real impl, not be
   re-trapped. `retrace_reentrance_guard_enter_permanent`
   sets the guard active for the thread's lifetime.
2. **Permanent reentrance guard on the flusher thread too**:
   the flusher is the one calling `otlp_exporter_emit`,
   which calls into the otlp-c library, which calls
   `g_allocator.alloc` (configured to `retrace_real_impls.malloc`).
   That real malloc is the dlsym'd one — no trampoline, no
   engine — but if any other libc call slips in (and otlp-c
   does call `strlen` etc. internally), the flusher's
   `send`/`connect`/whatever must not be re-trapped.
3. **otlp-c's allocator wired to `retrace_real_impls.malloc/free`**
   (with a thin `malloc+memcpy+free` realloc shim). Without
   this, otlp-c's internal allocations go through libc, get
   trapped, recurse into the engine, and crash.

## See also

- TODO 30 (Wave A: `retrace-to-otlp` batch tool) — the
  predecessor; if you have an existing JSON log and just want
  to ship it once, use the batch tool. Use live streaming when
  you want the *next* event, not the *last* event.
- TODO 32 (Wave C: security events as OTLP logs) — jail denials,
  fuzz crash clusters, drift hits surface as OTLP log records.
- TODO 28 (the NtWriteFile recursion fix that proved the
  permanent-guard pattern works for "internal threads own
  their own dispatch").
