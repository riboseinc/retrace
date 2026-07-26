# 10 — v1 deprecation & migration

**Status**: [ ] pending
**Layer**: cross-cutting
**Depends on**: 02, 04, 06
**Blocks**: nothing

## Goal

Migrate users from v1 (line-oriented config, per-function C wrappers) to v2
(JSON config, assembly-driven engine) without breaking anyone. Eventually
remove v1 source code entirely.

## Why

v1 and v2 implement the same domain concept two different ways. Maintaining
both is a tax: every new feature must be considered for both. v2 is the
better architecture (model-driven, plugin-based, see TODOs 02-04), so v1
should be retired.

But v1 has users today (every `examples/*/retrace.conf*` file uses v1 syntax).
A hard cutover would break them.

## Migration path

### Phase 1 (current): v1 still ships, v2 marked "preferred"

- README has a deprecation notice: "v1 is in maintenance. New users should
  use v2. v1 will be removed in v3.0 (no earlier than 2027-Q1)."
- v1 gets bug fixes only — no new features.
- v2 gets all new features.
- The migration tool `retrace migrate v1-to-json` (TODO 06) ships.

### Phase 2 (after TODO 06 lands): v1 deprecated in code

- `./configure --enable-v1` prints a deprecation warning.
- `retrace` (the v1 CLI binary) prints a one-line notice on every run:
  "warning: v1 is deprecated, use `retrace migrate v1-to-json` to upgrade."
- The notice points to a migration guide URL.

### Phase 3 (v2.1): v1 still ships, build disabled by default

- `./configure` no longer builds v1 unless `--enable-v1` is passed.
- Same for CMake: `RETRACE_BUILD_V1=OFF` by default.
- Distribution packages (vcpkg, conan, deb, rpm) ship only v2.

### Phase 4 (v3.0, no earlier than 2027-Q1): v1 source removed

- `src/v1/` directory is removed.
- Old configs work via the `text` config source (TODO 04), which preserves v1
  syntax as a parser for the new engine.
- The v1 CLI's `retrace` binary becomes an alias for `retrace run --backend=preload-elf`.

**Important**: per global rule, removal of `src/v1/` source files happens only
after (a) the text config source has 100% syntax coverage and (b) explicit
user sign-off. Until then, v1 source stays in tree.

## Migration tool

`retrace migrate v1-to-json <old.conf> > new.json`:

- Parses the v1 line-oriented config via the text config source.
- Builds a `retrace_script_t` in memory.
- Serializes to canonical JSON via `retrace_script_serialize`.

One-to-one mapping for every directive:

| v1 directive | v2 JSON equivalent |
|--------------|-------------------|
| `getuid,0` | `{"func_name":"getuid","actions":[{"action_name":"modify_return_value_int","action_params":{"retval_int":0}},{"action_name":"call_real"}]}` |
| `connect,src,dst` | (custom `modify_in_param_*` actions on `connect`) |
| `fopen,/etc/passwd,/tmp/passwd` | (custom `modify_in_param_str` on `fopen`) |
| `SSL_get_verify_result,10` | `{"func_name":"SSL_get_verify_result","actions":[{"action_name":"modify_return_value_int","action_params":{"retval_int":10}},{"action_name":"call_real"}]}` |
| `memoryfuzzing,0.05` | `{"func_name":"malloc","actions":[{"action_name":"call_real"},{"action_name":"memory_fuzz","action_params":{"fail_rate":0.05}}]}` |
| `incompleteio,10` | (new `incomplete_io` action — needs to be written) |
| `showtimestamp` | `retrace_engine_set_option(eng, "log.timestamp", "true")` |
| `showcalltime,0.0001` | `retrace_engine_set_option(eng, "log.calltime_threshold", "0.0001")` |
| `logtofile,retrace.log,1` | `retrace_engine_set_option(eng, "log.path", "retrace.log")` |
| `disabledatadump` | `retrace_engine_set_option(eng, "log.enabled", "false")` |
| `forcefollowexec` | `retrace_engine_set_option(eng, "exec.follow", "true")` |
| `fuzzingseed,1498729252` | `retrace_engine_set_option(eng, "fuzz.seed", "1498729252")` |
| `fuzzing-getenv,FOOBAR,all,GARBAGE,10,1` | (custom action sequence — see examples) |
| `logging-global,LOG_GROUP_FILE\|LOG_GROUP_MEM,LOG_LEVEL_ALL` | `retrace_engine_set_option(eng, "log.groups", "file\|mem"); _set_option(eng, "log.levels", "all")` |
| `logging-excluded-funcs,free\|memcpy\|malloc` | `retrace_engine_set_option(eng, "log.excluded_funcs", "free\|memcpy\|malloc")` |
| `backtrace` | `retrace_engine_set_option(eng, "stacktrace.enabled", "true")` |
| `config-test*` | (test directives — drop or convert to no-ops) |

Directives without a clean v2 action become `log_params` + `call_real` with a
warning during migration.

## Tasks

### [P0] Documentation
- [ ] README.adoc deprecation notice (pointers to migration tool)
- [ ] New `docs/migrating-from-v1.md` walkthrough
- [ ] `CHANGELOG.md` entries per Phase

### [P0] Migration tool
- [ ] `retrace migrate v1-to-json` subcommand in `src/cli/cmd_migrate.c`
- [ ] Round-trip test: every `examples/*/retrace.conf*` produces valid JSON
- [ ] Round-trip test: parsed-v1-script equals parsed-converted-JSON-script

### [P0] Coverage of v1 syntax
- [ ] Audit `src/v1/common.c::rtr_get_config_*` for every directive
- [ ] Confirm each has a v2 equivalent or document the gap
- [ ] Gap list drives new action work (e.g., `incomplete_io` action — TODO 02)

### [P1] Build flags
- [ ] CMake: `option(RETRACE_BUILD_V1 "Build deprecated v1 implementation" ON)` for Phase 2
- [ ] CMake: same `OFF` by default in Phase 3
- [ ] Autotools: mirror with `--enable-v1` (warning emitted)

### [P1] v1 CLI binary
- [ ] `bin_PROGRAMS=retrace` stays as v1 launcher during transition
- [ ] New `bin_PROGRAMS=retrace2` becomes `bin_PROGRAMS=retrace` in Phase 3
- [ ] Old `retrace` binary renamed to `retrace-v1` for one release cycle

### [P2] Final removal (Phase 4)
- [ ] Verify text config source covers 100% of v1 syntax
- [ ] Issue deprecation notice one release in advance
- [ ] `git rm -r src/v1/` after user sign-off

## Acceptance criteria

- Every `examples/*/retrace.conf*` migrates to valid v2 JSON without warnings.
- The migration tool produces byte-identical output for the same input
  (deterministic).
- A user running `retrace migrate v1-to-json old.conf > new.json && retrace run --script new.json -- <binary>`
  sees identical behavior to `retrace --config old.conf <binary>`.

## Open questions

- Should v1 stay indefinitely as a "compatibility shim"? Lean no — the text
  config source preserves the syntax, the v1 binary code can go.
- Should the v1 launcher binary be a symlink to v2 in Phase 3? Yes, with
  auto-detection of config format.
