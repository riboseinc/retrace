#!/usr/bin/env python3
"""
E2E: the shipped policy templates parse and push (TODO.
supervisor/09 P1). Every file under share/policy-templates/ is
pushed onto a fresh daemon and must be accepted -- a template
that stops loading against the daemon's REAL validation is a
red test, so format drift in either direction dies here.

Usage: test_policy_templates.py <retraced> <retrace-ctl> <templates-dir>
"""
import glob
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


def main():
    if len(sys.argv) != 4:
        print("usage: test_policy_templates.py <retraced> "
              "<retrace-ctl> <templates-dir>", file=sys.stderr)
        return 2
    daemon, ctl_bin, tdir = (os.path.abspath(p)
                             for p in sys.argv[1:4])

    templates = sorted(glob.glob(os.path.join(tdir, "*.json")))
    if not templates:
        print("FAIL: no templates found", file=sys.stderr)
        return 1
    print(f"found {len(templates)} template(s)")

    work = tempfile.mkdtemp(prefix="pol-tpl-")
    sock = os.path.join(work, "agent.sock")
    ctl_sock = os.path.join(work, "ctl.sock")
    journal = os.path.join(work, "journal.jsonl")

    dlog = os.path.join(work, "daemon.log")
    log_f = open(dlog, "w")

    def boot_daemon(journal_path):
        return subprocess.Popen(
            [daemon, "--sock", sock, "--journal", journal_path,
             "--ctl", ctl_sock],
            stdout=log_f, stderr=subprocess.STDOUT)

    d = boot_daemon(journal)
    if not wait_sock(ctl_sock):
        d.kill()
        print("FAIL: daemon ctl socket never appeared",
              file=sys.stderr)
        return 1

    def stop_daemon(proc):
        proc.send_signal(signal.SIGTERM)
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            proc.kill()
        # the graceful close unlinks its sockets; a stale file
        # would satisfy wait_sock instantly and the next CLI
        # call would race the bind (rc=2, unreachable)
        end = time.time() + 5
        while time.time() < end and (os.path.exists(sock) or
                                     os.path.exists(ctl_sock)):
            time.sleep(0.1)

    fails = 0
    for tpl in templates:
        # a fresh daemon per template: every template ships at
        # epoch 1, and the ladder only climbs
        if tpl != templates[0]:
            stop_daemon(d)
            d = boot_daemon(journal + ".n")
            if not wait_sock(ctl_sock, 5.0):
                d.kill()
                print(f"FAIL: daemon never relistened for {tpl}",
                      file=sys.stderr)
                return 1
        p = subprocess.run(
            [ctl_bin, "--sock", ctl_sock, "policy-push", tpl],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            timeout=15)
        out = p.stdout.decode()
        name = os.path.basename(tpl)
        if p.returncode != 0 or '"ok":1' not in out:
            print(f"FAIL: {name}: rc={p.returncode} {out!r}",
                  file=sys.stderr)
            # the daemon's own words beat guessing at its state
            d.poll()
            with open(dlog) as f:
                print(f"--- daemon.log (alive={d.poll() is None}):"
                      f"\n{f.read()}", file=sys.stderr)
            fails += 1
        else:
            print(f"push ok: {name}")

    stop_daemon(d)
    if fails:
        return 1
    print(f"PASS: all {len(templates)} templates push clean")
    return 0


if __name__ == "__main__":
    sys.exit(main())
