#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause
#
# retrace-trace-html — generate an interactive HTML trace viewer
# from a retrace JSON log.
#
# Output is a self-contained HTML file (no server, no external deps).
# Open in any browser.
#
# Usage:
#   retrace run --config ... --log /tmp/trace.json -- ./your-program
#   python3 tools/trace-html/trace-html.py /tmp/trace.json -o /tmp/view.html
#   open /tmp/view.html

import argparse
import html
import json
import os
import sys
from collections import defaultdict

CATEGORY_KEYWORDS = {
    "open": "I/O", "read": "I/O", "write": "I/O", "close": "I/O",
    "fopen": "I/O", "fclose": "I/O", "fread": "I/O", "fwrite": "I/O",
    "fseek": "I/O", "ftell": "I/O", "lseek": "I/O",
    "opendir": "I/O", "readdir": "I/O", "closedir": "I/O",
    "stat": "I/O", "fstat": "I/O", "lstat": "I/O", "access": "I/O",
    "socket": "NET", "connect": "NET", "bind": "NET", "listen": "NET",
    "accept": "NET", "send": "NET", "recv": "NET",
    "sendto": "NET", "recvfrom": "NET", "sendmsg": "NET", "recvmsg": "NET",
    "getaddrinfo": "NET", "gethostbyname": "NET",
    "malloc": "MEM", "calloc": "MEM", "realloc": "MEM", "free": "MEM",
    "mmap": "MEM", "munmap": "MEM", "brk": "MEM",
    "memset": "MEM", "memcpy": "MEM", "memmove": "MEM", "memcmp": "MEM",
    "strlen": "MEM", "strcmp": "MEM", "strcpy": "MEM", "strncmp": "MEM",
    "pthread_mutex": "SYNC", "pthread_cond": "SYNC", "pthread_rwlock": "SYNC",
    "pthread_create": "SYNC", "pthread_join": "SYNC",
    "system": "EXEC", "exec": "EXEC", "fork": "EXEC", "vfork": "EXEC",
    "exit": "EXEC", "_exit": "EXEC", "abort": "EXEC",
    "getenv": "ENV", "setenv": "ENV", "putenv": "ENV",
    "time": "TIME", "clock": "TIME", "gettimeofday": "TIME",
    "localtime": "TIME", "ctime": "TIME",
}

CATEGORY_COLORS = {
    "I/O": "#4a90d9",
    "NET": "#e8782c",
    "MEM": "#2ecc71",
    "SYNC": "#9b59b6",
    "EXEC": "#e74c3c",
    "ENV": "#f1c40f",
    "TIME": "#1abc9c",
    "OTHER": "#95a5a6",
}

def categorize(func):
    for keyword, cat in CATEGORY_KEYWORDS.items():
        if keyword in func:
            return cat
    return "OTHER"

def parse_entries(path):
    if path == "-":
        return json.loads(sys.stdin.read())
    with open(path) as f:
        return json.loads(f.read())

def extract_calls(entries):
    calls = []
    pending_args = None
    for entry in entries:
        if not isinstance(entry, dict):
            continue
        msg = entry.get("message", {})
        if not isinstance(msg, dict):
            continue
        func = msg.get("func")
        if func and "call_duration_us" in msg:
            call = {
                "func": func,
                "duration_us": float(msg.get("call_duration_us", 0)),
                "ret_val": msg.get("ret_val", ""),
                "category": categorize(func),
                "color": CATEGORY_COLORS[categorize(func)],
                "args": pending_args or {},
            }
            calls.append(call)
            pending_args = None
        elif func is None and "text" not in msg:
            pending_args = {k: v for k, v in msg.items()}
    return calls

def generate_html(calls, title, output_path):
    total_us = sum(c["duration_us"] for c in calls)
    total_count = len(calls)
    cat_counts = defaultdict(int)
    cat_time = defaultdict(float)
    for c in calls:
        cat_counts[c["category"]] += 1
        cat_time[c["category"]] += c["duration_us"]

    # Generate call rows
    rows_html = []
    for c in calls:
        args_str = " ".join(f"{k}={v}" for k, v in c["args"].items()) if c["args"] else ""
        dur_str = f"{c['duration_us']:.0f}µs" if c["duration_us"] >= 1 else "<1µs"
        rows_html.append(
            f'<tr class="call-row" style="border-left:3px solid {c["color"]}">'
            f'<td><span class="badge" style="background:{c["color"]}">{c["category"]}</span></td>'
            f'<td class="func-name">{html.escape(c["func"])}</td>'
            f'<td class="args">{html.escape(args_str)}</td>'
            f'<td>{html.escape(str(c["ret_val"]))}</td>'
            f'<td class="duration">{dur_str}</td>'
            f'</tr>'
        )

    # Generate category summary
    cat_html = []
    for cat in sorted(cat_counts.keys(), key=lambda k: -cat_time[k]):
        pct = 100 * cat_time[cat] / total_us if total_us else 0
        cat_html.append(
            f'<div class="cat-bar">'
            f'<span class="badge" style="background:{CATEGORY_COLORS[cat]}">{cat}</span>'
            f' {cat_counts[cat]} calls · {cat_time[cat]/1000:.1f}ms ({pct:.0f}%)'
            f'</div>'
        )

    # Generate sparkline data (call count per 100ms bucket)
    if calls:
        max_time = max(i for i in range(len(calls)))
        bucket_size = max(1, len(calls) // 50)
        sparkline = []
        for i in range(0, len(calls), bucket_size):
            bucket = calls[i:i+bucket_size]
            sparkline.append(len(bucket))
        spark_max = max(sparkline) if sparkline else 1
        sparkline_bars = "".join(
            f'<div class="spark-bar" style="height:{100*h/spark_max:.0f}%"></div>'
            for h in sparkline
        )
    else:
        sparkline_bars = ""

    doc = f"""<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>{html.escape(title)}</title>
<style>
* {{ box-sizing: border-box; margin: 0; padding: 0; }}
body {{ font-family: -apple-system, system-ui, sans-serif; background: #f8f9fa; color: #2c3e50; padding: 20px; }}
h1 {{ font-size: 1.5rem; margin-bottom: 4px; }}
.subtitle {{ color: #7f8c8d; font-size: 0.9rem; margin-bottom: 16px; }}
.summary {{ display: flex; gap: 12px; flex-wrap: wrap; margin-bottom: 20px; }}
.summary-card {{ background: white; border-radius: 8px; padding: 12px 16px; box-shadow: 0 1px 3px rgba(0,0,0,0.1); }}
.summary-card .label {{ font-size: 0.7rem; text-transform: uppercase; color: #95a5a6; }}
.summary-card .value {{ font-size: 1.5rem; font-weight: 600; }}
.badge {{ display: inline-block; padding: 2px 8px; border-radius: 3px; color: white; font-size: 0.7rem; font-weight: 600; min-width: 30px; text-align: center; }}
.cat-bar {{ font-size: 0.85rem; margin-bottom: 4px; }}
table {{ width: 100%; border-collapse: collapse; background: white; border-radius: 8px; overflow: hidden; box-shadow: 0 1px 3px rgba(0,0,0,0.1); }}
th {{ background: #2c3e50; color: white; padding: 8px 12px; text-align: left; font-size: 0.75rem; text-transform: uppercase; letter-spacing: 0.5px; cursor: pointer; }}
td {{ padding: 6px 12px; border-bottom: 1px solid #ecf0f1; font-size: 0.85rem; }}
tr:hover {{ background: #f8f9fa; }}
.func-name {{ font-family: 'SF Mono', monospace; font-weight: 600; color: #2c3e50; }}
.args {{ font-family: 'SF Mono', monospace; font-size: 0.75rem; color: #7f8c8d; max-width: 300px; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }}
.duration {{ font-family: 'SF Mono', monospace; text-align: right; font-weight: 600; }}
.sections {{ display: flex; gap: 20px; flex-wrap: wrap; margin-bottom: 20px; }}
.section {{ flex: 1; min-width: 300px; }}
.section h2 {{ font-size: 0.9rem; margin-bottom: 8px; color: #7f8c8d; }}
.sparkline {{ display: flex; align-items: flex-end; gap: 1px; height: 40px; background: white; border-radius: 4px; padding: 4px; }}
.spark-bar {{ flex: 1; background: #3498db; border-radius: 1px; min-height: 2px; }}
.search {{ padding: 8px 12px; border: 1px solid #ddd; border-radius: 4px; font-size: 0.85rem; width: 100%; margin-bottom: 12px; }}
</style>
</head>
<body>
<h1>{html.escape(title)}</h1>
<p class="subtitle">{total_count} calls intercepted · {total_us/1000:.1f}ms total libc time · {len(cat_counts)} categories</p>

<div class="summary">
<div class="summary-card"><div class="label">Calls</div><div class="value">{total_count}</div></div>
<div class="summary-card"><div class="label">Total time</div><div class="value">{total_us/1000:.1f}ms</div></div>
<div class="summary-card"><div class="label">Functions</div><div class="value">{len(set(c["func"] for c in calls))}</div></div>
<div class="summary-card"><div class="label">Avg / call</div><div class="value">{total_us/max(total_count,1):.0f}µs</div></div>
</div>

<div class="sections">
<div class="section">
<h2>By category</h2>
{''.join(cat_html)}
</div>
<div class="section">
<h2>Call rate over time</h2>
<div class="sparkline">{sparkline_bars}</div>
</div>
</div>

<input class="search" type="text" placeholder="Filter by function name..." id="search" onkeyup="filterTable()">

<table id="calls">
<thead>
<tr><th>Cat</th><th>Function</th><th>Args</th><th>Return</th><th>Duration</th></tr>
</thead>
<tbody>
{''.join(rows_html)}
</tbody>
</table>

<script>
function filterTable() {{
    const q = document.getElementById('search').value.toLowerCase();
    document.querySelectorAll('.call-row').forEach(row => {{
        const name = row.querySelector('.func-name').textContent.toLowerCase();
        row.style.display = name.includes(q) ? '' : 'none';
    }});
}}
</script>
</body>
</html>"""

    if output_path == "-":
        sys.stdout.write(doc)
    else:
        with open(output_path, "w") as f:
            f.write(doc)
        print(f"wrote {output_path} ({total_count} calls)", file=sys.stderr)

def main(argv):
    p = argparse.ArgumentParser(description="Generate interactive HTML trace viewer")
    p.add_argument("logfile", help="Path to JSON log, or - for stdin")
    p.add_argument("-o", "--output", default="trace.html",
                   help="Output HTML path (default: trace.html)")
    p.add_argument("--title", default="retrace trace",
                   help="HTML title")
    args = p.parse_args(argv)

    entries = parse_entries(args.logfile)
    calls = extract_calls(entries)
    generate_html(calls, args.title, args.output)
    return 0

if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
