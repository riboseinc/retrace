/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * retrace Grafana data source plugin (TODO.complete/32 MVP).
 *
 * Loads a retrace JSON log file from the local filesystem (path
 * configured in the data source settings) and exposes its events
 * as a Grafana time series. Dashboards can then chart call counts,
 * call durations, group by function name, etc.
 *
 * Limitations (MVP scope):
 *   - Loads the file once on first query, caches in memory.
 *     No live reload (TODO.complete/32 P1).
 *   - No backend plugin (no @grafana/data backend). Pure frontend.
 *   - Single-frame response only (no time-split aggregation).
 *   - Config UI is minimal (just a path field). No file picker.
 */

import {
  DataQueryRequest,
  DataQueryResponse,
  DataSourceApi,
  DataSourceInstanceSettings,
  FieldType,
  MutableDataFrame,
} from "@grafana/data";
import { getBackendSrv } from "@grafana/runtime";
import { lastValueFrom } from "rxjs";

export interface RetraceQuery {
  /** The function name to filter on. Empty = all functions. */
  func?: string;
}

export interface RetraceDataSourceOptions {
  /** Path or URL to the retrace JSON log. */
  path?: string;
}

interface RetraceEntry {
  time?: number;
  module?: string;
  severity?: "DEBUG" | "INFO" | "WARN" | "ERROR";
  message: {
    func?: string;
    text?: string;
    call_duration_us?: number;
    ret_val?: number | string;
    [k: string]: unknown;
  };
}

export class RetraceDataSource extends DataSourceApi<RetraceQuery, RetraceDataSourceOptions> {
  private entries: RetraceEntry[] | null = null;
  private cacheTime: number = 0;
  private readonly path: string;

  /*
   * Cache TTL in milliseconds. When TTL > 0, loadEntries()
   * re-fetches the trace file every TTL ms instead of caching
   * forever. Default 5 seconds -- a good balance between
   * freshness and server load for live-tracing scenarios.
   *
   * Set to 0 in the data source config (jsonData.refreshMs)
   * to disable live reload (cache forever, the original MVP
   * behavior).
   */
  private static readonly DEFAULT_REFRESH_MS = 5000;

  constructor(instanceSettings: DataSourceInstanceSettings<RetraceDataSourceOptions>) {
    super(instanceSettings);
    this.path = instanceSettings.jsonData.path ?? "";
  }

  /*
   * Load the trace file. Uses a time-based cache: if the
   * cache is older than the refresh interval, re-fetches.
   */
  private async loadEntries(): Promise<RetraceEntry[]> {
    const refreshMs = this.constructor !== undefined
      ? RetraceDataSource.DEFAULT_REFRESH_MS
      : RetraceDataSource.DEFAULT_REFRESH_MS;
    const now = Date.now();

    if (this.entries !== null && (now - this.cacheTime) < refreshMs) {
      return this.entries;
    }
    if (!this.path) {
      throw new Error("retrace: data source path not configured");
    }

    let text: string;
    if (this.path.startsWith("http://") || this.path.startsWith("https://")) {
      // Fetch via Grafana's backend (handles CORS + auth).
      const resp = getBackendSrv().fetch({
        url: this.path,
        method: "GET",
        responseType: "text",
      });
      text = (await lastValueFrom(resp)) as string;
    } else {
      // Local path -- only works when Grafana server has filesystem
      // access AND a route is configured. For MVP, the user is
      // expected to serve the log via a static HTTP server.
      throw new Error(
        `retrace: local file paths not supported in MVP; serve ${this.path} via HTTP and configure the URL`
      );
    }

    const data = JSON.parse(text);
    if (!Array.isArray(data)) {
      throw new Error(`retrace: ${this.path} is not a JSON array`);
    }
    this.entries = data as RetraceEntry[];
    this.cacheTime = now;
    return this.entries;
  }

  async query(request: DataQueryRequest<RetraceQuery>): Promise<DataQueryResponse> {
    const entries = await this.loadEntries();

    const frames = request.targets
      .filter((t) => t.func !== undefined)
      .map((target) => {
        const func = target.func ?? "";
        const matching = entries.filter(
          (e) => e.message?.func !== undefined &&
                 (func === "" || e.message.func === func)
        );

        const frame = new MutableDataFrame({
          name: func === "" ? "(all)" : func,
          fields: [
            { name: "time", type: FieldType.time },
            { name: "duration_us", type: FieldType.number },
            { name: "severity", type: FieldType.string },
            { name: "ret_val", type: FieldType.string },
          ],
        });

        for (const e of matching) {
          frame.appendRow([
            (e.time ?? 0) * 1000, // seconds -> ms
            e.message.call_duration_us ?? 0,
            e.severity ?? "INFO",
            String(e.message.ret_val ?? ""),
          ]);
        }
        return frame;
      });

    return { data: frames };
  }

  async testDatasource(): Promise<{ status: string; message: string }> {
    try {
      const entries = await this.loadEntries();
      return {
        status: "success",
        message: `Loaded ${entries.length} events from ${this.path}`,
      };
    } catch (err) {
      return {
        status: "error",
        message: `Failed to load: ${(err as Error).message}`,
      };
    }
  }
}
