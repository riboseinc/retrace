# 20 — Sandbox a binary with a file-access policy

## Problem

You need to run an untrusted binary and want to prevent it from
reading sensitive files (`/etc/shadow`, `~/.ssh/id_rsa`, etc.) — but
you don't have a full container or SELinux setup. You just want a
runtime path deny-list.

## Config

`sandbox-sensitive.json`:

```json
{
  "intercept_scripts": [
    {
      "func_name": "open",
      "actions": [
        { "action_name": "sandbox",
          "action_params": {
            "deny_paths": [
              "/etc/shadow",
              "/etc/gshadow",
              "/etc/sudoers",
              "/root/.ssh/",
              "/home/"
            ]
          }
        },
        { "action_name": "call_real" }
      ]
    },
    {
      "func_name": "openat",
      "actions": [
        { "action_name": "sandbox",
          "action_params": {
            "deny_paths": [
              "/etc/shadow",
              "/etc/gshadow",
              "/etc/sudoers",
              "/root/.ssh/",
              "/home/"
            ]
          }
        },
        { "action_name": "call_real" }
      ]
    }
  ]
}
```

`sandbox` runs BEFORE `call_real`. If the path matches the deny
list, the action aborts the script — `call_real` never runs, and
the caller sees `-1` (EACCES).

Path entries ending in `/` are prefix matches (blocks everything
under that directory). Entries without trailing `/` are exact
matches.

## Invocation

```sh
$ cat probe.c
#include <fcntl.h>
#include <stdio.h>
int main(void) {
    int fd = open("/etc/shadow", O_RDONLY);
    if (fd < 0) { perror("open /etc/shadow"); return 1; }
    printf("opened /etc/shadow (fd=%d)\n", fd);
    return 0;
}
$ cc probe.c -o probe
$ ./probe
opened /etc/shadow (fd=3)

$ retrace run --config docs/cookbook/sandbox-sensitive.json -- ./probe
open /etc/shadow: Permission denied
```

The binary tried to open `/etc/shadow`, retrace's sandbox action
matched the deny list, and the call was blocked before the kernel
ever saw it.

## Variations

### Log every access (audit mode)

Remove `sandbox` and use `log_params` + `call_real` instead. You
see every file access without blocking:

```json
{
  "func_name": "open",
  "actions": [
    { "action_name": "log_params" },
    { "action_name": "call_real" }
  ]
}
```

### Block network access

Add `connect` to the policy:

```json
{
  "func_name": "connect",
  "actions": [
    { "action_name": "modify_return_value_int",
      "action_params": { "retval_int": -1 } }
  ]
}
```

Every `connect()` returns -1. The binary thinks the network is
down.

### Combine with Docker

See [docs/docker.md](../../docs/docker.md) for a container that
automatically sandboxes every binary.

## See also

- [06 — Redirect open() paths](06-redirect-open.md)
- [13 — Audit system() for injection](13-audit-system.md)
- [19 — CI fuzzing](19-ci-fuzzing.md)
