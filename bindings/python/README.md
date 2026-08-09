# retrace Python binding

Python config builder + CLI wrapper for retrace v2. Pure Python 3,
no C extension needed.

## Install

```sh
pip install .
```

Or just copy `retrace.py` into your project.

## Quick start

```python
from retrace import Config

# Trace HTTP + DNS traffic
config = Config.trace_network()
config.save("trace.json")

# Run a command under retrace
config.run("/usr/bin/curl https://example.com")
```

## Fluent API

```python
from retrace import Config

config = Config()
config.add("open") \
      .log_params() \
      .call_real()

config.add("malloc") \
      .log_params() \
      .call_real() \
      .memory_fuzz(fail_rate=0.05)

# Filter: only log opens where flags == 0
config.filter("open", param="flags", op="==", value=0)

config.save("debug.json")
```

## Presets

```python
Config.log_all()           # log_params + call_real for "*"
Config.fail_mallocs(5)     # first 5 mallocs pass, rest fail
Config.sandbox_paths(["/etc/shadow", "/root/"])
Config.trace_http()        # HTTP request/response decoding
Config.trace_dns()         # DNS query decoding
Config.trace_network()     # HTTP + DNS + raw sockets
```

## Available actions

All 16 registered actions are available as fluent methods on `Script`:

- `log_params()` -- log all params as JSON
- `call_real()` -- call the real libc implementation
- `modify_return(value)` -- override the return value
- `memory_fuzz(fail_rate=0.1)` -- inject malloc failures
- `delay(ms=100)` -- add latency
- `count_limit(limit=5)` -- fail after N calls
- `sandbox(deny_paths=[...])` -- deny file paths
- `addr_deny(deny_addrs=[...])` -- deny network addresses
- `filter(param="x", op="==", value=0)` -- conditional guard
- `decode_http(param="buf")` -- HTTP/1.x protocol decoder
- `decode_dns(param="buf")` -- DNS protocol decoder
- `incomplete_io(rate=0.5)` -- short reads/writes
- `fuzzing_seed(seed=42)` -- deterministic fuzzing seed
- `modify_param_int(param="x", new_int=42)` -- modify integer param
- `modify_param_str(param="x", new_str="foo")` -- modify string param
