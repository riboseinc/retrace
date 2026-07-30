# 06 — Redirect open() paths

## Problem

A binary reads from a hardcoded path you can't change — say
`/etc/app/config.yaml` — and you want to substitute your own file
without modifying the binary or root filesystem.

## Config

`redirect-config-open.json`:

```json
{
  "intercept_scripts": [
    {
      "func_name": "open",
      "actions": [
        { "action_name": "modify_in_param_str",
          "action_params": {
            "param_name": "path",
            "new_val": "/tmp/fake-config.yaml"
          }
        },
        { "action_name": "log_params" },
        { "action_name": "call_real" }
      ]
    }
  ]
}
```

`modify_in_param_str` rewrites the `path` argument before the real
`open()` runs. `log_params` records both the original (in the
intercept log metadata) and the substituted path.

## Invocation

```sh
$ cat /tmp/fake-config.yaml
target: mock

$ cat app.c
#include <fcntl.h>
#include <stdio.h>
int main(void) {
    int fd = open("/etc/app/config.yaml", O_RDONLY);
    char buf[256];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    buf[n > 0 ? n : 0] = 0;
    printf("config: %s", buf);
    return 0;
}
$ cc app.c -o app
$ ./app
(returns real /etc/app/config.yaml or fails)

$ retrace run --config docs/cookbook/redirect-config-open.json -- ./app
config: target: mock
```

## Variations

### Redirect only specific paths

The example above redirects every `open()` to the same file. To
redirect only `/etc/app/config.yaml` and leave other opens alone,
you'd need a conditional action — currently not directly supported in
JSON; track via [issue #480](https://github.com/riboseinc/retrace/issues/480)
(engine refactor enables a per-call predicate).

Workaround: scope via `RETRACE_LOGGER_ALLOWED_FUNCS` and combine with
per-return-address routing once that lands.

### Redirect `fopen` too

`fopen()` is a separate prototype:

```json
{
  "intercept_scripts": [
    { "func_name": "open",  "actions": [ ... ] },
    { "func_name": "fopen", "actions": [ ... ] },
    { "func_name": "openat", "actions": [ ... ] }
  ]
}
```

### Redirect network ports

Same pattern for `connect()` — see
[07 — Redirect network connects](07-redirect-connect.md).

## How it works

`modify_in_param_str` looks up the parameter named `path` in the
prototype for `open` (declared in `src/core/prototypes/unistd.c`),
allocates a new string holding `new_val`, and overwrites the arg
slot in the arch-spec frame. The asm trampoline restores registers
from this frame before tail-calling real libc, so the real `open()`
sees the substituted path.

## See also

- [05 — Mock getuid() for root checks](05-mock-getuid.md)
- [07 — Redirect network connects](07-redirect-connect.md)
