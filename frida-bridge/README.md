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
functions. Each entry is `{ name, argc }`:

```js
const FUNC_LIST = [
    { name: 'open', argc: 2 },
    { name: 'malloc', argc: 1 },
    // ...
];
```

## Output format

```json
[
{"time":1786269481,"module":"FUNCS","severity":"INFO",
 "message":{"func":"open","args":["12345678","0"],"ret_val":"3",
            "call_duration_us":12}}
,
...
]
```

Note: Frida outputs each entry on its own line, comma-separated.
You need to append `]` to close the array (Frida scripts don't get
a clean shutdown hook by default).

## Limitations

This is the MVP. Out of scope for the first PR:

- Args are integers only (Frida's `args[0].toString()`). Dereferencing
  pointers (e.g. for `char *path` in `open`) is possible but the API
  differs per platform.
- Variadic functions (`printf`) get only the first N args.
- No filtering — every call to a hooked function is logged.
- No automatic closing `]` (see Usage above).

## See also

- TODO.complete/28 — Frida backend roadmap
- TODO.complete/29 — eBPF backend (Linux-only, kernel-context)
