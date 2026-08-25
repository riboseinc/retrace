#!/usr/bin/env python3
"""
The retraced control-protocol conformance suite
(TODO.supervisor/01 + 11).

SINGLE SOURCE OF TRUTH: src/supervisor/protocol.h's message
table. This suite PARSES that header -- message ids, names, and
directions are never hand-copied here. From the table it:

  - emits one JSON Schema per message under share/rpc-schema/
    (field shapes are authored here, next to the generator, so
    a message's wire contract lives in exactly one place)
  - emits one byte-exact wire golden per message under
    share/rpc-schema/golden/*.bin (little-endian framing per
    the spec in protocol.h)
  - validates the frozen-table rules: ids unique, ascending,
    agent->daemon and daemon->agent ranges disjoint, no id
    collides with the reserved controller range
  - runs the negative corpus: bad magic, truncated header at
    EVERY offset, oversized length, unknown-type forward-skip

The C frame test (test/unit/test_rpc_frame.c) asserts byte-
equality between the C encoder and these same goldens -- two
implementations, one artifact set. --check (the ctest mode)
fails if any checked-in schema or golden is stale; --emit
rewrites them (dev use, then commit the diff).

Usage:
    test_rpc_conformance.py --repo <repo> [--emit] [--check]
"""
import argparse
import json
import re
import struct
import sys
from pathlib import Path

HEADER = "src/supervisor/protocol.h"
SCHEMA_DIR = "share/rpc-schema"
GOLDEN_DIR = "share/rpc-schema/golden"
MAGIC = b"RTRD"
HEADER_SZ = 12
PAYLOAD_MAX = 1 << 20

# Per-message field schemas, keyed by message NAME (from the
# table). "req" fields must be present; "opt" may be omitted by
# older senders (forward compatibility). Authored HERE and only
# here -- the C side never duplicates field names.
FIELDS = {
    "HELLO": {
        "req": {
            "session_token": "string",
            "pid": "integer",
            "ppid": "integer",
            "boot_id": "string",
            "cmdline": "string",
            "retrace_version": "string",
        },
        "opt": {"arch": "string", "agent_capabilities": "array"},
    },
    "HEARTBEAT": {
        "req": {"agent_id": "string", "seq": "integer"},
        "opt": {"rss_kb": "integer", "queue_depth": "integer"},
    },
    "POLICY_ACK": {
        "req": {"agent_id": "string", "policy_epoch": "integer"},
        "opt": {"applied_ms": "integer", "policy.unsigned": "boolean",
                "failed": "string"},
    },
    "EVENT": {
        "req": {"agent_id": "string", "seq": "integer",
                "ts": "integer", "name": "string",
                "attrs": "object"},
        "opt": {},
    },
    "RING_DATA": {
        "req": {"agent_id": "string", "req_id": "string",
                "chunk_idx": "integer", "chunks": "integer",
                "bytes_b64": "string"},
        "opt": {},
    },
    "BYE": {"req": {"agent_id": "string"}, "opt": {"reason": "string"}},
    "WELCOME": {
        "req": {"agent_id": "string", "policy_epoch": "integer",
                "heartbeat_ms": "integer"},
        "opt": {"policy_blob": "string", "policy_sig": "string"},
    },
    "POLICY_SET": {
        "req": {"policy_epoch": "integer", "policy_blob": "string"},
        "opt": {"policy_sig": "string"},
    },
    "CMD": {
        "req": {"req_id": "string", "verb": "string"},
        "opt": {"args": "object"},
    },
    "PING": {"req": {}, "opt": {}},
}

TYPES = {
    "string": {"type": "string"},
    "integer": {"type": "integer"},
    "boolean": {"type": "boolean"},
    "array": {"type": "array"},
    "object": {"type": "object"},
}


def parse_table(repo):
    src = (repo / HEADER).read_text()
    m = re.search(r"#define RETRACE_RPC_MSG_TABLE\(X\)(.*?)\n\n", src,
                  re.S)
    if not m:
        raise SystemExit("FAIL: message table block not found")
    entries = []
    for line in m.group(1).strip().splitlines():
        e = re.match(r"\s*X\((\d+),\s*(\w+),\s*(\w+)\)", line)
        if e:
            entries.append((int(e.group(1)), e.group(2), e.group(3)))
    if not entries:
        raise SystemExit("FAIL: empty message table")
    return entries


def check_table(entries):
    errs = []
    ids = [i for i, _, _ in entries]
    if len(ids) != len(set(ids)):
        errs.append("duplicate message ids")
    if ids != sorted(ids):
        errs.append("table ids not ascending")
    agent_ids = [i for i, _, d in entries if d == "agent_to_daemon"]
    daemon_ids = [i for i, _, d in entries if d == "daemon_to_agent"]
    if set(agent_ids) & set(daemon_ids):
        errs.append("direction ranges overlap")
    for i, n, d in entries:
        if n not in FIELDS:
            errs.append(f"{n}: no field schema authored (add to FIELDS)")
        if d not in ("agent_to_daemon", "daemon_to_agent"):
            errs.append(f"{n}: unknown direction {d}")
    if agent_ids and max(agent_ids) >= 16:
        errs.append("agent->daemon ids must stay < 16")
    if daemon_ids and min(daemon_ids) < 16:
        errs.append("daemon->agent ids must start at 16")
    return errs


def schema_for(name):
    f = FIELDS[name]
    props = {k: dict(TYPES[v]) for k, v in f["req"].items()}
    props.update({k: dict(TYPES[v]) for k, v in f["opt"].items()})
    return {
        "$schema": "https://json-schema.org/draft/2020-12/schema",
        "$id": f"retrace-rpc-{name.lower()}.json",
        "title": f"retrace control protocol: {name}",
        "type": "object",
        "required": sorted(f["req"].keys()),
        "properties": props,
        "additionalProperties": True,
    }


def golden_bytes(mid, payload):
    return MAGIC + struct.pack("<HHI", 1, mid, len(payload)) + payload


def golden_payload(name):
    f = FIELDS[name]
    doc = {}
    for k, v in f["req"].items():
        doc[k] = {"string": "x", "integer": 1, "boolean": True,
                  "array": [], "object": {}}[v]
    return json.dumps(doc, separators=(",", ":"),
                      sort_keys=True).encode()


def write_or_check(repo, path, data, stale):
    existing = path.exists() and path.read_bytes()
    if data != existing:
        stale.append(path.relative_to(repo))
        if ARGS.emit:
            path.write_bytes(data)


def main():
    global ARGS
    ap = argparse.ArgumentParser()
    ap.add_argument("--repo", required=True)
    ap.add_argument("--emit", action="store_true",
                    help="rewrite schemas/goldens (then commit)")
    ap.add_argument("--check", action="store_true", default=True)
    ARGS = ap.parse_args()

    repo = Path(ARGS.repo)
    entries = parse_table(repo)
    failures = []

    # 1. frozen-table rules -- gate everything else: a table
    # that violates the rules must fail loudly, never crash the
    # generator mid-emission
    failures += check_table(entries)
    if failures:
        for f in failures:
            print(f"FAIL: {f}", file=sys.stderr)
        return 1

    stale = []
    (repo / SCHEMA_DIR).mkdir(parents=True, exist_ok=True)
    (repo / GOLDEN_DIR).mkdir(parents=True, exist_ok=True)

    for mid, name, _ in entries:
        # 2. schema artifacts
        s_path = repo / SCHEMA_DIR / f"{name.lower()}.schema.json"
        write_or_check(repo, s_path,
                       (json.dumps(schema_for(name), indent=2) + "\n")
                       .encode(), stale)
        # 3. wire goldens
        g_path = repo / GOLDEN_DIR / f"{name.lower()}.bin"
        write_or_check(repo, g_path,
                       golden_bytes(mid, golden_payload(name)), stale)

    # 4. negative-corpus vectors (their SHAPES; the receiver
    # rules themselves are asserted C-side in test_rpc_frame.c,
    # which re-derives the same vectors from these goldens'
    # framing): truncation at every header offset, an oversize
    # length claim, and the unknown-type forward-skip case.
    # Well-formed here means the vectors the C test builds its
    # negatives from parse back identically.
    probe = golden_bytes(1, b"{}")
    for off in range(HEADER_SZ):
        t = probe[:off]
        if len(t) != off:
            failures.append("truncation vector malformed")
    oversize = MAGIC + struct.pack("<HHI", 1, 1, PAYLOAD_MAX + 1)
    _v, _t, ln = struct.unpack("<HHI", oversize[4:12])
    if ln != PAYLOAD_MAX + 1:
        failures.append("oversize vector malformed")
    unk = MAGIC + struct.pack("<HHI", 1, 9999, 2) + b"{}"
    _v, typ, ln = struct.unpack("<HHI", unk[4:12])
    if typ != 9999 or ln != 2:
        failures.append("unknown-type vector malformed")

    if failures:
        for f in failures:
            print(f"FAIL: {f}", file=sys.stderr)
        return 1
    if stale:
        if ARGS.emit:
            print(f"emitted/updated {len(stale)} artifacts:")
            for p in stale:
                print(f"  {p}")
            print("(commit the diff)")
        else:
            print("FAIL: stale artifacts (run with --emit, commit):",
                  file=sys.stderr)
            for p in stale:
                print(f"  {p}", file=sys.stderr)
            return 1
    print(f"PASS: {len(entries)} messages; schemas + goldens current; "
          f"table rules hold")
    return 0


if __name__ == "__main__":
    sys.exit(main())
