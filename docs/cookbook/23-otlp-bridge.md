# 23 — Bridge to OpenTelemetry (OTLP)

## Problem

You captured a trace with `retrace run`, and now you want to ship
those timing entries to your existing observability stack -- Jaeger,
Tempo, Honeycomb, Datadog, or any OTLP-compatible collector. Writing
a custom parser for retrace's JSON-lines log is not the answer.

`retrace-to-otlp` (built from `tools/otlp-converter/`) converts the
log to OTLP/JSON -- the standard ingest format for OpenTelemetry
collectors. No engine change, no runtime overhead.

## Config

Use any retrace config that emits `call_duration_us` entries. The
default wildcard config works:

`otlp-default.json`:

```json
{
  "intercept_scripts": [
    {
      "func_name": "*",
      "actions": [
        { "action_name": "log_params" },
        { "action_name": "call_real" }
      ]
    }
  ]
}
```

For a tighter trace, restrict to functions you care about:

`otlp-net-only.json`:

```json
{
  "intercept_scripts": [
    {
      "func_name": "connect",
      "actions": [
        { "action_name": "log_params" },
        { "action_name": "call_real" }
      ]
    },
    {
      "func_name": "send",
      "actions": [
        { "action_name": "log_params" },
        { "action_name": "call_real" }
      ]
    },
    {
      "func_name": "recv",
      "actions": [
        { "action_name": "log_params" },
        { "action_name": "call_real" }
      ]
    }
  ]
}
```

## Invocation

### 1. Capture a trace

```sh
$ retrace run --config cookbook/otlp-net-only.json \
    --log /tmp/trace.json \
    -- curl -sS https://example.com
```

### 2. Convert to OTLP/JSON

```sh
$ retrace-to-otlp < /tmp/trace.json > /tmp/spans.json
$ head -c 200 /tmp/spans.json
{
  "resourceSpans": [{
    "scopeSpans": [{
      "spans": [{
        "traceId": "00000000000000000000000000000001",
        "spanId": "0000000000000001",
```

### 3. Ship to a collector

If you have an `otelcol` running locally on the default OTLP HTTP
port:

```sh
$ curl -X POST http://localhost:4318/v1/traces \
    -H "Content-Type: application/json" \
    -d @/tmp/spans.json
```

Or pipe directly, skipping the intermediate file:

```sh
$ retrace run --config cookbook/otlp-net-only.json -- curl -sS https://example.com \
  2>&1 | retrace-to-otlp | \
  curl -X POST http://localhost:4318/v1/traces \
    -H "Content-Type: application/json" \
    -d @-
```

## Expected output

`retrace-to-otlp` writes one OTLP/JSON document to stdout. The
document has:

- One `traceId` per process (deterministic, padded to 32 hex
  chars).
- One `spanId` per call_real entry (sequential counter as 16 hex
  chars).
- `startTimeUnixNano` / `endTimeUnixNano` from the entry's `time`
  field + `call_duration_us`.
- `spanKind` = `SPAN_KIND_INTERNAL`.
- `name` = the function name from `func`.

## Variations

- **Filter at conversion time**: pipe through `jq` first to keep
  only entries for a specific function or time window:
  ```sh
  jq -c 'select(.message.func == "connect")' /tmp/trace.json | \
    retrace-to-otlp > /tmp/connect-spans.json
  ```
- **Combine multiple traces**: concatenate the OTLP/JSON outputs
  into a single batch document:
  ```sh
  jq -s '{resourceSpans: [.[].resourceSpans[]]}' \
    /tmp/spans-*.json > /tmp/batch.json
  ```
- **Combine with `decode_http`**: run `decode_http` (recipe 22)
  before `call_real` to capture method/path/status alongside
  timing. Those fields land in the log_params entry; the OTLP
  converter ignores them today, but a future P2 task will surface
  them as span attributes.

## Caveats

- `retrace-to-otlp` is a post-processing tool. If you need real-time
  streaming into a collector, wait for TODO.complete/22 (real-time
  WebSocket streaming) or use `tail -f` on the log file piped into
  the converter.
- The `traceId` is derived from the first entry's timestamp; reruns
  get different IDs. For deterministic IDs across reruns (e.g. for
  regression testing), patch `g_trace_id` in the converter source.
- Only entries that contain `call_duration_us` become spans. Other
  entries (engine noise, args-only entries) are silently skipped.

## See also

- Recipe 04 -- Time each call (for the underlying timing data).
- Recipe 22 -- Decode HTTP and DNS (to enrich spans with protocol
  fields).
- TODO.complete/21 -- OTel bridge roadmap.
