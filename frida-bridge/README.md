# retrace-frida

A [Frida](https://frida.re) script that captures libc calls and
emits retrace-compatible JSON. Pairs with retrace-audit, retrace-diff,
retrace-replay, retrace-to-otlp — every retrace tool that reads JSON
logs can consume Frida-captured traces.

## Use cases

- You can't use `LD_PRELOAD` (iOS, static binaries, hardened runtime).
- You want to attach to an already-running process.
- You want to use Frida's existing ecosystem (scripts, tools).

## Usage

```sh
$ pip install frida-tools   # if not already installed
$ frida -l retrace-frida.js -f ./your-binary
$ # or attach to a running process:
$ frida -l retrace-frida.js -n YourProcessName
```

The script writes a retrace JSON array to stdout. Pipe to a file:

```sh
$ frida -l retrace-frida.js -f ./your-binary > /tmp/trace.json
# After the run, manually close the JSON array:
$ echo ']' >> /tmp/trace.json
```

Then feed to any retrace tool:

```sh
$ retrace-audit --policy baseline.json --trace /tmp/trace.json
$ retrace-diff before.json /tmp/trace.json
$ retrace-replay /tmp/trace.json
```

## Configuration

Edit `FUNC_LIST` in `retrace-frida.js` to add or remove intercepted
functions. Each entry is `{ name, argc, stringArgs }`:

```js
const FUNC_LIST = [
    { name: 'open', argc: 2, stringArgs: [0] },        // deref arg 0 as string
    { name: 'malloc', argc: 1 },                       // integer args only
    { name: 'rename', argc: 2, stringArgs: [0, 1] },   // both args as strings
    // ...
];
```

`stringArgs` is a 0-based list of argument indices to dereference as
NUL-terminated C strings (capped at 256 chars). Frida's
`Memory.readUtf8String` handles the read safely -- invalid pointers
fall back to the integer representation so consumers still see the
pointer value.

## Output format

```json
[
{"time":1786269481,"module":"FUNCS","severity":"INFO",
 "message":{"func":"open","args":["/etc/passwd","0"],"ret_val":"3",
            "call_duration_us":12000}}
,
...
]
```

Functions marked with `stringArgs` get their pointer args dereferenced
as strings (truncated to 256 chars). Functions without `stringArgs`
return integer args only (the MVP behavior).

Note: Frida outputs each entry on its own line, comma-separated.
You need to append `]` to close the array (Frida scripts don't get
a clean shutdown hook by default).

## Limitations

This is the MVP + P1 (string-arg dereferencing). Out of scope:

- Variadic functions (`printf`) still capture first arg only as an integer.
  Dereferencing the format string is possible but requires parsing the
  varargs list, which differs per calling convention.
- No filtering -- every call to a hooked function is logged.
- No automatic closing `]` (see Usage above).

## See also

- TODO.complete/28 — Frida backend roadmap
- TODO.complete/29 — eBPF backend (Linux-only, kernel-context)
