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
`EACCES` before libc executes — pointer-returning calls deny
with NULL, int calls with -1 (prototype-driven). The allowlist
must come from the DECLARED set (`--inside`) when you have one —
the observed trace would allowlist its own escapes. Scoped to
observed functions, not `*` (a wildcard jail also jails the
dynamic loader's opens and kills startup).

The `sandbox` action accepts further policies (emitted by
`retrace-profile jail` flags, or hand-written):

- `deny_classes: ["write"]` (`--read-only`) — read-only
  detonation: ANY write-class call dies regardless of path
  (fopen "w/a/+", open O_WRONLY/O_RDWR, unlink/rename/...).
- `allow_env` / `deny_env` — env NAME policy: denied getenv
  returns NULL, denied setenv returns -1.
- `decoy_dir` (`--decoy <dir>`) — DECEPTION: instead of denying
  an undeclared READ, the path is rewritten to
  `decoy_dir/<basename>` and the real call runs against the
  decoy. A denial is a detectable signal; a plausible fake
  keeps the sample on its happy path. Every redirect is logged
  (`sandbox: DECOYED '...' -> '...'`).
- `--pin-clock <epoch>` — appends a `time()` script pinned to a
  fixed epoch (deterministic reruns for diff oracles; v1 pins
  coarse time only).

Run it:

```sh
RETRACE_JSON_CONFIG=jail.json LD_PRELOAD=libretrace.so ./app     # POSIX
set RETRACE_JSON_CONFIG=jail.json && retrace-win-run app.exe      # Windows
```

## The fuzz report

`retrace-fuzz-report` (>= 2.21.0) emits `report.json` and one
reproducer per failure cluster:

```json
{
 "iterations": 8,
 "crashes": 8,
 "assertions": 0,
 "clusters": [
  {
   "id": "17626269942680772151",
   "func": "malloc",
   "params": 0,
   "count": 1,
   "seed": "5977186486321276735",
   "kind": "crash",
   "repro": "fuzz-repro-17626269942680772151.json"
  }
 ]
}
```

id and seed are STRINGS (values exceed 2^53; JSON doubles would
lose precision). The cluster signature is (last-called
function, param count) -- a v1 heuristic; `func: "?"` marks a
death before any entry flushed (unattributable, never merged
with a named function). A reproducer is the ORIGINAL config
plus the recorded seed: `RETRACE_FUZZ_SEED=<seed>
LD_PRELOAD=... <cmd>` replays the exact failure sequence.

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
