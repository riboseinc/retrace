# retrace wasm playground

A minimal demo of retrace's intercept-log-trace pattern, running entirely in the browser via WebAssembly. Part of TODO.complete/36 P2.

## What it does

Visitors pick a number N, click "Run", and see the libc-call trace for computing N! (factorial). The trace shows every `malloc`, `memset`, `strlen`, `strcpy`, `free` call — exactly what retrace would intercept in a real target.

This is NOT the full retrace engine (which requires LD_PRELOAD semantics that don't exist in wasm). It's a concept demo that proves the trace pattern works in the browser.

## Build

```sh
clang --target=wasm32 -O2 -nostdlib \
  -Wl,--no-entry -Wl,--export=run \
  -Wl,--export=get_trace \
  -o playground.wasm playground.c
```

Then open `index.html` in a browser (serve via `python3 -m http.server` to avoid CORS issues).

## Files

- `playground.c` -- self-contained C module compiled to wasm. Exports `run(n)` and `get_trace()`.
- `index.html` -- browser page that loads the wasm and displays the trace.

## Architecture

The wasm module is completely standalone (no libc, no retrace engine). It implements:
- A tiny heap (16 KB) with a bump allocator
- Instrumented `malloc`, `memset`, `strlen`, `strcpy`, `free` that record each call
- A `run(n)` function that computes factorial(N) using those functions
- A `get_trace()` function that returns the trace as a C string

The browser page loads the wasm, calls `run(n)`, reads the trace string from wasm memory, and displays it with syntax highlighting (function names in cyan).

## Future work

The TODO.complete/36 P2 spec envisioned running the actual retrace engine in the browser. That requires either:
1. A wasm build of the retrace engine + a wasm-compatible trampoline mechanism (wasm imports instead of LD_PRELOAD)
2. Or a wasm re-linking tool that wraps a target module's imports

Both are multi-week efforts. This demo proves the concept is viable without the engineering investment.
