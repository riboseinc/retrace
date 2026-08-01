# retrace-trace-html

Interactive HTML trace viewer — generates a self-contained web page
from a retrace JSON log.

## Why

People like graphs and UI. The JSON log is powerful for tools but
unreadable for humans. `retrace pp` produces text; `logpp` produces
text with colors; `flamegraph` produces a static SVG bar chart.
`trace-html` produces a **full interactive page** you can share,
filter, and explore.

## Usage

```sh
$ retrace run --config ... --log /tmp/trace.json -- ./your-program
$ python3 tools/trace-html/trace-html.py /tmp/trace.json -o /tmp/view.html
$ open /tmp/view.html
```

## Features

- **Summary cards**: call count, total time, unique functions, avg/call
- **Category breakdown**: I/O, memory, network, sync, exec, env, time
  — each with call count and time share
- **Call-rate sparkline**: visual histogram of call density over time
- **Filterable call table**: search by function name, color-coded by
  category, shows args + return value + duration
- **Self-contained**: no server, no CDN, no JavaScript libraries

## Output example

```
┌──────────┐ ┌────────────┐ ┌───────────┐ ┌──────────┐
│  1247    │ │  48.3ms    │ │    42     │ │   39µs   │
│  Calls   │ │ Total time │ │ Functions │ │ Avg/call │
└──────────┘ └────────────┘ └───────────┘ └──────────┘

By category:
[I/O]     485 calls · 32.1ms (66%)
[MEM]     412 calls ·  8.3ms (17%)
[SYNC]    186 calls ·  4.1ms ( 8%)
...

Call rate: ▁▂▅█▇▃▁▂▄▆▅▂▁▁▂▃▅█▇▄▂▁

[Filter by function name...]
[IO]  open       path=/etc/hosts flags=0        3   12µs
[IO]  read       fd=3 buf=0x7ff... count=4096   4096 8µs
[MEM]  malloc    size=1024                        0x7f8e2a <1µs
...
```

## See also

- [tools/logpp](../logpp/) — text pretty-printer
- [tools/flamegraph](../flamegraph/) — SVG bar chart
- [tools/benchmark](../benchmark/) — overhead measurement
