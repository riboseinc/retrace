# 10 — Partial I/O (short reads/writes)

## Problem

Your code calls `read()` in a loop, expecting it might return fewer
bytes than requested. You want to exercise that retry path without
waiting for a real slow input source.

## Config

`incomplete-io.json`:

```json
{
  "intercept_scripts": [
    {
      "func_name": "read",
      "actions": [
        { "action_name": "call_real" },
        { "action_name": "incomplete_io",
          "action_params": { "rate": 0.5 } }
      ]
    },
    {
      "func_name": "write",
      "actions": [
        { "action_name": "call_real" },
        { "action_name": "incomplete_io",
          "action_params": { "rate": 0.5 } }
      ]
    }
  ]
}
```

`incomplete_io` runs after `call_real` and truncates the return value
to `real_ret * rate`. So if `read()` returned 100 bytes and rate is
0.5, the caller sees 50.

## Invocation

```sh
$ cat app.c
#include <unistd.h>
#include <stdio.h>
int main(void) {
    char buf[100];
    ssize_t total = 0;
    while (total < 50) {
        ssize_t n = read(0, buf + total, 50 - total);
        if (n <= 0) break;
        total += n;
        printf("got %zd (total %zd)\n", n, total);
    }
    return 0;
}
$ cc app.c -o app
$ echo "hello world this is a test input" | ./app
got 30 (total 30)

$ echo "hello world this is a test input" \
    | retrace run --config docs/cookbook/incomplete-io.json -- ./app
got 15 (total 15)
got 7 (total 22)
got 3 (total 25)
...
```

## Variations

### Tune the rate

| `rate` | Effect |
|--------|--------|
| `1.0` | No truncation (action is a no-op). |
| `0.5` | Halve every return value. |
| `0.1` | Return 10% of bytes; great for stress-testing retry loops. |
| `0.0` | Always return 0 (EOF). |

### Combine with fuzzing_seed for reproducibility

```json
{
  "intercept_scripts": [
    {
      "func_name": "*",
      "actions": [
        { "action_name": "fuzzing_seed",
          "action_params": { "seed": 12345 } }
      ]
    },
    {
      "func_name": "read",
      "actions": [ ... ]
    }
  ]
}
```

### Apply to recv/send too

Network I/O has the same short-read/short-write semantics:

```json
{ "func_name": "recv", "actions": [ ... ] },
{ "func_name": "send", "actions": [ ... ] },
{ "func_name": "recvfrom", "actions": [ ... ] },
{ "func_name": "sendto", "actions": [ ... ] }
```

## How it works

`incomplete_io` reads the real return value from the thread context
(after `call_real` has populated it), computes
`(long)(real_ret * rate)`, and writes it back. The asm trampoline
then returns this truncated value to the caller. The actual I/O
already happened — bytes were transferred; we just lie about how many.

## See also

- [09 — Fuzz malloc failures](09-fuzz-malloc.md)
- [11 — Deterministic fuzzing seed](11-deterministic-fuzz.md)
