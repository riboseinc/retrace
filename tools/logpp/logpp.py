#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause
#
# retrace-log pp — pretty-printer for the JSON log that retrace emits.
#
# The raw log is a single JSON array with one object per log entry.
# For long program runs that's thousands of lines of JSON — useful
# for tools but unreadable for humans. This script reformats it into
# one line per intercepted call, color-coded by severity, with the
# call's argument names and values inlined.
#
# Usage:
#   retrace run --config ... --log /tmp/trace.json -- ./your-program
#   python3 tools/logpp/logpp.py /tmp/trace.json
#
# Or pipe directly:
#   retrace run --config ... -- ./your-program 2>&1 | logpp.py -
#
# Exit codes: 0 = success, 1 = bad input, 2 = internal error.

import json
import sys
import os
from collections import Counter


COLORS = {
    "INFO": "\033[37m",     # white
    "WARN": "\033[33m",     # yellow
    "ERROR": "\033[31m",    # red
    "DBG": "\033[90m",      # gray
}
RESET = "\033[0m"
BOLD = "\033[1m"


def use_color(stream):
    return hasattr(stream, "isatty") and stream.isatty() and \
        os.environ.get("NO_COLOR") is None


def fmt_msg(msg):
    if not isinstance(msg, dict):
        return str(msg)
    parts = []
    for k, v in msg.items():
        if k == "text":
            parts.append(str(v))
        elif isinstance(v, list):
            parts.append(f"{k}={v!r}")
        else:
            parts.append(f"{k}={v}")
    return " ".join(parts)


def format_entry(entry, color):
    sev = entry.get("severity", "INFO")
    mod = entry.get("module", "?")
    msg = entry.get("message", {})
    text = fmt_msg(msg)

    if color:
        c = COLORS.get(sev, COLORS["INFO"])
        return f"{c}[{sev:5s}] {mod:6s}{RESET} {text}"
    return f"[{sev:5s}] {mod:6s} {text}"


def summarize(entries):
    counts = Counter()
    for e in entries:
        msg = e.get("message", {})
        if isinstance(msg, dict):
            func = msg.get("func")
            if func:
                counts[func] += 1
    return counts


def main(argv):
    if len(argv) < 2:
        print(f"usage: {argv[0]} <logfile.json|->", file=sys.stderr)
        return 1

    path = argv[1]
    if path == "-":
        raw = sys.stdin.read()
    else:
        try:
            with open(path) as f:
                raw = f.read()
        except OSError as e:
            print(f"error reading {path}: {e}", file=sys.stderr)
            return 1

    try:
        data = json.loads(raw)
    except json.JSONDecodeError as e:
        print(f"error parsing JSON: {e}", file=sys.stderr)
        return 1

    if not isinstance(data, list):
        print(f"expected a JSON array, got {type(data).__name__}",
              file=sys.stderr)
        return 1

    color = use_color(sys.stdout)

    for entry in data:
        if not isinstance(entry, dict):
            continue
        print(format_entry(entry, color))

    if data:
        print("", file=sys.stdout)
        print(f"{BOLD}Summary{RESET}" if color else "Summary")
        counts = summarize(data)
        total = sum(counts.values())
        print(f"  {total} calls intercepted")
        for func, n in counts.most_common(10):
            pct = 100.0 * n / total if total else 0
            print(f"  {func:30s} {n:8d}  ({pct:5.1f}%)")

    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
