# Development guide

How to build, test, and contribute to retrace itself. If you're
looking to *use* retrace, start with the [tutorials](tutorials.md)
instead.

## Build from source

CMake is the only build system. Ninja is the recommended generator
(faster, handles the per-arch trampoline objects cleanly).

```sh
git clone https://github.com/riboseinc/retrace.git
cd retrace
cmake -B build -G Ninja -DRETRACE_BUILD_TESTS=ON
cmake --build build
```

Output:

```
build/src/libretrace.so          # the preloadable library
build/src/cli/retrace            # the CLI launcher
build/test/...                   # test binaries (with RETRACE_BUILD_TESTS=ON)
```

### CMake options

| Option | Default | Effect |
|---|---|---|
| `RETRACE_BUILD_V2` | `ON` | Build the retrace shared library. |
| `RETRACE_BUILD_CLI` | `ON` | Build the CLI launcher. |
| `RETRACE_BUILD_TESTS` | `OFF` | Build the per-feature test binaries in `test/`. |
| `RETRACE_BUILD_EXAMPLES` | `OFF` | Build the demos under `examples/`. |
| `RETRACE_ENABLE_RPC` | `OFF` | Build the `rpc/` subtree. |
| `RETRACE_ENABLE_ASAN` | `OFF` | AddressSanitizer. |
| `RETRACE_ENABLE_UBSAN` | `OFF` | UndefinedBehaviorSanitizer. |
| `RETRACE_ENABLE_TSAN` | `OFF` | ThreadSanitizer. |
| `RETRACE_ENABLE_COVERAGE` | `OFF` | Coverage instrumentation. |

The build does feature probes (`CheckIncludeFile`, `CheckSymbolExists`,
`CheckCSourceCompiles`) that populate `config.h` from
`cmake/config.h.cmake.in`. Per-OS selection happens via
`RETRACE_PLATFORM_{LINUX,DARWIN,FREEBSD,OPENBSD,NETBSD,WINDOWS}`.

### Install

```sh
sudo cmake --install build
```

Or use the build output directly via `RETRACE_LIB`:

```sh
RETRACE_LIB=$PWD/build/src/libretrace.so retrace trace -- ./your-binary
```

## Run the tests

CTest drives the test pyramid. Labels: `integration`, `unit`,
`smoke`, `v2`.

```sh
ctest --test-dir build                 # run all
ctest --test-dir build -L integration  # only integration
ctest --test-dir build -L v2           # only v2-targeted
ctest --test-dir build --output-on-failure
```

The integration runner is generated from `test/runtests.sh.in` via
`file(GENERATE)`. It runs each per-feature binary under v2 via
`LD_PRELOAD`/`DYLD_INSERT_LIBRARIES` and reports pass/fail counts.

Examples under `examples/*/` (dns-fuzz, getenv-fuzzing,
http-server-overflow, id-redirection, net-fuzzing, stringinject,
unsafe-system) are self-contained demos.

### Test conventions

- **Use `CHECK(cond)`, not `assert(cond)`, whenever the
  expression contains a function call whose side effects later
  code needs.** `assert()` compiles to `((void)0)` under
  `-DNDEBUG` (the CMake Release default), eliding both the check
  and the call — this class of bug once segfaulted the test
  suite on Alpine/musl + gcc -O3 while passing on glibc/macOS.
  `CHECK()` lives in `test/helpers/test_utils.h` (or a per-file
  copy); it always evaluates, prints `FAIL [file:line]`, bumps
  `tests_fail`, and returns from the test function.
- Every tool module under `tools/` has a standalone
  `test/unit/test_<module>.c` that links just that module — no
  engine, no LD_PRELOAD. Keep new modules linkable that way
  (pure logic in the module, I/O in the CLI driver).
- Fix style issues in the commit that introduces them:
  checkpatch runs per-commit over the whole PR range, so a
  fixup commit does not clear the original warning.

## Code style

`.clang-format` is Mozilla-based. Format manually if you want
consistency — there's no enforced pre-commit hook.

```sh
clang-format -i src/path/to/your-file.c
```

### checkpatch

GitHub checks via Linux `checkpatch.pl` with a long ignore list in
`ci/checkpatch.sh`. `typedefs.checkpatch` carries project-specific
typedefs. `src/config/json/parson.{c,h}` is excluded (vendored
third-party).

Run locally:

```sh
./ci/checkpatch.sh
```

Common gotchas:

- **Commit message**: don't use markdown heading underlines like
  `---` (three or more dashes on their own line). checkpatch treats
  these as malformed patch separators. Use `==` instead, or no
  underline.
- **Trailing statements**: `if (x) fprintf(...);` triggers a
  warning. Use braces: `if (x) { fprintf(...); }`.
- **`*/` on the same line as content**: put `*/` on its own line.

## Contribution workflow

1. **Fork** the repo on GitHub.
2. **Branch** from `main`: `git checkout -b my-feature`.
3. **Commit** with a clear message. Format:
   `<scope>: <imperative summary>` then a blank line then a body
   that explains the *why*. See `git log` for examples.
4. **Push** to your fork.
5. **Open a PR** against `riboseinc/retrace:main`. The CI matrix
   (~30 jobs across Linux/macOS/Windows/BSD/Android) runs on every
   PR; wait for green.
6. **Rebase-merge** when CI is green. Maintainers will do this for
   you if you don't have write access.

### What gets merged

- Bug fixes — always welcome.
- New actions — drop a `.c` file in `src/core/actions/`, register
  via `RETRACE_ACTION_REGISTER`, add a cookbook recipe showing the
  use case.
- New backend ports — extend `src/backends/` following the pattern
  in `src/backends/preload_elf/`. Self-register via the constructor
  section.
- New prototypes — add to `src/core/prototypes/<header>.c`, add the
  function to `src/v2/funcs_symbols.def`, update any per-backend
  `funcs_symbols.S`.
- Documentation improvements — cookbook recipes, tutorials,
  clarifications.

### What probably won't get merged

- Cosmetic refactors with no behavior change.
- "Spring cleaning" PRs that touch many files.
- Features without a documented use case.
- Vendored third-party code (retrace is BSD-2 clean; we want to
  keep it that way).

## Code of conduct

Ribose's standard community code of conduct applies. Be respectful,
be technical, be patient. Disagreements about architecture are fine;
personal attacks are not.

Report conduct issues by emailing the maintainers at
`opensource@ribose.com`.

## Security disclosures

**Do not open a public issue for security vulnerabilities.**

Email `security@ribose.com` with details. Acknowledgment within
48 hours. We coordinate disclosure on a timeline that works for
you.

See [SECURITY.md](https://github.com/riboseinc/retrace/security/policy)
for the full policy.

## Debugging retrace itself

retrace debugging is tricky because retrace intercepts the libc
calls that debuggers use. Workarounds:

- **Disable interception while debugging**: set
  `RETRACE_LOGGER_DEF_ENA=0`. The engine still runs (so you can
  debug it) but no log output is produced, reducing interference.
- **Use `gdb`'s `LD_PRELOAD` override**: launch gdb with
  `gdb -ex 'set environment LD_PRELOAD=' your-binary` to suppress
  retrace entirely.
- **AddressSanitizer**: build retrace itself with
  `-DRETRACE_ENABLE_ASAN=ON` to catch memory errors in retrace's
  own code.
- **`retrace_real_impls` struct**: if retrace recurses, the issue
  is almost always that some libc call inside retrace bypassed the
  real-impl indirection. Grep for direct libc calls in
  `src/core/` and route them through `retrace_real_impls.*`.

### Common debugging scenarios

**"My target segfaults under retrace"**

1. Build retrace with ASAN.
2. Run the target under retrace + ASAN.
3. The ASAN report will show whether the bug is in retrace or in
   the target's error path triggered by retrace's interception.

**"retrace hangs at startup"**

Likely a `dlsym` deadlock during `retrace_real_impls_init`. Check
that `retrace_as_init` ran first (it sets up the section data that
`dlsym(RTLD_NEXT, ...)` may need on some platforms).

**"My custom action doesn't fire"**

1. Run `retrace list-actions` — does your action show up?
2. If yes, check that your JSON config's `func_name` matches an
   intercepted function (run `retrace list-functions | grep <name>`).
3. If both check out, add a `printf` to your action's `run`
   callback and confirm the engine is dispatching to it.

## Adding a new backend

Backends live in `src/backends/<name>/`. Each backend:

1. Implements `retrace_backend_t` from
   `include/retrace/backend.h`.
2. Self-registers via a constructor in
   `__attribute__((section("__retrace_backend")))`.
3. Provides a `probe()` that returns non-zero if the backend can
   run on the current platform.
4. Provides `spawn()` that sets up the target process (env vars,
   ptrace attach, inline hooks — whatever the mechanism needs).

See `src/backends/preload_elf/` for the canonical example. The
registry walks the constructor section at startup and picks the
highest-rank backend whose `probe()` succeeds.

## Adding a new tool module

The standalone tools follow one decomposition pattern (see
[architecture.md](architecture.md#the-tooling-ecosystem) for the
current module chains):

1. **Pure module first.** Put the algorithm in its own
   `<module>.c` with a small header — no `printf`, no file I/O,
   no global CLI state. Think `threshold.c` (one predicate) or
   `lcs.c` (compute + callback).
2. **Thin CLI driver.** The tool's `main()` file owns argument
   parsing, loading, and printing. It calls the modules.
3. **Standalone test.** Add `test/unit/test_<module>.c` linking
   only the module (+ parson via the `parson_stub` include if it
   parses JSON), following the `TEST`/`CHECK` conventions above.
   Copy the CMake block from an existing tool test.

This pattern is why the audit and diff tools could be tested (and
why testing them found seven real bugs): small pure files make
wrong answers visible; monolithic CLI drivers make them
invisible.

## Adding a new action

Drop a `.c` file in `src/core/actions/`. Implement the
`RETRACE_ACTION_REGISTER` macro call:

```c
RETRACE_ACTION_REGISTER("my_action", ia_my_action,
    "Does the thing. action_params:\n"
    "  foo - required, int. The foo value.\n"
    "  bar - optional, string. The bar value.\n");
```

Then implement `ia_my_action`:

```c
static int ia_my_action(struct ThreadContext *t_ctx,
                        const JSON_Object *action_params)
{
    double foo;
    const char *bar;

    if (!action_params) {
        log_err("action_params required for my_action");
        return -1;
    }

    if (!json_object_has_value(action_params, "foo")) {
        log_err("'foo' required for my_action");
        return -1;
    }
    foo = json_object_get_number(action_params, "foo");

    bar = json_object_get_string(action_params, "bar");

    /* ... do the thing ... */
    return 0;
}
```

The build system picks up the new file automatically. Your action
now appears in `retrace list-actions` and is referenceable from any
JSON config.

See `src/core/actions/basic.c` for the canonical examples.

## CI

GitHub workflows under `.github/workflows/`:

- `build.yml` — Linux x64+arm64, macOS Intel+arm64, Windows MSVC
  x64+arm64 (matrix). CMake + ctest.
- `alpine.yml` — Alpine/musl x64+arm64.
- `msys.yml` — MinGW64 + UCRT64.
- `nix.yml` — builds `packages.v2/v2wrapper` from `flake.nix`.
- `checkpatch.yml` — Linux `checkpatch.pl` against the diff.
- `coverity.yml` — daily scheduled scan.
- `release.yml` — tag-triggered source tarball + binary artifacts.

`.cirrus.yml` covers FreeBSD 13/14.

CI runs every PR through the full matrix. To reproduce locally:

```sh
./ci/main.sh
```

## See also

- [Architecture](architecture.md) — how the engine, backends, and
  actions fit together.
- [Configuration reference](configuration.md) — the JSON schema.
- [CLI reference](cli.md) — every subcommand.
- [ADR index](adr/README.md) — architecture decision records.
