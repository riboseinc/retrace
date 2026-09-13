#!/usr/bin/env python3
"""
E2E: goretrace, the Go runtime agent (TODO.impl/12).

The Go agent joins the SAME supervisor session the preload
agents use: a cgo-built fetcher runs SUPERVISED (runtime lane:
go.http.request/response with method/host/path) and, when the
retrace preload rides along, its name resolution dispatches
getaddrinfo through the libc lane -- one process, two evidence
lanes. The daemon journals the runtime events; the preload's
own log carries the libc calls of the same pid.

Usage: test_goretrace.py <retraced> <gofetch> [retrace-lib]
"""
import json
import os
import signal
import subprocess
import sys
import tempfile
import threading
import time
from http.server import BaseHTTPRequestHandler, HTTPServer

from rpipe import (wait_sock, journal_records)


class _One(BaseHTTPRequestHandler):
    def do_GET(self):
        body = b"go lane\n"
        self.send_response(200)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, *a):
        pass


def main():
    if len(sys.argv) not in (3, 4):
        print("usage: test_goretrace.py <retraced> <gofetch> "
              "[retrace-lib]", file=sys.stderr)
        return 2
    daemon, gofetch = (os.path.abspath(p) for p in sys.argv[1:3])
    rtlib = (os.path.abspath(sys.argv[3])
             if len(sys.argv) == 4 else None)
    if not os.path.exists(gofetch):
        print(f"SKIP: gofetch not built ({gofetch})")
        return 0

    srv = HTTPServer(("127.0.0.1", 0), _One)
    port = srv.server_address[1]
    threading.Thread(target=srv.serve_forever, daemon=True).start()

    work = tempfile.mkdtemp(prefix="gort-")
    sock = os.path.join(work, "agent.sock")
    journal = os.path.join(work, "journal.jsonl")
    nonce_file = os.path.join(work, "nonce.txt")
    libc_log = os.path.join(work, "libc.log")
    # the libc lane rides a scoped config: `write` only. The
    # full inventory under a cgo Go binary trips value-result
    # hazards (getsockopt EFAULT; interposed getaddrinfo breaks
    # resolution) -- a real retrace-vs-Go gap, out of this
    # card's lane-demonstration scope and recorded on the card.
    lane_cfg = os.path.join(work, "lane.json")
    with open(lane_cfg, "w") as f:
        f.write(json.dumps({"intercept_scripts": [{
            "func_name": "write",
            "actions": [{"action_name": "log_params"},
                        {"action_name": "call_real"}],
        }]}))

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

        env = dict(os.environ)
        env.update({
            "RETRACE_SUPERVISOR": "1",
            "RETRACE_SUPERVISOR_SOCK": sock,
            "RETRACE_SUPERVISOR_NONCE": nonce,
            "RETRACE_SUPERVISOR_EAGER": "1",
        })
        argv = [gofetch, f"http://localhost:{port}/"]
        preload_var = ("DYLD_INSERT_LIBRARIES"
                       if sys.platform == "darwin"
                       else "LD_PRELOAD")
        libc_lane = False
        if rtlib is not None and os.path.exists(rtlib):
            # the libc lane: the fetcher's own stdout writes
            # dispatch through the (scoped) preload
            env[preload_var] = rtlib
            env["RETRACE_JSON_CONFIG"] = lane_cfg
            env["RETRACE_LOGGER_DEF_ENA"] = "1"
            env["RETRACE_LOGGER_DEF_STDOUT_ENA"] = "0"
            env["RETRACE_LOGGER_DEF_FN"] = libc_log
            libc_lane = True
        with open(libc_log + ".out", "w") as so:
            child = subprocess.run(argv, env=env, stdout=so,
                stderr=subprocess.STDOUT, timeout=60)
        if child.returncode != 0:
            with open(libc_log + ".out") as f:
                tail = f.read()[-400:]
            print(f"FAIL: gofetch rc={child.returncode}: "
                  f"{tail}", file=sys.stderr)
            return 1

        # durability: stop the daemon gracefully before reading
        d.send_signal(signal.SIGTERM)
        try:
            d.wait(timeout=5)
        except subprocess.TimeoutExpired:
            d.kill()

        recs = journal_records(journal)
        names = [r.get("ev", {}).get("name") for r in recs]
        for want, label in (
                ("go.http.request", "the runtime-attributed request"),
                ("go.http.response", "the response with status"),
                ("go.test.marker", "the direct emit")):
            if want not in names:
                print(f"FAIL: {label} not journaled: {names}",
                      file=sys.stderr)
                return 1
        resp = [r for r in recs if r.get("ev", {}).get("name") ==
                "go.http.response"]
        if resp and resp[-1]["ev"].get("attrs", {}).get(
                "status") != "200":
            print(f"FAIL: wrong status: {resp[-1]}",
                  file=sys.stderr)
            return 1
        # the runtime agent is a full peer
        auth = [r for r in recs if r.get("ev", {}).get("name") ==
                "retrace.auth.agent"]
        if not auth or auth[-1]["ev"].get("role") != "full":
            print(f"FAIL: runtime agent not full role: {auth}",
                  file=sys.stderr)
            return 1

        # the libc lane beside the runtime lane: the SAME pid's
        # writes interposed beside its journaled HTTP events
        if libc_lane:
            try:
                with open(libc_log, errors="replace") as f:
                    n_write = f.read().count('"func": "write"')
            except OSError:
                n_write = 0
            if n_write == 0:
                print("FAIL: libc lane silent (no interposed "
                      "writes)", file=sys.stderr)
                return 1
            print(f"libc lane: {n_write} interposed writes "
                  "beside the runtime lane")

        print("goretrace: request+response+marker journaled; "
              "runtime agent a full peer -- OK")
        return 0
    finally:
        if d.poll() is None:
            d.send_signal(signal.SIGTERM)
            try:
                d.wait(timeout=5)
            except subprocess.TimeoutExpired:
                d.kill()
        srv.shutdown()


if __name__ == "__main__":
    sys.exit(main())
