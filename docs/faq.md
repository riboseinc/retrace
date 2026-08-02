# retrace FAQ

The questions every evaluator asks. Mirrored from the website's
[FAQ section](https://riboseinc.github.io/retrace/#faq). If yours
isn't here, open an
[issue](https://github.com/riboseinc/retrace/issues).

## Does retrace work with my language?

Yes, as long as the program compiles to a native binary. C, C++,
Rust, Go (with cgo enabled, or on Linux where Go's runtime uses
libc for some syscalls), Swift, Zig, D, Pascal — all work.

Pure managed runtimes (JIT-compiled Java, JavaScript, pure-Go
without cgo) make fewer libc calls and are less useful to trace,
but the libc calls they do make are still captured. Interpreters
(Python, Ruby, Node) — retrace traces the interpreter's own libc
activity, which often reveals what the script is doing.

## Does it work on stripped binaries?

Yes. retrace intercepts at the libc symbol level, not the binary's
own symbols. A fully stripped binary still calls libc functions by
name; those are the symbols retrace hooks. You don't need debug
info, source, or symbols in the target.

## Does it work on PIE / ASLR binaries?

Yes. Position-Independent Executables and ASLR don't affect retrace
— interposition happens at the dynamic linker level, before the
binary's load address matters. The same goes for stack-protector,
relro, and fortify hardening flags.

## How much overhead does it add?

Single-digit microseconds per intercepted call, plus JSON
serialization proportional to argument size. Most programs see
wall-clock overhead of 5–15% under default logging.

To reduce overhead:

- Set `RETRACE_LOGGER_ALLOWED_FUNCS` to only the functions you care
  about. Overhead drops sharply.
- Set `RETRACE_LOGGER_DEF_STDOUT_ENA=0` and use `--log` to write to
  a file. stdout mirroring has its own cost.
- Skip `log_params` for high-frequency calls you only want to
  modify, not observe.

The fork+engine path is assembly; there is no interpreter in the
hot loop.

## Can I use retrace in production?

Yes, with care.

- Logs may contain argument values (file paths, buffer contents,
  environment variable names) — scrub or redact before persisting.
- For long-running processes, log to a file with rotation, not
  stdout.
- The `sandbox` and `mock` actions are stateless and safe in
  production.
- The `memory_fuzz`, `incomplete_io`, and `delay` actions are not —
  they will crash or stall your program by design. Reserve them for
  test environments.

## Is retrace safe to run on a binary I'm investigating?

retrace itself doesn't modify the target binary — it preloads a
library and intercepts calls at runtime. The target runs with its
normal privileges under your normal user.

The untrusted-binary workflow (sandbox action, deny-list) is
specifically designed for running code you don't trust; see
[cookbook recipe 20](cookbook/20-sandbox.md).

## How does retrace compare to eBPF?

Different layers.

- **eBPF** runs in kernel context, observes syscalls, and can't
  (portably) modify arguments or skip calls.
- **retrace** runs in userspace, observes libc calls, and can
  mutate arguments, override return values, and inject failures.

They're complementary — eBPF for system-wide observability, retrace
for per-process control. eBPF is Linux-only; retrace works on 13
platforms.

## How does it compare to strace?

strace observes syscalls (kernel boundary) and writes them as text.
It cannot modify arguments, override returns, or inject failures.
retrace observes libc calls (userspace boundary), can mutate them,
runs on more platforms, and produces structured JSON plus
interactive HTML.

Use strace for "what is this binary doing right now?" — use retrace
for "what does this binary do under failure conditions I control?"

## Can I write my own actions?

Yes. Add one `.c` file under `src/core/actions/` that registers
itself via the `RETRACE_ACTION_REGISTER` macro. The action receives
the thread context (parsed args, return value pointer) and the JSON
`action_params`. No engine change needed. See
`src/core/actions/basic.c` for the pattern.

## What's the deal with macOS SIP?

Apple's System Integrity Protection blocks `DYLD_INSERT_LIBRARIES`
for binaries in `/usr/bin` and similar protected paths. Either copy
the target to `/tmp/` first, or disable SIP via `csrutil` (not
recommended for daily use). Third-party binaries in `/Applications`
or your home directory are unaffected.

## Is the JSON log format stable?

Within a major version, yes. New fields may be added (parse
defensively); existing fields are not renamed or removed. See the
[configuration reference](configuration.md) for the schema. The
interactive HTML viewer handles whatever fields are present.

## How do I cite retrace in academic work?

Cite the repository: `riboseinc/retrace`, version 2.1.0,
BSD-2-Clause license, available at
<https://github.com/riboseinc/retrace>. There is no formal paper
yet; if you'd like one, open an issue.

## See also

- [Tutorials](tutorials.md) — 18 scenario-driven walkthroughs.
- [Cookbook](cookbook/README.md) — 21 recipe-style recipes.
- [CLI reference](cli.md) — every subcommand.
- [Configuration reference](configuration.md) — full JSON schema.
