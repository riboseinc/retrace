# 31 — otlp-c Wave B: live OTLP streaming from the traced process

Status: in flight
Depends on: 30 (Wave A shipped; vendored otlp-c, profile
export, end-to-end verified)

## The feature

RETRACE_OTLP_ENDPOINT=http://collector:4318 -- retrace.dll/lib
emits OTLP spans LIVE as the wrapped calls happen. otlp-c's
MPSC queue + own exporter thread is the right shape: the
wrapper path only ENQUEUES (cheap), the exporter thread owns
the sockets.

## Design (the NtWriteFile lesson)

- The exporter thread runs OUTSIDE the engine's dispatch. It
  holds the per-thread reentrance guard for its LIFETIME
  (guard is per-thread -- clean) so its connect/send/recv
  through hooked ws2_32 pass straight to the real
  implementations. This is the SAME class of bug fixed for
  NtWriteFile -- the hook system must never be reentered by
  the logger.
- One span per intercept entry (func name, module, pid/tid,
  params summary via log_params' serializer, call_duration_us
  where present); trace ID per process, span ID per call;
  attributes carry the retrace semantics (retrace.func etc).
- Bounded failure: endpoint down -> bounded queue, drop with a
  counter (never block, never crash the target). Queue depth +
  drop counters surfaced via RETRACE_OTLP_STATS=1 stderr line
  at exit.
- Shutdown: flush-on-exit with a bounded deadline (2s) in the
  logger deinit path.

## Tests

- Local: otelcol (or fixture HTTP server) + a traced target
  -> assert spans in the file; assert zero reentrancy; assert
  endpoint-down survival with drops counted.
- CI: fixture server variant (no network dependency).
- The reentrance guard is THE critical design item -- the smoke
  asserts no NtWriteFile recursion under streaming.

## Done when

A one-env-variable run produces live spans in a collector, the
target's behavior is unchanged, and the endpoint-down case is
proven harmless. v-minor release.
