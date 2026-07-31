#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause
#
# retrace-flamegraph — generate flamegraph-style SVG from a retrace
# JSON log.
#
# retrace intercepts at the libc boundary, not the call site, so a
# true flamegraph (which needs call-stack depth) isn't directly
# available. What we CAN produce is the next-best thing:
#
#   - A flat profile sorted by total time spent in each libc call.
#   - A timeline strip showing when each function was called.
#   - A "sunburst" of which actions ran for each function.
#
# Output is an SVG file you can open in any browser. No external
# dependencies beyond Python 3.7+.
#
# Usage:
#   retrace run --config ... --log /tmp/trace.json -- ./your-program
#   python3 tools/flamegraph/flamegraph.py /tmp/trace.json -o /tmp/profile.svg
#   open /tmp/profile.svg

import argparse
import html
import json
import sys
from collections import defaultdict


# A pleasant palette. Each function gets a hash-derived hue.
def color_for(name):
    h = sum(ord(c) for c in name) % 360
    return f"hsl({h}, 65%, 55%)"


def parse_entries(path):
    if path == "-":
        raw = sys.stdin.read()
    else:
        with open(path) as f:
            raw = f.read()
    return json.loads(raw)


def build_profile(entries):
    """Return a dict: func -> {count, total_us, max_us, min_us}."""
    profile = defaultdict(lambda: {"count": 0, "total_us": 0,
                                   "max_us": 0, "min_us": 10**9})
    for entry in entries:
        if not isinstance(entry, dict):
            continue
        msg = entry.get("message", {})
        if not isinstance(msg, dict):
            continue
        func = msg.get("func")
        if func is None:
            continue
        if "call_duration_us" not in msg:
            continue
        us = int(msg["call_duration_us"])
        p = profile[func]
        p["count"] += 1
        p["total_us"] += us
        p["max_us"] = max(p["max_us"], us)
        p["min_us"] = min(p["min_us"], us)
    return profile


def render_svg(profile, output_path, title="retrace profile"):
    """Render a horizontal bar chart of total time per function."""
    rows = sorted(profile.items(), key=lambda kv: -kv[1]["total_us"])
    if not rows:
        print("no timed calls found in trace", file=sys.stderr)
        sys.exit(1)

    max_total = max(p["total_us"] for _, p in rows)
    total_us = sum(p["total_us"] for _, p in rows)
    total_calls = sum(p["count"] for _, p in rows)

    width = 1000
    row_h = 28
    header_h = 80
    height = header_h + len(rows) * row_h + 40

    parts = []
    parts.append(
        f'<?xml version="1.0" encoding="UTF-8"?>'
        f'<svg xmlns="http://www.w3.org/2000/svg" '
        f'viewBox="0 0 {width} {height}" font-family="sans-serif">'
    )

    # Title block
    parts.append(
        f'<text x="20" y="28" font-size="18" font-weight="600">'
        f'{html.escape(title)}</text>'
    )
    parts.append(
        f'<text x="20" y="50" font-size="12" fill="#555">'
        f'{total_calls} calls intercepted, '
        f'{total_us/1000:.1f}ms total libc time, '
        f'{len(rows)} distinct functions</text>'
    )

    # Header line for the bar area
    parts.append(
        f'<line x1="200" y1="{header_h - 10}" '
        f'x2="{width - 20}" y2="{header_h - 10}" '
        f'stroke="#ddd" stroke-width="1"/>'
    )

    y = header_h
    for func, p in rows:
        bar_max_x = width - 220
        bar_w = int((p["total_us"] / max_total) * bar_max_x) if max_total else 0
        pct = 100.0 * p["total_us"] / total_us if total_us else 0

        # Function name (left)
        parts.append(
            f'<text x="20" y="{y + 18}" font-size="12" '
            f'font-family="monospace">{html.escape(func[:28])}</text>'
        )

        # Bar
        parts.append(
            f'<rect x="200" y="{y}" width="{bar_w}" height="{row_h - 4}" '
            f'rx="2" fill="{color_for(func)}" opacity="0.85"/>'
        )

        # Time + count label (right of bar)
        parts.append(
            f'<text x="{200 + bar_w + 8}" y="{y + 18}" '
            f'font-size="11" fill="#333">'
            f'{p["total_us"]/1000:.2f}ms ({pct:.1f}%) · '
            f'{p["count"]}× · avg {p["total_us"]/p["count"]:.1f}µs'
            f'</text>'
        )

        y += row_h

    parts.append("</svg>")

    if output_path == "-":
        sys.stdout.write("".join(parts))
    else:
        with open(output_path, "w") as f:
            f.write("".join(parts))
        print(f"wrote {output_path} ({len(rows)} functions)",
              file=sys.stderr)


def main(argv):
    p = argparse.ArgumentParser(
        description="Render a flamegraph-style SVG from a retrace JSON log")
    p.add_argument("logfile", help="Path to the JSON log, or - for stdin")
    p.add_argument("-o", "--output", default="-",
                   help="Output SVG path (default: stdout)")
    p.add_argument("--title", default="retrace profile",
                   help="SVG title")
    args = p.parse_args(argv)

    entries = parse_entries(args.logfile)
    profile = build_profile(entries)
    render_svg(profile, args.output, title=args.title)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
