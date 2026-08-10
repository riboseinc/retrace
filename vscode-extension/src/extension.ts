/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * retrace viewer VS Code extension (TODO.complete/31 MVP).
 *
 * Activates on first run of either command. The viewer command
 * opens a webview that loads a retrace JSON log and renders it
 * with function-name highlighting + severity coloring. The
 * stats command opens a quick summary (call counts per function).
 */

import * as vscode from "vscode";
import * as fs from "fs";
import * as path from "path";

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

/**
 * Load a retrace JSON log from a path. Returns the entries.
 * Throws on parse error or non-array root.
 */
function loadTrace(filePath: string): RetraceEntry[] {
    const text = fs.readFileSync(filePath, "utf-8");
    const data = JSON.parse(text);
    if (!Array.isArray(data)) {
        throw new Error(`${filePath} is not a JSON array`);
    }
    return data as RetraceEntry[];
}

/**
 * Compute per-function stats: call count, total duration.
 */
function computeStats(entries: RetraceEntry[]): Map<string, { count: number; totalUs: number }> {
    const stats = new Map<string, { count: number; totalUs: number }>();
    for (const e of entries) {
        const func = e.message?.func;
        if (!func) continue;
        const cur = stats.get(func) ?? { count: 0, totalUs: 0 };
        cur.count++;
        if (typeof e.message.call_duration_us === "number") {
            cur.totalUs += e.message.call_duration_us;
        }
        stats.set(func, cur);
    }
    return stats;
}

/**
 * Render the viewer HTML for a set of entries.
 */
function renderViewer(entries: RetraceEntry[], fileName: string): string {
    const rows = entries
        .map((e, i) => {
            const func = e.message?.func ?? "(no func)";
            const sev = e.severity ?? "INFO";
            const args = JSON.stringify(
                Object.fromEntries(
                    Object.entries(e.message).filter(
                        ([k]) =>
                            k !== "func" &&
                            k !== "call_duration_us" &&
                            k !== "ret_val" &&
                            k !== "text"
                    )
                )
            );
            const ret = e.message?.ret_val;
            const us = e.message?.call_duration_us;
            return `<div class="event sev-${sev}">
                <span class="idx">#${i}</span>
                <span class="func">${escapeHtml(func)}</span>
                <span class="sev">${sev}</span>
                <span class="args">${escapeHtml(args)}</span>
                ${ret !== undefined ? `<span class="ret">→ ${escapeHtml(String(ret))}</span>` : ""}
                ${us !== undefined ? `<span class="dur">${us}µs</span>` : ""}
            </div>`;
        })
        .join("\n");

    return `<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>retrace: ${escapeHtml(fileName)}</title>
<style>
body { font-family: var(--vscode-editor-font-family, monospace); margin: 0; padding: 8px; color: var(--vscode-editor-foreground, #eee); background: var(--vscode-editor-background, #1e1e1e); }
.event { display: flex; gap: 12px; padding: 2px 4px; border-bottom: 1px solid var(--vscode-editorWidget-background, #2a2a2a); white-space: nowrap; overflow-x: auto; }
.idx { color: var(--vscode-descriptionForeground, #888); min-width: 60px; }
.func { color: var(--vscode-textLink-foreground, #6ed6ff); font-weight: bold; min-width: 160px; }
.sev { min-width: 60px; }
.sev-WARN { color: #ffcc66; }
.sev-ERROR { color: #ff6666; }
.args { color: var(--vscode-editor-foreground, #ddd); flex-grow: 1; }
.ret { color: var(--vscode-textPreformat-foreground, #b5cea8); }
.dur { color: var(--vscode-descriptionForeground, #888); min-width: 80px; text-align: right; }
h1 { font-size: 14px; margin: 8px 0; padding: 0; color: var(--vscode-foreground, #eee); }
</style>
</head>
<body>
<h1>${escapeHtml(fileName)} (${entries.length} events)</h1>
${rows}
</body>
</html>`;
}

function escapeHtml(s: string): string {
    return s
        .replace(/&/g, "&amp;")
        .replace(/</g, "&lt;")
        .replace(/>/g, "&gt;")
        .replace(/"/g, "&quot;");
}

/**
 * Render a self-contained HTML page that connects to the
 * retrace-ws WebSocket and displays events live.
 *
 * The page opens a WebSocket, parses each incoming JSON entry,
 * and appends it to a scrollable list. Same visual style as
 * renderViewer but with auto-scroll and a connection-status
 * indicator.
 */
function renderLivePage(wsUrl: string): string {
    return `<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>retrace: live</title>
<style>
body { font-family: var(--vscode-editor-font-family, monospace); margin: 0; padding: 8px; color: var(--vscode-editor-foreground, #eee); background: var(--vscode-editor-background, #1e1e1e); }
#status { position: fixed; top: 0; right: 8px; padding: 4px 8px; background: var(--vscode-editorWidget-background, #2a2a2a); border-radius: 0 0 4px 4px; font-size: 12px; }
#events { padding-top: 24px; }
.event { display: flex; gap: 12px; padding: 2px 4px; border-bottom: 1px solid var(--vscode-editorWidget-background, #2a2a2a); white-space: nowrap; }
.idx { color: var(--vscode-descriptionForeground, #888); min-width: 60px; }
.func { color: var(--vscode-textLink-foreground, #6ed6ff); font-weight: bold; min-width: 160px; }
.sev { min-width: 60px; }
.sev-WARN { color: #ffcc66; }
.sev-ERROR { color: #ff6666; }
.args { color: var(--vscode-editor-foreground, #ddd); flex-grow: 1; }
</style>
</head>
<body>
<div id="status">connecting...</div>
<div id="events"></div>
<script>
const ws = new WebSocket(${JSON.stringify(wsUrl)});
const status = document.getElementById('status');
const events = document.getElementById('events');
let count = 0;
ws.onopen = () => status.textContent = 'live (0 events)';
ws.onclose = () => status.textContent = 'disconnected';
ws.onerror = () => status.textContent = 'error';
ws.onmessage = (e) => {
    try {
        const entry = JSON.parse(e.data);
        const div = document.createElement('div');
        div.className = 'event';
        const func = entry.message?.func || entry.message?.text || '?';
        const sev = entry.severity || 'INFO';
        div.innerHTML = '<span class="idx">#' + count + '</span>' +
            '<span class="func sev-' + sev + '">' + escapeHtml(func) + '</span>' +
            '<span class="sev">' + sev + '</span>' +
            '<span class="args">' + escapeHtml(e.data.substring(0, 200)) + '</span>';
        events.appendChild(div);
        count++;
        status.textContent = 'live (' + count + ' events)';
        window.scrollTo(0, document.body.scrollHeight);
    } catch (err) {}
};
function escapeHtml(s) {
    return s.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;');
}
</script>
</body>
</html>`;
}

export function activate(context: vscode.ExtensionContext): void {
    const showViewer = vscode.commands.registerCommand(
        "retrace.showViewer",
        async (uri?: vscode.Uri) => {
            let filePath: string | undefined;
            if (uri?.fsPath) {
                filePath = uri.fsPath;
            } else {
                const picked = await vscode.window.showOpenDialog({
                    canSelectMany: false,
                    filters: { "JSON logs": ["json"] },
                    title: "Select a retrace log",
                });
                filePath = picked?.[0]?.fsPath;
            }
            if (!filePath) return;

            let entries: RetraceEntry[];
            try {
                entries = loadTrace(filePath);
            } catch (err) {
                void vscode.window.showErrorMessage(
                    `retrace: cannot load ${filePath}: ${(err as Error).message}`
                );
                return;
            }

            const panel = vscode.window.createWebviewPanel(
                "retraceViewer",
                `retrace: ${path.basename(filePath)}`,
                vscode.ViewColumn.Active,
                { enableScripts: false }
            );
            panel.webview.html = renderViewer(entries, path.basename(filePath));
        }
    );

    const showStats = vscode.commands.registerCommand(
        "retrace.showStats",
        async (uri?: vscode.Uri) => {
            let filePath: string | undefined;
            if (uri?.fsPath) {
                filePath = uri.fsPath;
            } else {
                const picked = await vscode.window.showOpenDialog({
                    canSelectMany: false,
                    filters: { "JSON logs": ["json"] },
                    title: "Select a retrace log",
                });
                filePath = picked?.[0]?.fsPath;
            }
            if (!filePath) return;

            let entries: RetraceEntry[];
            try {
                entries = loadTrace(filePath);
            } catch (err) {
                void vscode.window.showErrorMessage(
                    `retrace: cannot load ${filePath}: ${(err as Error).message}`
                );
                return;
            }

            const stats = computeStats(entries);
            const items = Array.from(stats.entries())
                .sort((a, b) => b[1].count - a[1].count)
                .map(([func, s]) => ({
                    label: func,
                    description: `${s.count} calls`,
                    detail: `${(s.totalUs / 1000).toFixed(1)}ms total`,
                }));

            await vscode.window.showQuickPick(items, {
                placeHolder: `${stats.size} functions, ${entries.length} events`,
            });
        }
    );

    const connectLive = vscode.commands.registerCommand(
        "retrace.connectLive",
        async () => {
            const url = await vscode.window.showInputBox({
                prompt: "WebSocket URL of retrace-ws",
                value: "ws://localhost:8765/ws",
                placeHolder: "ws://hostname:port/ws",
            });
            if (!url) return;

            const panel = vscode.window.createWebviewPanel(
                "retraceLive",
                `retrace: live @ ${url}`,
                vscode.ViewColumn.Active,
                { enableScripts: true }
            );

            panel.webview.html = renderLivePage(url);
        }
    );

    context.subscriptions.push(showViewer, showStats, connectLive);
}

export function deactivate(): void {
    /* nothing to clean up */
}
