#!/usr/bin/env python3
"""
The reference stub agent (TODO.supervisor/11 P0, the gate for
TODO.beyond-libc/03+04): a THIRD-PARTY implementation of the
control protocol, written against protocol.h and the checked-in
schemas ONLY -- no retrace headers, no C linkage. If the real
daemon accepts this stub, the protocol is implementable outside
the tree; that is the conformance bar for eBPF, ETW, and
runtime agents to come.

The state machine it walks (the L3 semantics, from plan 11):

  HELLO(nonce)  -> WELCOME(role=full) [+ POLICY_SET if loaded]
  HEARTBEAT     -> (registry liveness)
  EVENT         -> (journaled; asserted from the journal file)
  PING          -> PING reply
  BYE           -> (registry exit; asserted via the journal)

Usage: ref_agent.py <retraced> <repo-root>
"""
import json
import os
import signal
import socket
import struct
import subprocess
import sys
import tempfile
import time

MAGIC = b"RTRD"


def parse_table(repo):
    """message table from protocol.h -- the SSOT parse"""
    entries = {}
    with open(os.path.join(repo, "src/supervisor/protocol.h")) as f:
        text = f.read()
    for line in text.splitlines():
        line = line.strip()
        if not line.startswith("X("):
            continue
        parts = [p.strip() for p in line[2:].rstrip(" \\").split(",")]
        if len(parts) != 3:
            continue
        try:
            mid = int(parts[0])
        except ValueError:
            continue
        entries[parts[1]] = (mid, parts[2])
    return entries


def frame(mid, payload):
    b = payload.encode() if isinstance(payload, str) else payload
    return MAGIC + struct.pack("<HHI", 1, mid, len(b)) + b


def read_frame(sock, timeout=5.0):
    try:
        sock.settimeout(timeout)
    except (AttributeError, OSError):
        pass  # raw pipe file: blocking is fine (the daemon replies)
    hdr = b""
    while len(hdr) < 12:
        chunk = sock.recv(12 - len(hdr))
        if not chunk:
            raise EOFError("daemon closed")
        hdr += chunk
    magic, ver, mid, ln = struct.unpack("<4sHHI", hdr)
    if magic != MAGIC:
        raise ValueError("bad magic from daemon")
    body = b""
    while len(body) < ln:
        chunk = sock.recv(ln - len(body))
        if not chunk:
            raise EOFError("short body")
        body += chunk
    return mid, body.decode(errors="replace")


def journal_records(path):
    recs = []
    try:
        with open(path) as f:
            for ln in f.read().splitlines():
                try:
                    recs.append(json.loads(ln))
                except json.JSONDecodeError:
                    pass
    except OSError:
        pass
    return recs


def main():
    if len(sys.argv) != 3:
        print("usage: ref_agent.py <retraced> <repo-root>",
              file=sys.stderr)
        return 2
    daemon, repo = (os.path.abspath(p) for p in sys.argv[1:3])
    table = parse_table(repo)
    for need in ("HELLO", "HEARTBEAT", "EVENT", "PING", "BYE",
                 "WELCOME"):
        if need not in table:
            print(f"FAIL: message {need} missing from the table",
                  file=sys.stderr)
            return 1

    work = tempfile.mkdtemp(prefix="conf-ref-")
    sock_path = os.path.join(work, "agent.sock")
    journal = os.path.join(work, "journal.jsonl")
    nonce_file = os.path.join(work, "nonce.txt")

    d = subprocess.Popen(
        [daemon, "--sock", sock_path, "--journal", journal,
         "--nonce-file", nonce_file],
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        end = time.time() + 5
        def sock_up():
            if sock_path.startswith(r"\\\\.\\pipe"):
                try:
                    open(sock_path, "r+b", buffering=0).close()
                    return True
                except OSError:
                    return False
            return os.path.exists(sock_path)

        while time.time() < end and not sock_up():
            time.sleep(0.1)
        def _up():
            if sock_path.startswith(r"\\.\\pipe\\"):
                try:
                    open(sock_path, "r+b", buffering=0).close()
                    return True
                except OSError:
                    return False
            return os.path.exists(sock_path)

        if not _up():
            print("FAIL: daemon never listened", file=sys.stderr)
            return 1
        with open(nonce_file) as f:
            nonce = f.read().strip()

        if sock_path.startswith(r"\\.\pipe\\"):
            s = open(sock_path, "r+b", buffering=0)
        else:
            s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            s.connect(sock_path)

        # HELLO -> WELCOME(role full)
        s.sendall(frame(table["HELLO"][0], json.dumps({
            "session_token": "", "nonce": nonce,
            "pid": os.getpid(), "ppid": os.getppid(),
            "boot_id": "ref", "cmdline": "ref_agent",
            "retrace_version": "conformance-1"})))
        mid, body = read_frame(s)
        if mid != table["WELCOME"][0]:
            print(f"FAIL: expected WELCOME, got {mid}",
                  file=sys.stderr)
            return 1
        welcome = json.loads(body)
        if welcome.get("role") != "full":
            print(f"FAIL: nonce HELLO not full role: {welcome}",
                  file=sys.stderr)
            return 1
        if welcome.get("session_token", "") == "":
            print("FAIL: no session minted in WELCOME",
                  file=sys.stderr)
            return 1

        # HEARTBEAT
        s.sendall(frame(table["HEARTBEAT"][0],
                        json.dumps({"agent_id": welcome["agent_id"],
                                    "seq": 1})))

        # EVENT (journaled)
        s.sendall(frame(table["EVENT"][0], json.dumps({
            "agent_id": welcome["agent_id"], "seq": 2,
            "ts": int(time.time()),
            "name": "ref.conformance.event",
            "attrs": {"kind": "stub"}}, sort_keys=True)))

        # PING round-trip
        s.sendall(frame(table["PING"][0], "{}"))
        mid, _ = read_frame(s)
        if mid != table["PING"][0]:
            print(f"FAIL: PING not answered (got {mid})",
                  file=sys.stderr)
            return 1

        # BYE
        s.sendall(frame(table["BYE"][0],
                        json.dumps({"agent_id": welcome["agent_id"]})))
        s.close()

        # routine telemetry is buffered (the durability
        # contract): stop the daemon gracefully so the journal
        # flushes + closes, THEN read it
        d.send_signal(signal.SIGTERM)
        try:
            d.wait(timeout=5)
        except subprocess.TimeoutExpired:
            d.kill()
        recs = journal_records(journal)
        names = [r.get("ev", {}).get("name") for r in recs]
        if "ref.conformance.event" not in names:
            print(f"FAIL: stub event not journaled: {names}",
                  file=sys.stderr)
            return 1
        if "retrace.auth.agent" not in names:
            print("FAIL: the auth record missing", file=sys.stderr)
            return 1

        print("ref-agent conformance: HELLO/WELCOME(full), "
              "heartbeat, event journaled, PING, BYE -- OK")
        return 0
    finally:
        if d.poll() is None:
            d.send_signal(signal.SIGTERM)
            try:
                d.wait(timeout=5)
            except subprocess.TimeoutExpired:
                d.kill()


if __name__ == "__main__":
    sys.exit(main())
