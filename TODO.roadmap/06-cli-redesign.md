# 06 — CLI redesign

**Status**: [ ] pending
**Layer**: 6 (product surface — what users type)
**Depends on**: 02, 03, 04, 05
**Blocks**: 07

## Goal

Replace both existing launchers (`src/v1/retrace.c` and `src/v2/retrace_v2.c`)
with a single coherent CLI: `retrace`. Subcommand-driven, no external
dependencies beyond libc + the retrace library itself, consistent across
Linux/macOS/BSD/Windows.

## Why

Today there are two CLIs with overlapping responsibilities:

- `retrace` (v1) — `retrace [--config <path>] [--lib <path>] <binary>`
- `retrace2` (v2) — depends on libnereon for arg parsing (`-c`, `-p`, `-l`, `-d`)

Problems:
- The libnereon dependency adds 1MB+ to the binary and a build step.
- The two CLIs have different flag conventions.
- Neither has subcommands; `--help` is the only inspection.
- Neither validates a config before launch.
- Neither lists available actions/backends/prototypes.

## CLI surface

```
retrace --version
retrace --help

retrace run [--script PATH | --inline JSON] [--backend NAME]
            [--log PATH] [--log-level LEVEL]
            [--dry-run]
            -- <binary> [args...]

retrace script validate <PATH>
retrace script convert <v1.conf>      # converts text → JSON
retrace script describe <PATH>        # pretty-print with resolved actions

retrace backends list
retrace backends describe <NAME>

retrace actions list
retrace actions describe <NAME>

retrace prototypes list
retrace prototypes describe <FUNC>

retrace migrate v1-to-json <v1.conf>   # alias for `script convert`
```

`run` is the only subcommand that spawns a target. All others are pure
inspection / validation and exit 0 without side effects.

## Argument parsing

Replace libnereon with a hand-rolled parser. The needs are minimal:

- Long options (`--script`, `--backend`)
- Short options (`-s`, `-b`)
- Subcommands
- `--` terminator for the target command line
- `--help` / `--version`

Implementation: ~300 lines of C in `src/cli/args.c`. No external dep.

## File layout

```
src/cli/
├── main.c             # entry: dispatch to subcommand
├── args.h / .c        # option parser
├── cmd_run.c          # `retrace run` — calls backend->spawn()
├── cmd_script.c       # `retrace script {validate,convert,describe}`
├── cmd_backends.c     # `retrace backends {list,describe}`
├── cmd_actions.c      # `retrace actions {list,describe}`
├── cmd_prototypes.c   # `retrace prototypes {list,describe}`
├── cmd_migrate.c      # `retrace migrate v1-to-json`
├── log.h / .c         # log to stderr / file
├── usage.c            # --help text generation
└── CMakeLists.txt
```

Each `cmd_*.c` exports a single `int cmd_<name>(int argc, char **argv)`
function. `main.c` is a dispatch table — adding a subcommand is purely
additive (OCP).

## Environment variables (backward compat)

Both existing CLIs read env vars; the new CLI keeps them as aliases:

| Old env var | New flag | Notes |
|-------------|----------|-------|
| `RETRACE_CONFIG` | `--script` (text format) | auto-detect format from extension / content |
| `RETRACE_JSON_CONFIG` | `--script` (json format) | auto-detect |
| `RETRACE_CLI` | `--interactive` | PTS-attached CLI (Linux only) |
| `RETRACE_LOGGER_DEF_ENA` | `--no-log` | |
| `RETRACE_LOGGER_DEF_STDOUT_ENA` | `--log=-` or `--log=stderr` | |
| `RETRACE_LOGGER_DEF_FN` | `--log=PATH` | |

Env vars still work; flags win when both are set.

## Tasks

### [P0] Skeleton
- [ ] `src/cli/main.c` with subcommand dispatch
- [ ] `src/cli/args.c` parser
- [ ] `--version` and `--help` (auto-generated from subcommand table)
- [ ] CMake target `retrace` linking `retrace::core`

### [P0] `retrace run`
- [ ] Select backend via `--backend` or auto-probe
- [ ] Parse `--script` (auto-detect JSON vs text by content)
- [ ] Call `backend->spawn(...)` to launch target
- [ ] Stream logs to stderr or `--log` file
- [ ] `--dry-run` validates script and prints chosen backend + target without spawning

### [P0] `retrace script validate`
- [ ] Load + parse + validate, exit 0 on success / 1 on error
- [ ] Print human-readable error pointing to the offending rule

### [P0] `retrace backends list`
- [ ] Iterate `retrace_engine_list_backends()`, print name + description + rank

### [P0] `retrace actions list` / `prototypes list`
- [ ] Iterate registries, print one per line

### [P1] `retrace migrate v1-to-json`
- [ ] Parse text config, build script, serialize to canonical JSON
- [ ] Round-trip test: every example v1 conf produces equivalent JSON

### [P1] Interactive mode
- [ ] `--interactive` enables PTS-attached CLI (was: `RETRACE_CLI=1`)
- [ ] Move `src/v1/retrace_cli.{c,h}` into `src/cli/interactive.{c,h}`
- [ ] Register commands via the public action registry so users can extend

### [P1] Manpages
- [ ] `man/retrace.1` — top-level
- [ ] `man/retrace-run.1`, `man/retrace-script.1`, etc.
- [ ] Generate from AsciiDoctor source in `docs/man/`

### [P2] Shell completion
- [ ] `retrace --generate-completion=bash` / `=zsh` / `=fish`
- [ ] Hand-written, no external dep

## Acceptance criteria

- `retrace --help` lists every subcommand with one-line description.
- `retrace run --script examples/getenv-fuzzing/getenvbof-retrace.conf -- /usr/bin/env`
  produces identical output to `LD_PRELOAD=... retrace-v1 ...` for the same config.
- No link-time dependency on libnereon. `ldd retrace` shows only libc, libdl,
  libpthread, and `libretrace.so`.
- Manpage renders with `man -l man/retrace.1`.

## Open questions

- Should `retrace run` accept multiple `--script` flags that layer? Yes —
  supports the "docker layer" framing. Implementation: `retrace_script_compose`
  from TODO 04.
- Do we keep `retrace2` as an alias during transition? Lean yes — Phase 4 only.
