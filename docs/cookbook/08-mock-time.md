# 08 — Mock time() for time-sensitive code

## Problem

Your code branches on the current time — license expiry, token TTL,
schedule windows — and you want to test boundary conditions without
waiting for the right time to arrive.

## Config

`mock-time-fixed.json`:

```json
{
  "intercept_scripts": [
    {
      "func_name": "time",
      "actions": [
        { "action_name": "modify_return_value_int",
          "action_params": { "retval_int": 1893456000 } }
      ]
    },
    {
      "func_name": "localtime_r",
      "actions": [
        { "action_name": "log_params" },
        { "action_name": "call_real" }
      ]
    }
  ]
}
```

`time()` returns the mocked epoch. `localtime_r()` still runs (it
takes the time via its first argument, so we don't need to mock the
return — the caller passes the mocked `time()` result).

## Invocation

```sh
$ date -d @1893456000 2>/dev/null || date -r 1893456000
# 2030-01-01 00:00:00 UTC

$ cat license-check.c
#include <time.h>
#include <stdio.h>
int main(void) {
    time_t now = time(NULL);
    if (now > 1893456000) {  /* Jan 1 2030 */
        printf("license expired (now=%ld)\n", now);
        return 1;
    }
    printf("license valid (now=%ld)\n", now);
    return 0;
}
$ cc license-check.c -o license-check
$ ./license-check
license valid (now=1785400000)

$ retrace run --config docs/cookbook/mock-time-fixed.json -- ./license-check
license expired (now=1893456000)
```

## Variations

### Mock a relative offset

The current action can only set a fixed value. For "advance time by
1 hour per call", track via [#503](https://github.com/riboseinc/retrace/issues/503)
— the engine refactor enables a per-call increment action.

Workaround: use a wrapper that calls real `time()`, adds the offset,
and returns the result. This needs a custom action; see
[18-custom-action.md](18-custom-action.md) (planned).

### Mock `gettimeofday` too

Some code uses `gettimeofday()` instead of `time()`. Intercept
both:

```json
{
  "intercept_scripts": [
    { "func_name": "time",         "actions": [ ... ] },
    { "func_name": "gettimeofday", "actions": [
        { "action_name": "call_real" },
        { "action_name": "modify_return_value_int",
          "action_params": { "retval_int": 0 } }
    ] }
  ]
}
```

### Mock `clock_gettime`

`clock_gettime(CLOCK_REALTIME, ...)` is the modern API. The
prototype is in `src/core/prototypes/time.c`. Note that
`CLOCK_MONOTONIC` should NOT be mocked (it's used internally by
retrace for `call_duration_us`).

## See also

- [05 — Mock getuid() for root checks](05-mock-getuid.md)
- [06 — Redirect open() paths](06-redirect-open.md)
