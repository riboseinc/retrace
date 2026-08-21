# Tools overview

`retrace` ships one shared-library backend plus a small ecosystem of
standalone tools that consume the standard retrace JSON log format.
Each tool owns one job; pick the one that matches what you're trying
to do.

## When to reach for what

| You want to... | Use |
|----------------|-----|
| Trace a dynamically-linked Linux/macOS/BSD binary | `retrace run` (the core CLI; `LD_PRELOAD` / `DYLD_INSERT_LIBRARIES`) |
| Trace a Windows binary | `retrace run` (inline hooking via `CreateRemoteThread`) |
| Trace an iOS app, a static Linux binary, or a running PID | [`frida-bridge`](#frida-bridge--retrace-fridajs) |
| Observe every syscall on the system, kernel-level | [`ebpf-bridge`](#ebpf-bridge--retrace-ebpf) |
| Audit a captured trace for compliance violations | [`retrace-audit`](#retrace-audit) |
| Detect a perf regression between two builds | [`retrace-diff`](#retrace-diff) |
| Compare call-order between two runs | [`retrace-diff --order`](#retrace-diff) |
| Step backward and forward through a trace | [`retrace-replay`](#retrace-replay) |
| Stream a running trace to browsers and clients | [`retrace-ws`](#retrace-ws) |
| Browse a trace without leaving VS Code | [VS Code extension](#vs-code-extension) |
| Put a trace on a Grafana dashboard | [Grafana plugin](#grafana-plugin) |
| Ship trace spans to Tempo / Jaeger / Honeycomb | [`retrace-to-otlp`](#retrace-to-otlp) |
| Reduce a trace to what a binary does (profile) | [`retrace-profile`](#retrace-profile) |
| Turn a profile / declared set into a runtime file-access jail | [`retrace-profile --jail-out`](#retrace-profile) |
| Convert an strace file capture to the trace format | [`retrace-strace2retrace`](#retrace-strace2retrace) |
| Trace a Windows binary (inject `retrace.dll`) | [`retrace-win-run`](#retrace-win-run) |

## Tool reference

Every tool below consumes (or produces) the same JSON format — a
single array of entry objects with `time`, `module`, `severity`,
and `message` fields. Output from any producer is byte-compatible
with input to any consumer. Mix and match freely.

### `retrace-audit`

Compliance audit report generator. Reads a retrace trace, applies
a policy file (rule list with predicates over function name, args,
env vars), emits findings.

- **Output formats**: human-readable JSON, SARIF 2.1.0 (GitHub Code
  Scanning / Azure DevOps), printable PDF.
- **Built-in policies**: baseline, PCI-DSS, HIPAA, ISO 27001 (in
  `share/policies/`).
- **Custom policies**: write your own — six fields per rule, no
  code change.
- **Cookbook**: [recipe 24 — Audit a trace](cookbook/24-audit-compliance.md).

### `retrace-diff`

Differential trace analysis. Reads two traces and prints a
per-function diff of call counts and total time. Three modes:

- Default: per-function count + duration diff.
- `--threshold pct=N`: suppress changes below N% — for CI gating.
- `--order`: LCS-based call-sequence diff (like `git diff` for
  traces).
- `--stats BASE1,BASE2,...`: statistical significance via z-score
  against N baseline traces.

Exit codes double as CI gates: 0 = no diff, 1 = diff found, 2 =
error. **Cookbook**: [recipe 25](cookbook/25-diff-regression.md),
[recipe 26](cookbook/26-diff-call-order.md).

### `retrace-replay`

Interactive TUI for traces. Step forward and backward, jump to any
index, search forward by regex, list nearby events. Like `gdb` for
libc-call traces. Surfaces `capture_buffer:` entries under their
parent call. **Cookbook**: [recipe 27](cookbook/27-replay-debug.md).

### `retrace-ws`

WebSocket streamer for running traces. Tails a JSON log file as
`retrace run` writes it, parses each entry, and broadcasts to every
connected WebSocket client. A built-in browser viewer is served at
the root URL — zero-client-setup observability. **Cookbook**:
[recipe 28](cookbook/28-live-stream.md).

### `retrace-to-otlp`

Converts a retrace JSON log to OTLP/JSON — the standard ingest
format for OpenTelemetry collectors (Jaeger, Tempo, Honeycomb,
Datadog). Pairs well with recipe 04 (timing data) and recipe 22
(protocol decoding). **Cookbook**: [recipe 23](cookbook/23-otlp-bridge.md).

### `retrace-profile`

Claims-vs-truth risk profiler (≥ 2.12.0). Reduces libc-layer
traces to a profile (functions, filesystem accesses by class,
env vars, network addresses), grades them against a kernel-layer
truth stream (`--kernel`; kernel-only accesses = sub-libc risk),
statically scans the binary for raw-syscall gadgets and ntdll
imports (`--binary`), and emits a ready-to-run sandbox jail
(`--jail-out`; allowlist from `--inside`, the declared set).

Subcommands:

- `capture [-- cmd]` — one-shot: run a command under retrace and
  emit its profile (POSIX preload; Windows delegates the launch
  to `retrace-win-run`, ≥ 2.15.0).
- `diff <baseline> <candidate>` — drift report between two
  profiles/traces; exit 1 when drift exists (CI-able).
- `jail <profile.json> [--inside d.json]` — emit a jail config
  from an existing profile (≥ 2.15.0): the update-the-jail step
  of the upgrade story, no re-capture needed.
- `validate <profile.json>` — check the profile contract
  (`share/profile-schema.json`).

- **Inputs**: any standard trace (retrace capture, strace or
  procmon converted, VFS inside log), or a profile doc for
  `jail`.
- **Cookbook**: [recipe 34 — Profile a binary, then jail it](cookbook/34-profile-and-jail.md).
- **Runnable demo**: `examples/profile-hunting/`.

### `retrace-win-run`

The Windows launcher (≥ 2.13.0). No `LD_PRELOAD` on Windows:
`retrace-win-run [--lib <retrace.dll>] target.exe` creates the
target suspended, injects `retrace.dll` (hooks install + engine
boots inside the child), and resumes it under trace. Opt into
ntdll-depth hooks with `RETRACE_WIN_NTDLL=1` — see
[windows.md](windows.md).

### `retrace-profile`

The profiler (≥ 2.12.0; subcommands ≥ 2.14.0): reduce a trace to
what a binary does, grade libc claims against kernel truth, scan
static syscall capability, and emit a deny-by-default jail.
Three modes beyond the flags documented in
[cookbook 34](cookbook/34-profile-and-jail.md):

- `retrace-profile capture [-o p.json] [--inside d.json]
  [--jail-out j.json] -- cmd` — one-shot: run under the preload,
  trace, profile, jail (POSIX).
- `retrace-profile diff base.json cand.json [--json]` — drift
  between two profiles (upgrades); exit 1 on drift.
- `retrace-profile validate p.json` — check the profile against
  `share/profile-schema.json`; exit 1 on violations.

### `retrace-strace2retrace`

strace log → trace JSON. Feeds `retrace-profile --kernel` and
`retrace-correlate --outside` from an
`strace -f -e trace=%file` capture on Linux.

- **Cookbook**: [recipe 34](cookbook/34-profile-and-jail.md).

### Frida bridge — `retrace-frida.js`

Frida script that hooks libc functions and emits retrace-compatible
JSON. The escape hatch when `LD_PRELOAD` can't reach the target:
iOS apps, statically-linked binaries, already-running processes.
Output is byte-compatible with `retrace run`, so every other tool
in this list works unchanged. **Cookbook**:
[recipe 29](cookbook/29-frida-bridge.md).

### eBPF bridge — `retrace-ebpf.bpf.c`

Linux kernel-level BPF program that hooks `sys_enter_openat` and
`sys_enter_close` tracepoints. Every syscall on the system fires;
filter to a target PID and emit retrace-JSON. **Observation only**
— eBPF cannot modify arguments or skip calls. For intervention
(mocking, fuzzing, redirecting), use `LD_PRELOAD` or Frida.
**Cookbook**: [recipe 30](cookbook/30-ebpf-system.md).

### VS Code extension

VS Code extension that renders a retrace JSON log in a webview pane
inside the editor. Severity color-coded, function names highlighted,
args prettified. Also connects directly to a `retrace-ws` live
stream (recipe 28). **Cookbook**:
[recipe 31](cookbook/31-vscode-viewer.md).

### Grafana plugin

Grafana data source plugin. Loads a retrace JSON log (served over
HTTP) and exposes events as a Grafana frame — time series of
duration, bar gauge of calls per function, state timeline of
severity. Cache TTL (v2.3.0) re-fetches the trace periodically for
self-updating dashboards. **Cookbook**:
[recipe 32](cookbook/32-grafana-dashboard.md).

## Output-format compatibility

The standard retrace JSON log is one array of entry objects. Every
producer below writes this format; every consumer reads it.

**Producers**: `retrace run`, `retrace-frida.js`, `retrace-ebpf`,
`retrace-replay` (when used to extract a slice).

**Consumers**: `retrace-audit`, `retrace-diff`, `retrace-replay`,
`retrace-ws`, `retrace-to-otlp`, VS Code extension, Grafana plugin.

Pipelines compose: `retrace-frida.js` output → `retrace-audit` →
SARIF → GitHub Code Scanning. Or: `retrace-ebpf` → `retrace-ws` →
browser dashboard. No format conversion in between.

## See also

- [README.adoc](../README.adoc) — install, platform support matrix.
- [cookbook/](cookbook/) — task-driven recipes.
- [configuration.md](configuration.md) — JSON config schema.
- [cli.md](cli.md) — CLI + env var reference.
- [docs/adr/](adr/) — architecture decisions.
