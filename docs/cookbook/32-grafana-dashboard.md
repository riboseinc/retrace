# 32 — Visualize a trace in Grafana

## Problem

You have ops folks who live in Grafana. Telling them to install a
terminal tool or VS Code extension to look at a trace is a
non-starter. They want call-count and duration panels on the same
dashboard as their other infrastructure metrics.

The `retrace` Grafana data source plugin loads a retrace JSON log
(over HTTP) and exposes its events as a Grafana frame. Build any
panel: time series of `malloc` duration, bar gauge of calls per
function, state timeline of severity. With cache TTL (v2.3.0),
the panel re-fetches the trace file periodically — pair it with a
live-trace writer for a refresh-on-a-schedule dashboard.

## Config

The plugin is a viewer; it consumes any retrace JSON log. Use the
default wildcard config to capture the trace:

`grafana-default.json`:

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

### 1. Capture the trace and serve it over HTTP

```sh
$ retrace run --config cookbook/grafana-default.json \
    --log /var/log/retrace/trace.json -- ./your-server &
# Re-run periodically (cron, systemd timer) for a fresh-trace pipeline
$ python3 -m http.server 8000 --directory /var/log/retrace &
```

### 2. Build and install the plugin

```sh
$ cd grafana-plugin
$ npm install && npm run build
$ npx @grafana/toolkit plugin:sign --rootUrls http://localhost:3000
$ sudo cp -r dist /var/lib/grafana/plugins/riboseinc-retrace-datasource
$ sudo systemctl restart grafana-server
```

### 3. Add the data source in Grafana

1. Grafana → **Configuration → Data Sources → Add data source** →
   pick **retrace**.
2. Set **Path** to `http://localhost:8000/trace.json`.
3. Set **Cache TTL (ms)** to your refresh interval (e.g. `5000`
   for 5-second live reload, `0` to cache forever).
4. **Save & Test**.

### 4. Build dashboards

Add panels using the retrace data source. Per-panel query editor
filters by function name.

## Expected output

Each query returns a frame with four fields:

| Field | Type | Source |
|-------|------|--------|
| `time` | time | event timestamp (ms) |
| `duration_us` | number | `call_duration_us` |
| `severity` | string | INFO / WARN / ERROR |
| `ret_val` | number | the call's return value |

Panel recipes that work well:

| Panel type | What to plot |
|------------|--------------|
| Time series | `duration_us` over time, group by `func` — find slow calls. |
| Bar gauge | Count of events per `func` — see hot functions. |
| State timeline | `severity` over time — spot error bursts. |
| Table | Latest 50 events grouped by `func`, sorted by `duration_us` desc. |

## Variations

### Live reload via cache TTL

The v2.3.0 release added `cacheTime` + TTL to the data source.
With TTL > 0, every panel refresh re-fetches the trace file, so a
rolling `retrace run` produces a self-updating dashboard:

```sh
# Stream retrace output to the served file forever
$ while true; do
    retrace run --config cookbook/grafana-default.json \
        --log /var/log/retrace/trace.json -- ./your-server --one-request
done
```

Set **Cache TTL** to `1000` (1 second) and the panel reflects new
requests within a second.

### Filter at the source for high-volume traces

Grafana isn't a streaming UI; loading 100K events per refresh will
slow it down. Restrict the trace up front:

```sh
$ RETRACE_LOGGER_ALLOWED_FUNCS=malloc,free,read,write \
    retrace run --config cookbook/grafana-default.json \
    --log /var/log/retrace/trace.json -- ./your-server
```

### Combine with the OTLP bridge for long-term storage

Grafana's data source is great for live review. For long-term
storage and querying (30-day retention, alerting), use the OTLP
bridge (recipe 23) to ship spans to Tempo / Jaeger / Honeycomb and
point Grafana at the OTLP-compatible backend instead.

## Caveats

- **HTTP only.** The plugin fetches via HTTP; it doesn't read the
  local filesystem directly (Grafana's sandbox forbids it). Run a
  one-line `python3 -m http.server` next to the trace file.
- **Single-frame response.** The MVP doesn't do server-side
  aggregation; the entire file is parsed on each refresh. For
  traces >50 MB, restrict the function allowlist.
- **Pure frontend plugin.** No backend binary; everything runs in
  the browser. This limits panel types — for backend-powered
  features (streaming, alerting) use the OTLP bridge instead.
- **Plugin signing required.** Unsigned plugins only work with
  `allow_loading_unsigned_plugins = riboseinc-retrace-datasource`
  in `grafana.ini`. Sign for production deployments.

## See also

- Recipe 23 — Bridge to OpenTelemetry (the long-term-storage
  alternative).
- Recipe 28 — Live streaming via WebSocket (the real-time pipeline
  this dashboard consumes).
- Recipe 31 — VS Code extension (the developer-facing equivalent of
  this dashboard).
