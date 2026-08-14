# 29 — Trace an iOS / static / running process via Frida

## Problem

`LD_PRELOAD` doesn't work for your target. Common reasons:

- It's an iOS app (no `DYLD_INSERT_LIBRARIES`).
- It's a statically-linked Linux binary (no dynamic linker to
  preload into).
- It's already running and you can't restart it.
- It's on Windows and you'd rather use Frida's existing tooling.

You still want a retrace-style call log — args, return values,
per-function — and you want the output to be consumable by every
retrace tool (audit, diff, replay, OTLP converter).

`retrace-frida.js` is a Frida script that hooks libc functions and
emits retrace-compatible JSON. Same output format, same downstream
tools, different injection mechanism.

## Config

The Frida script's configuration is the `FUNC_LIST` array at the
top of `frida-bridge/retrace-frida.js`. Edit it in place:

```javascript
const FUNC_LIST = [
    { name: "open",    argc: 3, stringArgs: [0] },   // path
    { name: "openat",  argc: 4, stringArgs: [1] },   // path
    { name: "read",    argc: 3 },
    { name: "write",   argc: 3 },
    { name: "close",   argc: 1 },
    { name: "connect", argc: 3 },
    { name: "send",    argc: 4 },
    { name: "recv",    argc: 4 },
    { name: "system",  argc: 1, stringArgs: [0] },
    { name: "getenv",  argc: 1, stringArgs: [0] },
    // ...add more here
];
```

- `argc` — number of arguments the function takes (capped at 8 for
  the frame).
- `stringArgs` — 0-based indices of arguments to dereference as C
  strings (e.g. for `open(path, flags)`, `[0]` reads up to 256
  bytes at `path` and includes the string in the args array).

## Invocation

### Install Frida

```sh
$ pip install frida-tools   # or: brew install frida
```

### Three ways to attach

```sh
# Attach to a running process by name
$ frida -l retrace-frida.js -n YourProcess > /tmp/trace.json

# Spawn a fresh process and hook from the start
$ frida -l retrace-frida.js -f ./your-binary > /tmp/trace.json

# Inject into a process you'll launch separately
$ frida -l retrace-frida.js -- ./your-binary > /tmp/trace.json
```

### Run downstream tools

The output is byte-compatible with `retrace run` output. Pipe into
any retrace tool:

```sh
$ retrace-audit --policy share/policies/baseline.json \
    --trace /tmp/trace.json
$ retrace-replay /tmp/trace.json
$ cat /tmp/trace.json | retrace-to-otlp > /tmp/spans.json
```

## Expected output

One JSON object per call, wrapped in `[ ... ]` like retrace's
native output:

```json
[
{"time":1786269481,"module":"FUNCS","severity":"INFO",
 "message":{"func":"open","args":{"path":"/etc/passwd","flags":0},"ret":3}},
{"time":1786269481,"module":"FUNCS","severity":"INFO",
 "message":{"func":"read","args":{"fd":3,"buf":"0x7f...","count":4096},"ret":1024}},
...
]
```

## Variations

### iOS app debugging

`retrace-frida.js` works in `frida-server` on jailbroken iOS and in
`frida-gadget` embedded in a test build:

```sh
# On the iOS device (jailbroken): start frida-server
$ ssh root@iphone "frida-server &"

# From your dev machine: attach to the running app
$ frida -l retrace-frida.js -U -n YourApp > /tmp/ios-trace.json
```

The output is the same JSON; `retrace-audit` runs unchanged on
macOS/Linux against the iOS-gathered trace.

### Static binary on Linux

`LD_PRELOAD` can't hook a static binary. Frida injects via ptrace
and can:

```sh
$ file ./your-static-binary
ELF 64-bit LSB executable, x86-64, version 1 (SYSV), statically linked, ...

$ frida -l retrace-frida.js -f ./your-static-binary > /tmp/trace.json
```

### Attach without restarting the target

The killer feature vs `LD_PRELOAD`:

```sh
# Target is already running with PID 12345 in production
$ frida -l retrace-frida.js -p 12345 > /tmp/trace.json
^C  # detach when done; target keeps running
```

Use this for incident response — no restart, no deployment, just
attach and gather.

**On Linux, retrace can attach natively** since v2.4.0:

```sh
$ retrace attach --log /tmp/trace.json 12345
```

Same JSON output, no Frida dependency, no JS runtime overhead.
The Frida path remains the answer when the target is not a direct
child and ptrace permissions are locked down, or on iOS/macOS
where retrace's ptrace backend does not build.

### Custom function list for a specific investigation

Edit `FUNC_LIST` to focus on what matters. For a crypto audit,
strip down to just the OpenSSL functions:

```javascript
const FUNC_LIST = [
    { name: "SSL_read",        argc: 3 },
    { name: "SSL_write",       argc: 3 },
    { name: "SSL_connect",     argc: 1 },
    { name: "SSL_get_verify_result", argc: 1 },
    { name: "X509_verify_cert", argc: 1 },
];
```

## Caveats

- Frida's overhead is higher than `LD_PRELOAD` (~10x for the hook
  itself, plus IPC for every call). Don't use Frida for perf
  measurement — use it for behavioral tracing and security review.
- `stringArgs` reads up to 256 bytes at the pointer, capped. For
  variable-length buffers, you'll see truncation; pair with
  recipe 22 (`capture_buffer` style post-call memory observation)
  when you need exact lengths.
- The script doesn't follow child processes by default. Use
  `frida -f` with `enable_spawn_gating: true` for that.
- iOS requires a jailbroken device or a test build with
  `frida-gadget` injected at build time.

## See also

- Recipe 30 — System-wide tracing with eBPF (the kernel-level
  alternative for Linux observability).
- Recipe 24 — Audit a trace (works unchanged on Frida output).
- Recipe 27 — Time-travel replay (works unchanged on Frida output).
