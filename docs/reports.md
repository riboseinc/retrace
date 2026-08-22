# Output shapes and reports

Every artifact retrace produces, with an annotated example.
Field-level contract for the profile is machine-checked by
`retrace-profile validate` (see `share/profile-schema.json`);
this page is the human reference.

- [The trace document](#the-trace-document)
- [The profile](#the-profile)
- [The risk report](#the-risk-report)
- [The drift report](#the-drift-report)
- [The jail config](#the-jail-config)
- [Validation output](#validation-output)

## The trace document

A JSON array (or JSONL stream — the scanner accepts both and
truncated tails) of entry objects. Every producer (preload
engine, injected Windows DLL, strace/dtrace/truss/procmon
converters) emits the same shape:

```json
[
 {
  "time": 1787272825,        /* unix seconds */
  "pid":  4480,              /* process id (0 = unknown) */
  "tid":  259,               /* thread id */
  "module": "FUNCS",         /* FUNCS = engine banner; ACT = action output;
                                converter name for kernel truth */
  "severity": "INFO",
  "message": {
   "func": "fopen",          /* the function (stamped by log_params) */
   "filename": "0x100f4f29e",/* raw param (pointer as hex text) */
   "*filename": ["app.conf"],/* one-level deref: the actual value */
   "mode": "0x100f4f2f2",
   "*mode": ["rb"]
  }
 }
]
```

Banner entries carry `message.text`; return summaries carry
`message.call_duration_us` + `ret_val`. Offline tools skip both
(counting them would double every call). Path-bearing params
feed the profiler; the `detail` text feeds the write/read/probe
classifier.

## The profile

`retrace-profile --libc trace.json [-o profile.json]` (or the
`capture` one-command). The doc:

```json
{
 "profile": {
  "entries": 114,          /* trace entries consumed */
  "functions": [           /* observed functions, call counts */
   { "name": "fopen", "count": 9 }
  ],
  "env": [                 /* env var names READ */
   { "name": "HOME", "count": 1 }
  ],
  "net": [                 /* addresses contacted */
   { "name": "10.0.0.1:80", "count": 1 }
  ],
  "accesses": [            /* filesystem accesses, normalized paths */
   {
    "path": "/etc/hosts",
    "class": "read",       /* write > read > probe; most severe seen.
                              probe = existence check (stat/access) */
    "hits": 2
   }
  ]
 },
 "coverage": {
  "libc_layer": "captured",
  "kernel_layer": "ABSENT" /* present only when --kernel was given */
 }
}
```

Layer honesty: a libc-only capture cannot rule out sub-libc
accesses; the `coverage` header says so. Paths are normalized
(NT forms and mixed slashes meet POSIX forms).

## The risk report

Added when `--kernel truth.json` is given. Every kernel-layer
access is graded by whether the libc layer claims the same
normalized path:

```json
"risk": {
 "agreed": 41,             /* both layers saw it */
 "libc_only": 3,           /* libc claimed, kernel silent (filtering) */
 "kernel_only": 2,         /* THE headline: sub-libc surface */
 "verdict": "SUBLIBC_ACCESS_FOUND",
 "kernel_only_accesses": [ { "path": "/etc/master.passwd", "hits": 1 } ]
}
```

`verdict` is `clean` when `kernel_only == 0`. On Windows the
ntdll layer (opt-in, `RETRACE_WIN_NTDLL=1`) covers Win32-direct
depth at the ntdll boundary; procmon supplies the kernel truth.

## The drift report

`retrace-profile diff baseline.json candidate.json` (inputs may
be traces or profile docs). Human output, exit 1 on drift:

```text
profile-diff: baseline 114 entries, candidate 121 entries
  + /var/lib/new-cache (added, write, 3 hits)
  - /tmp/old-staging (removed)
  ! /etc/app.conf (read -> write)
  + fn connect (new)
profile-diff: DRIFT FOUND
```

`!` rows are class escalations — `read -> write` is the
headline for an upgrade review. `--json` emits the machine
shape:

```json
{
 "changes": [ { "path": "/var/lib/new-cache", "class_from": -1,
                "class_to": 3, "hits_from": 0, "hits_to": 3 } ],
 "new_functions": [ "connect" ],
 "drift": true
}
```

`class_from: -1` = added path; `class_to: -1` = removed.

## The jail config

`retrace-profile ... --jail-out jail.json`, `retrace-profile
capture --jail-out ...`, or `retrace-profile jail
<profile.json>` (the no-recapture path after a diff verdict):

```json
{
 "intercept_scripts": [
  {
   "func_name": "fopen",
   "actions": [
    {
     "action_name": "sandbox",
     "action_params": {
      "allow_paths": [ "/etc/hosts", "/usr/share/app/data" ]
     }
    },
    { "action_name": "call_real" }
   ]
  }
 ]
}
```

Deny-by-default: a path not on `allow_paths` fails with
`EACCES` before libc executes. The allowlist must come from the
DECLARED set (`--inside`) when you have one — the observed
trace would allowlist its own escapes. Scoped to observed
functions, not `*` (a wildcard jail also jails the dynamic
loader's opens and kills startup). Run it:

```sh
RETRACE_JSON_CONFIG=jail.json LD_PRELOAD=libretrace.so ./app     # POSIX
set RETRACE_JSON_CONFIG=jail.json && retrace-win-run app.exe      # Windows
```

## Validation output

`retrace-profile validate profile.json` checks the contract
(enums, required sections, the cross-field rule a JSON schema
cannot express: risk present iff the kernel layer was
captured):

```text
profile: valid
profile: 2 violations
  accesses[3].class: 'wrote' is not one of write/read/probe/none
  risk present but coverage.kernel_layer is ABSENT
```

Exit 0 valid, 1 violations/unparseable, 2 usage.
