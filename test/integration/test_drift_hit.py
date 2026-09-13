#!/usr/bin/env python3
"""
E2E: live hit-level drift grading (TODO.impl/18). The daemon
names WHAT escaped, beside the counts.

Two agents speak to one daemon: a LIBC claimant (source=libc
events -- the in-process agent's evidence) and a KERNEL
observer (source=kernel observations -- the ebpf lane's sight).
The kernel observation of a path the libc lane claimed is NOT
a hit; the observation of a path it never claimed IS
retrace.drift.hit, named, within the event stream itself.

The offline arm: the same corpus fed to retrace-correlate (the
rigorous offline grader) agrees -- the same escape, the same
name.

Usage: test_drift_hit.py <retraced> [correlate]
"""
import json
import os
import signal
import socket
import subprocess
import sys
import tempfile
import time

from rpipe import (frame, recv_frame, wait_sock, journal_records)

HELLO, EVENT, BYE = 1, 4, 6


class Agent:
    """one lane's connection: HELLO (full role, the nonce) and
    a framed EVENT emitter"""

    def __init__(self, sock_path, nonce, boot_id, pid):
        self.s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.s.connect(sock_path)
        self.s.sendall(frame(HELLO, json.dumps({
            "session_token": "", "nonce": nonce, "pid": pid,
            "ppid": 0, "boot_id": boot_id, "cmdline": boot_id,
            "retrace_version": "drift-e2e"})))
        rf = self.s.makefile("rb")
        recv_frame(rf)  # WELCOME
        rf.close()

    def emit(self, name, source, **attrs):
        self.s.sendall(frame(EVENT, json.dumps({
            "agent_id": "", "seq": int(time.time()), "ts":
            int(time.time()), "name": name, "attrs": attrs,
            "source": source})))

    def close(self):
        try:
            self.s.sendall(frame(BYE,
                                 json.dumps({"agent_id": ""})))
            self.s.close()
        except OSError:
            pass


def main():
    if len(sys.argv) not in (2, 3):
        print("usage: test_drift_hit.py <retraced> [correlate]",
              file=sys.stderr)
        return 2
    daemon = os.path.abspath(sys.argv[1])
    correlate = (os.path.abspath(sys.argv[2])
                 if len(sys.argv) == 3 else None)

    work = tempfile.mkdtemp(prefix="drift-")
    sock = os.path.join(work, "agent.sock")
    journal = os.path.join(work, "journal.jsonl")
    nonce_file = os.path.join(work, "nonce.txt")

    d = subprocess.Popen(
        [daemon, "--sock", sock, "--journal", journal,
         "--nonce-file", nonce_file],
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        if not wait_sock(sock):
            print("FAIL: daemon never listened", file=sys.stderr)
            return 1
        with open(nonce_file) as f:
            nonce = f.read().strip()

        libc = Agent(sock, nonce, "libc-lane", pid=4242)
        kern = Agent(sock, nonce, "kernel-lane", pid=4242)

        # the libc lane claims its own sight of the allowed read
        libc.emit("retrace.jail.claim", "libc",
                  func="openat", path="/cfg/ok.bin")
        # the kernel lane sees BOTH: the claimed read and a
        # sub-libc escape the libc layer never saw
        kern.emit("kernel.syscall.observe", "kernel",
                  syscall="openat", path="/cfg/ok.bin")
        kern.emit("kernel.syscall.observe", "kernel",
                  syscall="openat", path="/cfg/secret/keys.pem")
        # the escaping loop repeats: the dedup window folds it
        kern.emit("kernel.syscall.observe", "kernel",
                  syscall="openat", path="/cfg/secret/keys.pem")
        time.sleep(0.5)
        libc.close()
        kern.close()

        d.send_signal(signal.SIGTERM)
        try:
            d.wait(timeout=5)
        except subprocess.TimeoutExpired:
            d.kill()

        recs = journal_records(journal)
        hits = [r.get("ev", {}) for r in recs
                if r.get("ev", {}).get("name") ==
                "retrace.drift.hit"]
        shadow_hits = [h for h in hits
                       if h.get("path") == "/cfg/secret/keys.pem"]
        ok_hits = [h for h in hits
                   if h.get("path") == "/cfg/ok.bin"]
        if len(shadow_hits) != 1:
            print(f"FAIL: want exactly 1 named keys.pem hit, "
                  f"got {hits}", file=sys.stderr)
            return 1
        if ok_hits:
            print("FAIL: the claimed path hit anyway: "
                  f"{ok_hits}", file=sys.stderr)
            return 1
        if shadow_hits[0].get("op") != "openat":
            print(f"FAIL: hit misnamed: {shadow_hits[0]}",
                  file=sys.stderr)
            return 1

        # the offline arm: the same corpus through the rigorous
        # grader agrees on the escape
        if correlate is not None and os.path.exists(correlate):
            inside = os.path.join(work, "inside.json")
            outside = os.path.join(work, "outside.json")
            with open(inside, "w") as f:
                f.write(json.dumps({"time": 1755580000,
                    "pid": 4242, "tid": 4242, "module": "tfs",
                    "severity": "INFO", "message": {
                        "op": "openat",
                        "path": "/cfg/ok.bin"}}) + "\n")
            with open(outside, "w") as f:
                for pth in ("/cfg/ok.bin", "/cfg/secret/keys.pem"):
                    f.write(json.dumps({"time": 1755580001,
                        "pid": 4242, "tid": 4242,
                        "module": "retrace", "severity": "INFO",
                        "message": {"func": "openat",
                            "params": {"path": pth}}}) + "\n")
            r = subprocess.run(
                [correlate, "--inside", inside,
                 "--outside", outside, "--prefix", "/cfg"],
                capture_output=True, text=True, timeout=30)
            out = r.stdout + r.stderr
            if r.returncode != 1 or "keys.pem" not in out:
                print(f"FAIL: correlate disagrees "
                      f"(rc={r.returncode}): {out[:300]}",
                      file=sys.stderr)
                return 1
            print("correlate: names the same escape -- OK")

        print("drift-hit: escape named live (keys.pem), "
              "claimed path quiet, dedup held -- OK")
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
