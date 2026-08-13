# 28 — Live-stream a trace over WebSocket

## Problem

You're tracing a long-running server. The JSON log file is on disk
and growing, but you can't see what's happening right now without
`tail -f | jq` in a second terminal — and that's just you. You
want a live dashboard that anyone on the team can open in a browser,
or a programmatic client that reacts to events as they happen.

`retrace-ws` tails the JSON log file written by `retrace run`,
parses each entry as it lands, and broadcasts it over WebSocket to
every connected client. A built-in browser viewer is served at
`http://localhost:8765/` — zero-client-setup observability.

## Config

Any retrace config works. For live streaming you typically want
the lock-free logger (default since v2.3.0) and a focused set of
functions, because at high event rates the network becomes the
bottleneck:

`ws-net-only.json`:

```json
{
  "intercept_scripts": [
    { "func_name": "connect", "actions": [
        { "action_name": "log_params" },
        { "action_name": "call_real" } ] },
    { "func_name": "send",    "actions": [
        { "action_name": "log_params" },
        { "action_name": "call_real" } ] },
    { "func_name": "recv",    "actions": [
        { "action_name": "log_params" },
        { "action_name": "call_real" } ] }
  ]
}
```

## Invocation

### 1. Start retrace-ws (it watches the file)

```sh
$ retrace-ws /tmp/trace.json
[retrace-ws] listening on ws://localhost:8765
[retrace-ws] viewer: http://localhost:8765/
[retrace-ws] tailing /tmp/trace.json...
```

### 2. In another terminal, run retrace against the same file

```sh
$ retrace run \
    --config cookbook/ws-net-only.json \
    --log /tmp/trace.json \
    -- ./your-server
```

### 3. Open the viewer (or connect a client)

Open `http://localhost:8765/` in any browser. Events stream in
real time. Or connect a programmatic client:

```python
# Python client (websockets library)
import asyncio, json, websockets

async def main():
    async with websockets.connect("ws://localhost:8765/stream") as ws:
        async for msg in ws:
            entry = json.loads(msg)
            if entry["message"]["func"] == "connect":
                print(f"[!] connect to {entry['message']['args']['addr']}")

asyncio.run(main())
```

```javascript
// Browser client
const ws = new WebSocket("ws://localhost:8765/stream");
ws.onmessage = (e) => {
  const entry = JSON.parse(e.data);
  console.log(entry.message.func, entry.message.args);
};
```

## Expected output

Each WebSocket message is one JSON object — exactly one entry from
the retrace log. No array wrapping, no delimiters; clients just
parse line-by-line (or message-by-message).

The built-in viewer is a single HTML page served at the root URL.
It auto-connects and pretty-prints events; open it in 2 browsers
and both get the same stream.

## Variations

### Stream across machines

By default `retrace-ws` binds to localhost. Override with `--host
0.0.0.0` to expose it (use a tunnel or VPN — there's no auth):

```sh
$ retrace-ws --host 0.0.0.0 --port 8765 /tmp/trace.json
```

### Filter at the source

Don't send everything over the wire — restrict the trace up front
with `RETRACE_LOGGER_ALLOWED_FUNCS`:

```sh
$ RETRACE_LOGGER_ALLOWED_FUNCS=connect,send,recv \
    retrace run --config cookbook/ws-net-only.json \
    --log /tmp/trace.json -- ./your-server
```

### Pipe into retrace-audit for live alerting

Wire the WebSocket feed into a small Python script that runs audit
predicates live:

```python
# live-audit.py
import asyncio, json, websockets

CRITICAL_PATHS = ["/etc/shadow", "/etc/passwd"]

async def main():
    async with websockets.connect("ws://localhost:8765/stream") as ws:
        async for msg in ws:
            entry = json.loads(msg)
            args_str = json.dumps(entry.get("message", {}).get("args", {}))
            if any(p in args_str for p in CRITICAL_PATHS):
                print(f"[ALERT] {entry['message']['func']} hit {args_str}")

asyncio.run(main())
```

This is the building block for a real-time SOC2 monitor.

## Caveats

- `retrace-ws` is a Python tool (no native binary). The retrace
  release tarball ships the script under `tools/ws-stream/`; copy
  it anywhere in `$PATH`.
- The viewer uses a single JSON-streaming connection per client.
  For more than ~10 simultaneous clients, put a real WebSocket
  reverse proxy (nginx, HAProxy) in front.
- The streamer uses a brace-counting parser, not `json.loads`, so
  it can handle partial objects arriving mid-write. But it does
  assume the log writer emits well-formed entries eventually; if
  `retrace run` crashes mid-write, the last partial entry is
  dropped silently.

## See also

- Recipe 23 — Bridge to OpenTelemetry (the batch/postmortem
  alternative when you don't need live).
- Recipe 31 — VS Code extension (a richer client for this stream).
- Recipe 32 — Grafana dashboard (panel-based client for this stream).
