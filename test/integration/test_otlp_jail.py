#!/usr/bin/env python3
"""
Integration test for the live jail-denial OTLP event
(TODO.trace-profile/32, otlp-c Wave C).

Runs a target that opens a DENIED path (sandbox action) plus an
allowed one, under RETRACE_OTLP_ENDPOINT, and asserts:
  - at least one POST to /v1/logs (the retrace.jail.denied LOG)
  - the at-exit stats line reports logs_sent >= 1
  - /v1/traces spans still streamed (the denial didn't break
    the span pipeline)

Usage:
    test_otlp_jail.py <retrace_dylib> <target> <fixture_server>
"""
import json
import os
import selectors
import socket
import subprocess
import sys
import tempfile
import time


def find_free_port():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


def wait_ready(server):
    sel = selectors.DefaultSelector()
    sel.register(server.stdout, selectors.EVENT_READ)
    buf = b""
    deadline = time.time() + 20.0
    while time.time() < deadline:
        if server.poll() is not None:
            return None
        if sel.select(0.2):
            chunk = os.read(server.stdout.fileno(), 4096)
            if not chunk:
                break
            buf += chunk
            if b"\n" in buf:
                return buf.split(b"\n", 1)[0].decode(errors="replace")
    return None


def main():
    if len(sys.argv) != 4:
        print("usage: test_otlp_jail.py <dylib> <target> <fixture>",
              file=sys.stderr)
        return 2

    dylib = os.path.abspath(sys.argv[1])
    target = os.path.abspath(sys.argv[2])
    fixture = os.path.abspath(sys.argv[3])
    for p, what in ((dylib, "library"), (target, "target"),
                    (fixture, "fixture")):
        if not os.path.exists(p):
            print(f"FAIL: {what} not found: {p}", file=sys.stderr)
            return 2

    port = find_free_port()
    denied = tempfile.NamedTemporaryFile(
        prefix="jail-denied-", delete=False)
    denied.close()
    # The config denies this path; the target also opens a real
    # system file that the policy allows through.
    cfg = tempfile.NamedTemporaryFile(
        prefix="jail-cfg-", suffix=".json", mode="w", delete=False)
    json.dump({
        "intercept_scripts": [{
            "func_name": "open",
            "actions": [
                {"action_name": "sandbox",
                 "action_params": {"deny_paths": [denied.name]}},
                {"action_name": "call_real"},
            ],
        }],
    }, cfg)
    cfg.close()

    log = tempfile.NamedTemporaryFile(
        prefix="jail-req-", suffix=".jsonl", delete=False)
    log.close()

    child_python = "/usr/bin/python3"
    if not os.path.exists(child_python):
        child_python = sys.executable
    server = subprocess.Popen(
        [child_python, fixture, str(port), log.name],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    try:
        ready = wait_ready(server)
        if ready is None or not ready.startswith("fixture_otlp_server:"):
            server.terminate()
            out, err = server.communicate(timeout=5)
            print(f"FAIL: fixture server not ready "
                  f"(rc={server.poll()}) stdout={out!r} "
                  f"stderr={err!r}", file=sys.stderr)
            return 1

        env = os.environ.copy()
        env["RETRACE_JSON_CONFIG"] = cfg.name
        env["RETRACE_OTLP_ENDPOINT"] = f"http://127.0.0.1:{port}"
        env["RETRACE_LOGGER_DEF_ENA"] = "1"
        env["RETRACE_LOGGER_DEF_STDOUT_ENA"] = "0"
        env["RETRACE_LOGGER_FMT"] = "jsonl"
        trlog = tempfile.NamedTemporaryFile(
            prefix="jail-trace-", suffix=".jsonl", delete=False)
        trlog.close()
        env["RETRACE_LOGGER_DEF_FN"] = trlog.name + ".log"
        if sys.platform == "darwin":
            env["DYLD_INSERT_LIBRARIES"] = dylib
        else:
            env["LD_PRELOAD"] = dylib

        try:
            proc = subprocess.run(
                [target, denied.name, "/etc/protocols"], env=env,
                stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                timeout=30)
        except subprocess.TimeoutExpired:
            print("FAIL: target timed out", file=sys.stderr)
            return 1

        time.sleep(1.5)
        with open(log.name) as f:
            reqs = [json.loads(ln) for ln in f if ln.strip()]
        stderr = (proc.stderr or b"").decode(errors="replace")
        stdout = (proc.stdout or b"").decode(errors="replace")

        def trace_grep(pattern):
            try:
                with open(trlog.name + ".log") as f:
                    return sum(1 for ln in f if pattern in ln)
            except OSError:
                return -1

        logs_posts = [r for r in reqs if r["path"] == "/v1/logs"]
        trace_posts = [r for r in reqs if r["path"] == "/v1/traces"]

        if not logs_posts or not trace_posts:
            print(f"FAIL: logs_posts={len(logs_posts)} "
                  f"trace_posts={len(trace_posts)}; exit="
                  f"{proc.returncode}\n"
                  f"  target stdout: {stdout!r}\n"
                  f"  stderr: {stderr[:300]!r}\n"
                  f"  trace: otlp_live={trace_grep('otlp_live')} "
                  f"DENIED={trace_grep('DENIED')} "
                  f"lines={trace_grep(chr(123))}",
                  file=sys.stderr)
            return 1
        if "logs_sent=0" in stderr or "logs_emitted=0" in stderr:
            print(f"FAIL: stats line says no logs: {stderr[:300]!r}",
                  file=sys.stderr)
            return 1

        print(f"PASS: {len(logs_posts)} /v1/logs + "
              f"{len(trace_posts)} /v1/traces POST(s)")
        return 0
    finally:
        server.terminate()
        try:
            server.wait(timeout=5)
        except subprocess.TimeoutExpired:
            server.kill()
        for tmp in (denied.name, cfg.name, log.name):
            try:
                os.unlink(tmp)
            except OSError:
                pass


if __name__ == "__main__":
    sys.exit(main())
