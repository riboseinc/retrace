# 05 — Mock getuid() for root checks

## Problem

You're testing a binary that gates behavior on `getuid() == 0`. You
don't want to run it as root, but you need it to *think* it's root so
you can exercise the privileged code path.

This is the classic setuid-binary reverse-engineering scenario.

## Config

`mock-getuid-0.json`:

```json
{
  "intercept_scripts": [
    {
      "func_name": "getuid",
      "actions": [
        { "action_name": "call_real" },
        { "action_name": "modify_return_value_int",
          "action_params": { "retval_int": 0 } }
      ]
    },
    {
      "func_name": "geteuid",
      "actions": [
        { "action_name": "call_real" },
        { "action_name": "modify_return_value_int",
          "action_params": { "retval_int": 0 } }
      ]
    }
  ]
}
```

`call_real` runs first (so the real `getuid()` still executes —
important for audit logs), then `modify_return_value_int` overwrites
the return value the caller sees.

## Invocation

```sh
$ cat check-root.c
#include <unistd.h>
#include <stdio.h>
int main(void) {
    if (getuid() != 0) { printf("denied (uid=%d)\n", getuid()); return 1; }
    printf("welcome, root\n");
    return 0;
}
$ cc check-root.c -o check-root
$ ./check-root
denied (uid=501)

$ retrace run --config docs/cookbook/mock-getuid-0.json -- ./check-root
welcome, root
```

## Variations

### Mock as a specific user

```json
{ "action_params": { "retval_int": 1000 } }
```

### Mock only one of getuid / geteuid

Some root checks compare `getuid() == geteuid()`. If you only mock
`getuid`, the check still fails (good for testing that the binary
actually compares both).

### Use the id-redirection example

The repo ships a worked version:

```sh
$ retrace run --config examples/id-redirection/get-both-redirect.json -- /tmp/id
uid=0(root) gid=20(staff) ...
```

(Copy `/usr/bin/id` to `/tmp/id` first to bypass macOS SIP.)

## See also

- [06 — Redirect open() paths](06-redirect-open.md)
- [08 — Mock time() for time-sensitive code](08-mock-time.md)
