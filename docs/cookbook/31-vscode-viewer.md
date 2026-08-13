# 31 — Browse a trace in VS Code

## Problem

You're already debugging in VS Code. You have a retrace log open in
a text editor and you're scrolling through JSON, jumping between
`func:` fields, manually correlating arg values. The terminal-based
tools (`retrace-replay`, `jq`) work but break your editor flow.

The `retrace-viewer` VS Code extension renders a retrace JSON log
in a webview pane inside VS Code. Function names highlighted,
severity color-coded, args prettified, and a stats panel that
groups calls per function. Since v2.3.0 it also connects directly
to a live `retrace-ws` stream — see recipe 28.

## Config

The extension is a viewer; it doesn't need a retrace config. Any
retrace JSON log works. Use the default wildcard config to capture
the trace:

`vscode-default.json`:

```json
{
  "intercept_scripts": [
    {
      "func_name": "*",
      "actions": [
        { "action_name": "log_params" },
        { "action_name": "call_real" }
      ]
    }
  ]
}
```

## Invocation

### 1. Capture the trace

```sh
$ retrace run --config cookbook/vscode-default.json \
    --log /tmp/trace.json -- ./your-binary args...
```

### 2. Install the extension

```sh
$ cd vscode-extension
$ npm install && npm run compile
$ npx vsce package
$ code --install-extension retrace-viewer-0.1.0.vsix
```

### 3. Open the trace in VS Code

Reload VS Code. Open the Command Palette (`Ctrl/Cmd+Shift+P`),
run **retrace: Open Log Viewer**, pick `/tmp/trace.json`. A webview
opens with every event rendered.

## Expected output

Two commands ship with the extension:

| Command | Effect |
|---------|--------|
| `retrace: Open Log Viewer` | Pick a `.json` trace, render events in a webview. |
| `retrace: Show Stats` | Quick-pick menu of functions, sorted by call count, with total time per function. |
| `retrace: Connect to Live Stream` | Connect to a `retrace-ws` URL (recipe 28); events stream into the viewer without re-loading. |

The webview color-codes severity (red for ERROR, orange for WARN,
gray for INFO), emphasizes function names, and indents args for
readability.

## Variations

### Right-click a .json file in the Explorer

Skip the Command Palette: right-click any `.json` file → **retrace:
Open Log Viewer** opens it directly.

### Live-stream a long-running server

Start `retrace-ws` (recipe 28), then in VS Code:

1. Command Palette → **retrace: Connect to Live Stream**.
2. Enter the WebSocket URL (default `ws://localhost:8765/stream`).
3. Events stream into the same webview; no reload needed.

This is the team-shared dashboard pattern — one developer runs the
trace, anyone on the team can connect and watch.

### Pair with audit findings

Run `retrace-audit` to get entry indices of violations (recipe
24), then jump to those events in the viewer:

```sh
$ retrace-audit --policy share/policies/baseline.json \
    --trace /tmp/trace.json --format default \
    | jq '.findings[].entry_index'
```

The viewer doesn't (yet) have a "go to index" box; for now, use
`retrace-replay` (recipe 27) for index-based navigation.

## Caveats

- The MVP loads every event at once. For traces >10 MB, the
  webview is sluggish. Filter at capture time with
  `RETRACE_LOGGER_ALLOWED_FUNCS`.
- The extension is not on the Marketplace yet; package from source
  as shown above.
- No inline config builder. For point-and-click config generation,
  use the website's Recipe Builder.

## See also

- Recipe 27 — Time-travel replay (terminal-based; same data, more
  keyboard-driven workflow).
- Recipe 28 — Live streaming via WebSocket (the stream this
  extension consumes).
- Recipe 32 — Grafana dashboard (the team dashboard for ops
  audiences).
