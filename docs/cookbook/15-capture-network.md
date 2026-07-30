# 15 — Capture network traffic

## Problem

You want to see every byte a program sends or receives over the
network, in call order, without setting up Wireshark or tcpdump.

## Config

`capture-network.json`:

```json
{
  "intercept_scripts": [
    {
      "func_name": "socket",
      "actions": [
        { "action_name": "log_params" },
        { "action_name": "call_real" }
      ]
    },
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
    },
    {
      "func_name": "sendto",
      "actions": [
        { "action_name": "log_params" },
        { "action_name": "call_real" }
      ]
    },
    {
      "func_name": "recvfrom",
      "actions": [
        { "action_name": "log_params" },
        { "action_name": "call_real" }
      ]
    }
  ]
}
```

## Invocation

```sh
$ retrace run --config docs/cookbook/capture-network.json \
    --log /tmp/net.json -- ./your-client
$ python3 tools/logpp/logpp.py /tmp/net.json | less
[INFO ] FUNCS  socket(domain=2 type=1 protocol=6) ret=3
[INFO ] FUNCS  connect(fd=3 addr=127.0.0.1:8080 len=16) ret=0
[INFO ] FUNCS  send(fd=3 buf=0x7fff... len=78 flags=0) ret=78
[INFO ] FUNCS  recv(fd=3 buf=0x7fff... len=16384 flags=0) ret=74
```

## Variations

### Filter to just one connection

Use `RETRACE_LOGGER_ALLOWED_FUNCS` to narrow further:

```sh
$ RETRACE_LOGGER_ALLOWED_FUNCS=connect,send,recv \
    retrace run --config docs/cookbook/capture-network.json -- ./your-client
```

### Fuzz the network

Pair with `memory_fuzz` to simulate packet loss:

```json
{
  "func_name": "send",
  "actions": [
    { "action_name": "call_real" },
    { "action_name": "memory_fuzz",
      "action_params": { "fail_rate": 0.05 } }
  ]
}
```

5% of `send()` calls return -1 — test your client's retry logic.

### Use the net-fuzzing example

The repo ships a worked version with fuzzing applied:

```sh
$ retrace run --config examples/net-fuzzing/retrace.conf.json \
    -- ./test_client
```

## How it works

`send`, `recv`, `sendto`, `recvfrom` prototypes live in
`src/core/prototypes/uio.c`. The `buf` parameter is typed as a
byte-array, so `log_params` dumps up to 16 bytes (configurable) of
the buffer alongside the call. For full payload capture, post-
process the JSON.

## See also

- [13 — Audit system() for injection](13-audit-system.md)
- [07 — Redirect network connects](07-redirect-connect.md) (planned)
