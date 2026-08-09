/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * retrace Grafana plugin entry point. Exports the DataSource
 * so Grafana can register it on plugin load.
 */

import { DataSourcePlugin } from "@grafana/data";
import { RetraceDataSource, RetraceQuery, RetraceDataSourceOptions } from "./datasource";

export const plugin = new DataSourcePlugin<RetraceDataSource, RetraceQuery, RetraceDataSourceOptions>(
  RetraceDataSource
).setConfigEditor(undefined);
