# retrace data source for Grafana

A minimal Grafana data source plugin that loads a retrace JSON log (served via HTTP) and exposes its events as a Grafana time series. Dashboards can then chart call counts, durations, group by function name, severity, etc.

Part of TODO.complete/32 (Grafana plugin). MVP scope.

## Install

This plugin lives in-tree alongside retrace. To build and install:

```sh
cd grafana-plugin
npm install
npm run build
# Sign the plugin (replace <your-key> with your Grafana plugin signing key):
npx @grafana/toolkit plugin:sign --rootUrls http://localhost:3000
# Copy to Grafana plugins dir:
cp -r dist /var/lib/grafana/plugins/riboseinc-retrace-datasource
sudo systemctl restart grafana-server
```

## Configure

1. Serve your retrace JSON log via HTTP:
   ```sh
   $ retrace run --log /tmp/trace.json -- ./your-binary
   $ python3 -m http.server 8000 --directory /tmp &
   ```
2. In Grafana: Configuration -> Data Sources -> Add data source -> choose "retrace".
3. Set "Path" to `http://localhost:8000/trace.json`.
4. Click "Save & Test". The plugin loads the trace once on first query.

## Use in dashboards

Add a Time series panel. Set the data source to "retrace". In the query editor, set "Function" to filter (e.g. `malloc`, or empty for all functions). The frame returned has fields:

- `time` -- event timestamp (ms)
- `duration_us` -- call duration (microseconds)
- `severity` -- DEBUG / INFO / WARN / ERROR
- `ret_val` -- the call's return value

Visualize as Time series (duration over time), Bar gauge (call count by function), or State timeline (severity timeline).

## Limitations (MVP scope)

- Loads the file once on first query, caches in memory. No live reload (TODO.complete/32 P1).
- No backend plugin (no @grafana/data backend). Pure frontend.
- Single-frame response only (no time-split aggregation).
- Config UI is minimal (just a path field). No file picker.
- Local file paths not supported (the plugin fetches via HTTP to avoid FS sandboxing).

## See also

- TODO.complete/32 -- Grafana plugin roadmap
- TODO.complete/31 -- VS Code extension (similar viewer, in-IDE)
- TODO.complete/23 -- OTLP bridge (the right way to integrate with observability stacks long-term)
