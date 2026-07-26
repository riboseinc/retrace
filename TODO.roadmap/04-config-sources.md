# 04 — Config source abstraction

**Status**: [ ] pending
**Layer**: 4 (config — how a script is described)
**Depends on**: 02
**Blocks**: 05, 06

## Goal

Define a single `retrace_config_source_t` interface that produces a
`retrace_script_t` from an arbitrary input. Ship three implementations: JSON
(current v2 default), text (current v1 syntax), and programmatic (build a
script in C without parsing anything).

## Why

Today there are two incompatible config formats and no way to build a script
in-process:

- v1: line-oriented text (`getuid,0`, `connect,src,dst`, `memoryfuzzing,0.05`)
  parsed in `src/v1/common.c::rtr_get_config_*`.
- v2: JSON (`{"intercept_scripts":[...]}`) parsed in `src/v2/conf.c` using
  vendored `parson`.

An embedding test framework (the "library" use case) cannot easily construct
a script without writing JSON to a temp file and pointing `RETRACE_JSON_CONFIG`
at it. That's silly for an in-process API.

The fix: every config source, including the programmatic one, produces the
same `retrace_script_t` value object. The engine never sees JSON or text.

## Architecture

```c
/* include/retrace/config.h */

typedef struct retrace_config_source {
    const char *name;            /* "json", "text", "api" */

    /* Parse a file at the given path. Returns 0 on success, errno on failure.
     * On success, *out owns the resulting script. */
    int (*parse_file)(retrace_engine_t *eng,
                      const char *path,
                      retrace_script_t **out);

    /* Parse an in-memory buffer. Optional — sources that don't support
     * in-memory parsing set this to NULL. */
    int (*parse_buffer)(retrace_engine_t *eng,
                        const char *buf, size_t len,
                        retrace_script_t **out);
} retrace_config_source_t;

int retrace_config_register(const retrace_config_source_t *source);
const retrace_config_source_t *retrace_config_find(const char *name);

/* Builder API (the "api" config source, but exposed directly because it
 * doesn't actually parse anything). */
retrace_script_t *retrace_script_new(retrace_engine_t *eng);
int retrace_script_add_intercept(retrace_script_t *script,
                                  const char *func_glob,
                                  const retrace_intercept_rule_t **out);
int retrace_intercept_rule_add_action(retrace_intercept_rule_t *rule,
                                       const char *action_name,
                                       retrace_action_params_t *params);
```

## Config source inventory

| Name | Status | Format | Source of truth |
|------|--------|--------|-----------------|
| `json` | migrate from v2 | JSON object | `src/v2/conf.c` (parson-based) |
| `text` | migrate from v1 | line-oriented key,value | `src/v1/common.c::rtr_get_config_*` |
| `api` | new | programmatic C builder | n/a |
| `toml` | future | TOML | new — for human-friendly authoring |

The first two migrate existing parsers verbatim; they produce identical
output scripts so v1 and v2 configs work interchangeably against the new
engine.

## File layout

```
src/config/
├── interface.h                 /* retrace_config_source_t */
├── registry.c                  /* retrace_config_register/find */
├── json/
│   ├── source.c                /* was: src/v2/conf.c */
│   ├── parson.c                /* was: src/v2/parson.c — vendored, unchanged */
│   ├── parson.h
│   └── CMakeLists.txt
├── text/
│   ├── source.c                /* was: src/v1/common.c config-parsing sections */
│   ├── grammar.md              /* one-page BNF + examples */
│   └── CMakeLists.txt
└── api/
    ├── builder.c               /* retrace_script_new / _add_intercept / ... */
    └── CMakeLists.txt
```

## Text config grammar (canonicalized)

The current text format is implicit (it's whatever `rtr_get_config_*` accepts).
Document it explicitly:

```ebnf
script       ::= line*
line         ::= directive EOL
directive    ::= ident ["," arg]* [";" comment]
arg          ::= string | number | ip_address | path
ident        ::= [a-zA-Z_][a-zA-Z0-9_]*
```

Plus the existing keywords: `logtofile`, `disabledatadump`, `fuzzingseed`,
`memoryfuzzing`, `incompleteio`, `forcefollowexec`, `fuzzing-getenv`,
`showtimestamp`, `showcalltime`, `logging-global`, `logging-excluded-funcs`,
`logging-allowed-funcs`, `stacktrace-groups`, `stacktrace-disabled-funcs`,
`backtrace`, `config-test*`.

Each line becomes a single-rule script with the corresponding action
sequence. (Some directives are engine-level settings, not function rules —
they map to `retrace_engine_set_option(...)` calls instead.)

## Tasks

### [P0] Interface
- [ ] Define `retrace_config_source_t` in `include/retrace/config.h`
- [ ] Implement `src/config/registry.c`
- [ ] Define builder API types `retrace_action_params_t` (a small typed-map: name → int/string)

### [P0] JSON config source
- [ ] Move `src/v2/conf.c` → `src/config/json/source.c`
- [ ] Move vendored `src/v2/parson.{c,h}` → `src/config/json/parson.{c,h}`
- [ ] Replace direct calls to `retrace_real_impls.{fopen,fread,...}` with the engine's real-impl table reference (TODO 02)
- [ ] Output: a `retrace_script_t*` via the builder API (not a global `JSON_Object *retrace_conf`)

### [P0] Text config source
- [ ] Extract config-parsing sections from `src/v1/common.c` into `src/config/text/source.c`
- [ ] Document the grammar in `src/config/text/grammar.md`
- [ ] Output: same `retrace_script_t*` via builder API
- [ ] Backward compat: every existing `examples/*/retrace.conf*` file parses identically

### [P0] Programmatic API
- [ ] Implement `src/config/api/builder.c`
- [ ] Example:
  ```c
  retrace_script_t *s = retrace_script_new(eng);
  retrace_intercept_rule_t *rule;
  retrace_script_add_intercept(s, "malloc", &rule);
  retrace_action_params_t *p = retrace_action_params_new();
  retrace_action_params_set_double(p, "fail_rate", 0.05);
  retrace_intercept_rule_add_action(rule, "call_real", NULL);
  retrace_intercept_rule_add_action(rule, "memory_fuzz", p);
  retrace_engine_set_script(eng, s);
  ```

### [P1] Config layering (docker-layer metaphor)
- [ ] `retrace_script_compose(base, overlay)` — merge two scripts, overlay wins on conflicts
- [ ] This is the foundation for the "virtual docker layer" framing: stack scripts

### [P1] Validation
- [ ] Every action parameter is checked against the prototype at script-build time
- [ ] Unknown function name in selector = error (not silent no-op)
- [ ] Glob `*` matches anything in the prototype registry

### [P2] Migration tool
- [ ] `retrace migrate v1-to-v2 < old.conf > new.json` — converts text config to JSON
- [ ] Used in TODO 10 to ease v1 deprecation

## Acceptance criteria

- `grep -rn 'json_object_get\|json_value_init' src/core/` returns nothing.
- `grep -rn 'RETRACE_JSON_CONFIG\|RETRACE_CONFIG' src/core/` returns nothing.
- An embedding test in `test/embed/` constructs a script via the builder API
  and verifies `memory_fuzz` actually fails mallocs at the requested rate.
- Every existing `examples/*/retrace.conf*` and `src/v1/retrace.conf.example`
  produces an equivalent script via the text config source (golden-file tests).

## Open questions

- Should the JSON source be the default, or should we ship a unified YAML/TOML
  as canonical? Lean toward JSON-as-default (already shipped), TOML as future
  opt-in for hand-authored configs.
- For the "config layering" feature: should compose be order-sensitive (first
  rule wins) or last-write-wins? Lean toward last-write-wins — matches Docker
  layer semantics.
