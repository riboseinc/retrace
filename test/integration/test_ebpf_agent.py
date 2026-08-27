#!/usr/bin/env python3
"""
E2E: the kernel-observation agent (TODO.beyond-libc/03).

The eBPF agent joins the supervisor as a SPECTATOR (observer
doctrine: evidence ALWAYS, policy NEVER -- it HELLOs without
the nonce deliberately) and its kernel-source events land in
the same hash-chained journal as the libc lane. The synthetic
source proves the pipe on CI (no BPF privileges needed); the
--loader source does the same against the real bridge on a
BPF-capable host.

Usage: test_ebpf_agent.py <retraced> <agent-script>
"""
import json
import os
import signal
import subprocess
import sys
import tempfile
import time


def wait_sock(path, deadline=5.0):
    end = time.time() + deadline
    while time.time() < end:
        if os.path.exists(path):
            return True
        time.sleep(0.1)
    return False


def journal_records(path):
    recs = []
    with open(path) as f:
        for ln in f.read().splitlines():
            try:
                recs.append(json.loads(ln))
            except json.JSONDecodeError:
                pass
    return recs


def main():
    if len(sys.argv) != 3:
        print("usage: test_ebpf_agent.py <retraced> <agent>",
              file=sys.stderr)
        return 2
    daemon, agent = (os.path.abspath(p) for p in sys.argv[1:3])

    work = tempfile.mkdtemp(prefix="ebpf-ag-")
    sock = os.path.join(work, "agent.sock")
    journal = os.path.join(work, "journal.jsonl")

    d = subprocess.Popen(
        [daemon, "--sock", sock, "--journal", journal],
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        if not wait_sock(sock):
            print("FAIL: daemon never listened", file=sys.stderr)
            return 1

        r = subprocess.run(
            [sys.executable, agent, "--sock", sock,
             "--synthetic", "--samples", "3"],
            capture_output=True, timeout=30)
        if r.returncode != 0:
            print(f"FAIL: agent rc={r.returncode}: "
                  f"{r.stderr.decode()[:300]}", file=sys.stderr)
            return 1

        # durability: graceful stop flushes the routine tail
        d.send_signal(signal.SIGTERM)
        try:
            d.wait(timeout=5)
        except subprocess.TimeoutExpired:
            d.kill()

        recs = journal_records(journal)
        names = [r.get("ev", {}).get("name") for r in recs]
        obs = [r for r in recs
               if r.get("ev", {}).get("name") ==
               "kernel.syscall.observe"]
        if len(obs) != 3:
            print(f"FAIL: expected 3 kernel observations, "
                  f"got {len(obs)}: {names}", file=sys.stderr)
            return 1
        auth = [r for r in recs
                if r.get("ev", {}).get("name") == "retrace.auth.agent"]
        if not auth or auth[-1]["ev"].get("role") != "spectator":
            print(f"FAIL: kernel agent not seated as spectator: "
                  f"{auth}", file=sys.stderr)
            return 1
        if any(r.get("ev", {}).get("name") == "retrace.policy.pushed"
               for r in recs):
            print("FAIL: policy reached an observer",
                  file=sys.stderr)
            return 1

        print("ebpf-agent: 3 kernel observations journaled; "
              "spectator seat; zero policy reach -- OK")
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
