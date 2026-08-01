# retrace documentation

Welcome. This directory holds the reference documentation for retrace.
Start with the [`README.adoc`](../README.adoc) in the repository root for
install, quick start, and platform support.

## Where to go next

| You want to… | Go to |
|--------------|-------|
| Run a specific scenario (trace, fuzz, mock, sandbox) | [Cookbook](cookbook/README.md) — 20 recipes with copy-paste JSON |
| Add retrace to a Dockerfile or compose stack | [Docker guide](docker.md) |
| Trace an Android app via `wrap.sh` or Magisk | [Android guide](android.md) |
| Understand a design decision | [Architecture decisions (ADR)](adr/README.md) |
| Read the public API | [`include/retrace/`](../include/retrace/) |

## Documentation map

```
docs/
├── README.md              ← you are here
├── cookbook/              ← 20 recipe-driven walkthroughs
│   ├── README.md          ← index + action reference
│   ├── 01-trace-all-calls.md
│   ├── …
│   └── 20-sandbox.md
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
