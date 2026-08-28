# retrace documentation

Welcome. This directory holds the reference documentation for retrace.
Start with the [`README.adoc`](../README.adoc) in the repository root for
install, quick start, and platform support.

- [platforms.md](platforms.md) — the per-platform guide: capture,
  kernel truth, jail, and each platform's honest limitations.
- [reports.md](reports.md) — every output shape annotated: trace
  entries, profiles, risk, drift, jail configs, validation.

## Where to go next

| You want to… | Go to |
|--------------|-------|
| Solve a specific problem step-by-step | [Tutorials](tutorials.md) — 27 scenario-driven walkthroughs |
| Get an answer to a common question | [FAQ](faq.md) — language support, overhead, production use, more |
| Look up a CLI subcommand or env var | [CLI reference](cli.md) |
| Look up an action or write a JSON config | [Configuration reference](configuration.md) |
| Run a specific scenario (trace, fuzz, mock, sandbox) | [Cookbook](cookbook/README.md) — 32 recipes with copy-paste JSON |
| Pick the right standalone tool for a job | [Tools overview](tools.md) — `retrace-audit`, `retrace-diff`, `retrace-replay`, `retrace-ws`, Frida/eBPF bridges, VS Code + Grafana plugins |
| Contribute, build, test, or debug retrace | [Development guide](development.md) |
| Understand the engine, backends, and actions | [Architecture](architecture.md) |
| Add retrace to a Dockerfile or compose stack | [Docker guide](docker.md) |
| Trace an Android app via `wrap.sh` or Magisk | [Android guide](android.md) |
| Understand a design decision | [Architecture decisions (ADR)](adr/README.md) |
| Run the supervisor, fleet CLI, or kernel enforcement | [Supervisor reference](supervisor.md) — retraced, retrace-ctl, enforce, the lanes |
| Read the public API | [`include/retrace/`](../include/retrace/) |
| Write a runtime agent (Python, JVM, any language) | [Runtime agents](runtime-agents.md) — the supervisor protocol for third-party agents |

## Documentation map

```
docs/
├── README.md              ← you are here
├── tutorials.md           ← 27 scenario-driven step-by-step walkthroughs
├── faq.md                 ← common questions: language support, overhead, etc.
├── cli.md                 ← CLI subcommand + env var reference
├── configuration.md       ← JSON config schema + action parameter reference
├── tools.md               ← standalone tooling ecosystem (audit, diff, replay, ws, frida, ebpf, ide plugins)
├── development.md         ← building, testing, contributing, debugging retrace
├── architecture.md        ← how engine, backends, and actions fit together
├── runtime-agents.md      ← writing a supervisor runtime agent (protocol, doctrine)
├── supervisor.md           ← retraced + retrace-ctl + enforce: the control plane
├── engine-state-machine.md ← per-call lifecycle (engine_wrapper flow)
├── cookbook/              ← 32 recipe-driven walkthroughs
│   ├── README.md          ← index + compact action reference
│   ├── 01-trace-all-calls.md
│   ├── …
│   └── 21-mock-ssl-verify.md
├── docker.md              ← container integration patterns
├── android.md             ← Android cross-compile + deploy
└── adr/                   ← architecture decision records
    ├── README.md
    ├── 0001-cmake-as-primary-build.md
    ├── …
    └── 0013-engine-mece-split.md
```

## The three-verb model

Everything retrace does is one of three verbs, applied to a libc call
as it routes through the wrapper:

| Verb     | Color     | What it means                                  | Actions                                       |
|----------|-----------|------------------------------------------------|-----------------------------------------------|
| **See**     | cyan      | Observe the call without affecting it          | `log_params`, `call_real`, `fuzzing_seed`     |
| **Control** | amber     | Rewrite arguments or return value              | `modify_in_param_str/int/arr`, `modify_return_value_int` |
| **Break**   | vermilion | Force a failure condition                      | `memory_fuzz`, `incomplete_io`, `delay`, `call_count_limit`, `sandbox` |

A single intercept script composes actions from any combination of
these categories — e.g. "log the call, rewrite the path, then
short-read the result."

## Contributing

Documentation is part of the project. Open an
[issue](https://github.com/riboseinc/retrace/issues) or pull request
if something is wrong, missing, or unclear. The same BSD-2-Clause
license covers docs and code.
