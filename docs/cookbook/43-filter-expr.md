# 43 — Log only what you're hunting: the filter expression

## Problem

The trace is enormous and 99% of it is noise. You want the
opens of `*.log` — not every open on the box — and you want to
say it in the config, not in a grep pipeline after the fact.

## Config

The `filter` action speaks a small expression language in its
`expr` param:

```json
{
  "intercept_scripts": [
    { "func_name": "open",
      "actions": [
        { "action_name": "filter",
          "action_params": { "expr": "path ~ \"*.log\"" } },
        { "action_name": "log_params" },
        { "action_name": "call_real" } ] }
  ]
}
```

Everything after a non-matching filter is skipped — no log, no
actions — exactly like the single-comparison form (which still
works: `param_name`/`op`/`value`).

## The language

```
disj   := conj ("or" conj)*
conj   := neg  ("and" neg)*
neg    := "not" neg | primary
primary:= "(" disj ")" | operand OP operand
OP     := == != < <= > >= ~ !~
```

with `expr` = `disj` (the start production).

Operands: any param by name (`path`, `flags`, `fd`, ...),
numbers, quoted strings, and three builtins — `func` (the
intercepted function), `ret` (the call's return value:
meaningful in scripts that run after `call_real`), `caller`
(the caller's symbol). `~` is a glob match (`*`, `?`,
`[a-z]`); `!~` its negation.

More shapes:

```json
"expr": "func ~ \"open*\" and not path ~ \"/proc/*\""
"expr": "flags == 0 and path ~ \"*.conf\""
"expr": "ret < 0"                                  (after call_real)
```

A string param compared with a relational operator (`s < 3`)
evaluates FALSE at runtime — strings compare with `~`, `==`,
`!=`. A param the call doesn't have is FALSE, never an error.
**A bad expression is a config error**: the whole file is
refused at boot with the reason and character offset, not
silently matching nothing.

## Verified

`test/integration/test_filter_expr.py`: a target opens `.log`
and `.txt` files; only `.log` opens reach the log, and an
invalid expression refuses the config. The parser and
evaluator are property-tested in `test/unit/test_filter_dsl.c`
(seeded random expression trees, truth by construction).

## Notes

The predicate compiles once (a small arena-allocated tree) and
caches on the expression string — per-call cost is the
evaluation, not the parse. The glob matcher is retrace's own
portable one (`retrace_filter_glob_match`) — the same matcher
the caller-glob and future grammar family members share.
