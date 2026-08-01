# 04 — Time each call

## Problem

You want to find the hot spots in a program — which libc calls are
slow, and which are called too often. You need per-call timing, not
just per-function totals.

## Config

`time-each-call.json`:

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

This is the same as [01-trace-all-calls](01-trace-all-calls.md).
The timing comes for free: the engine measures each `call_real`
with `clock_gettime(CLOCK_MONOTONIC)` and emits `call_duration_us`
in the log entry.

## Invocation

```sh
$ retrace run --config docs/cookbook/time-each-call.json \
    --log /tmp/timed.json -- ./your-program
$ python3 tools/logpp/logpp.py /tmp/timed.json
[INFO ] FUNCS  malloc(ptr=0x... size=1024) ret=0x...
[INFO ] FUNCS  call_duration_us=0 ret_val=5133861376
[INFO ] FUNCS  open(path=/etc/hosts flags=0) ret=3
[INFO ] FUNCS  call_duration_us=87 ret_val=3
...
```

The `call_duration_us` field is microseconds. `open` took 87µs;
`malloc` took <1µs.

## Variations

### Find slow calls

```sh
$ python3 tools/logpp/logpp.py /tmp/timed.json \
    | grep call_duration_us \
    | awk -F'call_duration_us=' '{ print $2 }' \
    | awk '$1 > 1000 { print }'
```

This shows every call slower than 1ms.

### Find chatty calls

The logpp summary already counts calls per function. The top of
the list is your chatty list:

```sh
$ python3 tools/logpp/logpp.py /tmp/timed.json | tail -15
Summary
  1247 calls intercepted
  pthread_mutex_lock                  85 (  6.8%)
  pthread_mutex_unlock                85 (  6.8%)
  memset                              72 (  5.8%)
  ...
```

If `pthread_mutex_lock` is high, you have lock contention. If
`malloc` is high, you may benefit from a bulk allocator.

### Sort by total time

Combine the call count with the per-call duration:

```sh
$ python3 -c "
import json, sys
from collections import defaultdict

data = json.load(open('/tmp/timed.json'))
totals = defaultdict(lambda: [0, 0])  # func -> [count, total_us]

i = 0
while i < len(data):
    msg = data[i].get('message', {})
    func = msg.get('func')
    if func and 'call_duration_us' in msg:
        totals[func][0] += 1
        totals[func][1] += msg['call_duration_us']
    i += 1

for func, (n, t) in sorted(totals.items(), key=lambda x: -x[1][1])[:10]:
    print(f'{func:30s} {n:8d} calls  {t:10d}µs total  {t/n:8.1f}µs/call')
"
```

## How it works

The `call_real` action wraps the real libc invocation in
`clock_gettime(CLOCK_MONOTONIC)` before and after. The delta is
emitted as `call_duration_us`. This is per-call timing; aggregate
yourself via the script above.

`CLOCK_MONOTONIC` is NOT mocked by retrace, so the timing is
trustworthy even when you're mocking `time()` or `gettimeofday()`
(see [08-mock-time](08-mock-time.md)).

## See also

- [01 — Trace every libc call](01-trace-all-calls.md)
- [03 — Count calls per function](03-count-calls.md)
