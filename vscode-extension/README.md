# retrace viewer for VS Code

A minimal VS Code extension that loads a retrace JSON log and renders it in a webview with function-name highlighting and severity coloring. Also exposes a stats command that shows per-function call counts.

Part of TODO.complete/31 (VS Code extension). This is the MVP scaffold.

## Features

- **retrace: Open Log Viewer** -- opens a webview showing every event in the log, with color-coded severity and function-name emphasis.
- **retrace: Show Stats** -- quick-pick menu of all functions, sorted by call count, with total time per function.

Both commands are available in the Command Palette and via right-click on a `.json` file in the Explorer.

## Install

This extension lives in-tree alongside retrace. To package it for local testing:

```sh
cd vscode-extension
npm install
npm run compile
npx vsce package
# Install the resulting retrace-viewer-0.1.0.vsix into VS Code:
code --install-extension retrace-viewer-0.1.0.vsix
```

## Use

1. Run retrace on a binary to produce a JSON log:
   ```sh
   retrace run --log /tmp/trace.json -- ./your-binary
   ```
2. Open the Command Palette in VS Code, run `retrace: Open Log Viewer`, select `/tmp/trace.json`.

## Limitations (MVP scope)

- No live streaming (load is one-shot).
- No filtering in the viewer; loads every event at once. For very large logs (>10 MB), this is slow.
- No config builder (TODO.complete/31 P1).

## See also

- TODO.complete/31 -- VS Code extension roadmap
- TODO.complete/32 -- Grafana plugin (server-side dashboard)
