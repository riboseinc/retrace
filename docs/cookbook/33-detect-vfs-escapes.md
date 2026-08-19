# 33 — Detect filesystem escapes from a virtualized environment

## Problem

You run code inside a virtual filesystem — tebako's `tfs`, a
container runtime, a sandbox, a chroot-style packager — and you
need to answer one question with evidence:

> Which operations bypassed my VFS and touched the **real host
filesystem**?

An inside view alone can't answer it (by definition, the bypass
never appears inside), and an outside view alone can't either
(most touches are legitimate — the VFS itself does them). The
answer is the set difference: paths touched on the outside minus
paths the VFS saw. `retrace-correlate` computes exactly that.

## The three layers

Know which layer your captures come from, or the report will
over-claim:

| Layer | Who sees it | Example |
|---|---|---|
| Inside the VFS | the VFS's own log (tfs `TEBAKO_DEBUG_TFS`) | `open("/mnt/tfs/pkg/a.so")` served virtually |
| Libc boundary | retrace preload (POSIX) / inline hooks (Windows, in progress) | `open()`, `fopen()`, `dlopen()` — includes VFS-internal calls |
| Kernel syscall | ptrace backend, eBPF bridge, ETW/procmon | raw `openat(2)`, `NtCreateFile` — includes sub-libc callers |

**Layer honesty:** a libc-boundary capture cannot certify the
absence of raw-syscall escapes — a static binary or a hand-rolled
`syscall()` call goes straight to the kernel layer. For
sub-libc-certified reports on Linux use the ptrace backend
(recipe 29/30 cover the adjacent bridges); on Windows the kernel
layer (ETW/procmon, recipe below when the converter ships) is
currently the honest capture — the ntdll hook set is in progress. `retrace-correlate` reports what the
captures cover — no more.

## Capturing the outside stream

Every entry needs `pid` and `tid` (retrace ≥ 2.7.0 emits them
automatically) so multi-process targets and helper threads can be
grouped afterwards.

`escape-outside.json`:

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

```sh
$ RETRACE_JSON_CONFIG=escape-outside.json \
    RETRACE_LOGGER_DEF_FN=/tmp/outside.json \
    retrace run -- /path/to/packaged-app
```

The file-touching functions (`open`, `openat`, `fopen`, `stat`,
`dlopen`, ...) log their path arguments via the `ACT` entries —
`retrace-correlate` extracts paths from any string field at any
depth, so the default wildcard config works without tuning.

## Capturing the inside stream

Point the VFS's debug output at a file. tebako's `tfs` emits
retrace-shaped JSON (`TEBAKO_DEBUG_TFS=<file>`); any producer
that writes `{time, pid, tid, module, severity, message}` entries
works. The inside stream is only used for its set of paths.

## Running the correlation

```sh
$ retrace-correlate --inside /tmp/tfs.json \
                    --outside /tmp/outside.json \
                    --prefix /mnt/tfs
escape /mnt/tfs/pkg/hidden.dat func=open tid=4711
$ echo $?
1
```

- `--prefix` is the mount root of the virtual filesystem; matching
  is on path components (`/mnt/tfs2/x` is NOT under `/mnt/tfs`).
- Exit codes: `0` no escapes, `1` escapes found, `2` usage or I/O
  error — CI-friendly for gating packaged builds.
- `--json` emits the report as a JSON array for machine consumers.

Every path is normalized before the join: POSIX and Windows forms
meet. `\??\C:\pkg\x`, `\\?\C:\pkg\x`, and
`\Device\HarddiskVolume3\pkg\x` all normalize to `C:/pkg/x`
(volume 3 maps to `C:` — the documented guess; normalizing both
sides through the same function keeps the join consistent even
when the guess is wrong).

## Tolerant input

retrace writes one JSON array document; a traced process that
crashes mid-run leaves the tail truncated. The correlator yields
every complete entry from a truncated log, from a clean log, and
from JSONL — crash evidence survives.

## Parity fixtures

`tools/correlate/golden/` holds the escape-correlation fixtures
(POSIX escapes, NT path forms, truncated tail, JSONL). They are
the parity contract for any correlator — retrace's and
third-party implementations (tebako's Rust correlator runs the
same cases in its CI). Add a case by adding a directory; the
CTest loop picks it up at configure time.

## Variations

### No inside stream? Just inventory the outside touches

If you only have the outside trace and want "everything that
touched the prefix" without the set difference, `retrace-audit`
with a `path_contains` rule (recipe 24) covers it — that is the
matching half; correlation is what removes the legitimate VFS
traffic.

### Only flag writes, not reads

Log `creat`, `open` (check flags in the ACT entry), `rename`,
`unlink` by scoping the config's `func_name` — or post-filter the
JSON report.

### Group by process / thread

The JSON report carries `tid`; the log entries carry both `pid`
and `tid`, so a helper-thread escape is distinguishable from the
main thread's.

## See also

- [Recipe 24](24-audit-compliance.md) — policy-driven compliance
  findings from the same trace.
- [docs/architecture.md](../architecture.md) — the three-layer
  correlation model.
- `tools/correlate/golden/README.md` — the fixture contract.
