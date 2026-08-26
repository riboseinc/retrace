#!/usr/bin/env python3
"""
E2E: the in-process agent against the real daemon
(TODO.supervisor/03).

Phase 1 (delivery): a target under a denying sandbox config,
RETRACE_SUPERVISOR=1 pointing at a live retraced; the denial
must arrive in the daemon's journal as a complete EVENT payload
(agent_id/seq/ts/name/attrs) attributed to the minted agent.

Phase 2 (fail-open liveness): same run with the daemon KILLED
mid-flight -- the target must still exit 0, its behavior
unchanged (bounded queue, backoff, never die).

Usage: test_supervisor_agent.py <retraced> <libretrace> <target>
"""
import json
import os
import signal
import subprocess
import sys
import tempfile
import time

RETRY_WAIT = 3.0


TEST_NONCE = "0123456789abcdef0123456789abcdef"


def wait_sock(path, deadline=5.0):
    end = time.time() + deadline
    while time.time() < end:
        if os.path.exists(path):
            return True
        time.sleep(0.1)
    return False


def run_target(lib, target, sock, denied, env_extra=None):
    cfg = tempfile.NamedTemporaryFile(
        prefix="sup-agent-cfg-", suffix=".json", mode="w",
        delete=False)
    json.dump({"intercept_scripts": [{
        "func_name": "open",
        "actions": [
            {"action_name": "sandbox",
             "action_params": {"deny_paths": [denied]}},
            {"action_name": "call_real"}]}]}, cfg)
    cfg.close()
    env = dict(os.environ)
    env.update({
        "RETRACE_JSON_CONFIG": cfg.name,
        "RETRACE_SUPERVISOR": "1",
        "RETRACE_SUPERVISOR_SOCK": sock,
        "RETRACE_SUPERVISOR_NONCE": TEST_NONCE,
        "RETRACE_LOGGER_DEF_ENA": "1",
        "RETRACE_LOGGER_DEF_STDOUT_ENA": "0",
    })
    if env_extra:
        env.update(env_extra)
    if sys.platform == "darwin":
        env["DYLD_INSERT_LIBRARIES"] = lib
    else:
        env["LD_PRELOAD"] = lib
    return subprocess.run(
        [target, denied, "/etc/protocols"], env=env,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=30)


def main():
    if len(sys.argv) != 4:
        print("usage: test_supervisor_agent.py <retraced> "
              "<lib> <target>", file=sys.stderr)
        return 2
    daemon, lib, target = (os.path.abspath(p) for p in sys.argv[1:4])

    work = tempfile.mkdtemp(prefix="sup-agent-")
    sock = os.path.join(work, "agent.sock")
    journal = os.path.join(work, "journal.jsonl")
    dlog_path = os.path.join(work, "daemon.log")
    denied_f = tempfile.NamedTemporaryFile(
        prefix="sup-denied-", delete=False)
    denied_f.close()
    denied = denied_f.name

    def dump_daemon_log():
        try:
            with open(dlog_path) as f:
                tail = f.read().splitlines()[-12:]
            for ln in tail:
                print(f"  daemon: {ln[:200]}", file=sys.stderr)
        except OSError:
            print("  daemon: (no output captured)", file=sys.stderr)

    # ---- phase 1: delivery ------------------------------------
    dlog = open(dlog_path, "w")
    d = subprocess.Popen(
        [daemon, "--sock", sock, "--journal", journal,
         "--nonce", TEST_NONCE],
        stdout=dlog, stderr=subprocess.STDOUT)
    if not wait_sock(sock):
        d.kill()
        print("FAIL: daemon never listened", file=sys.stderr)
        dump_daemon_log()
        return 1

    try:
        proc = run_target(lib, target, sock, denied)
    except subprocess.TimeoutExpired:
        d.kill()
        print("FAIL: target timed out", file=sys.stderr)
        dump_daemon_log()
        return 1
    if proc.returncode != 0:
        d.kill()
        print(f"FAIL: target rc={proc.returncode} "
              f"err={proc.stderr[:200]!r}", file=sys.stderr)
        dump_daemon_log()
        return 1

    # give the agent's deinit flush a moment, then stop the daemon
    time.sleep(0.5)
    d.send_signal(signal.SIGTERM)
    try:
        d.wait(timeout=5)
    except subprocess.TimeoutExpired:
        d.kill()
    dlog.close()

    try:
        with open(journal) as f:
            lines = [ln for ln in f.read().splitlines() if ln.strip()]
    except FileNotFoundError:
        print("FAIL: daemon wrote no journal at all "
              "(no EVENT delivered)", file=sys.stderr)
        dump_daemon_log()
        print(f"  target stdout: {proc.stdout[:200]!r}", file=sys.stderr)
        return 1
    hits = []
    for ln in lines:
        try:
            rec = json.loads(ln)
        except json.JSONDecodeError:
            continue
        ev = rec.get("ev", {})
        if ev.get("name") == "retrace.jail.denied":
            hits.append((rec.get("agent"), ev))
    if not hits:
        print(f"FAIL: no jail.denied in journal ({len(lines)} lines)",
              file=sys.stderr)
        for ln in lines[:4]:
            print(f"  {ln[:160]}", file=sys.stderr)
        dump_daemon_log()
        return 1
    agent, ev = hits[0]
    attrs = ev.get("attrs", {})
    if not agent or "boot." not in agent:
        print(f"FAIL: bad agent attribution: {agent!r}", file=sys.stderr)
        return 1
    if attrs.get("retrace.jail.path") != denied:
        print(f"FAIL: attrs wrong: {attrs}", file=sys.stderr)
        return 1
    for k in ("agent_id", "seq", "ts", "name", "attrs"):
        if k not in ev:
            print(f"FAIL: EVENT schema missing {k}: {ev}",
                  file=sys.stderr)
            return 1
    print(f"phase 1 ok: {len(hits)} denial(s), agent={agent}, "
          f"seq={ev.get('seq')}")

    # ---- phase 2: fail-open liveness ---------------------------
    # daemon absent: the target must behave identically (exit 0)
    sock2 = os.path.join(work, "agent2.sock")
    try:
        proc2 = run_target(lib, target, sock2, denied)
    except subprocess.TimeoutExpired:
        print("FAIL: target timed out with daemon ABSENT",
              file=sys.stderr)
        return 1
    if proc2.returncode != 0:
        print(f"FAIL: daemon-absent run rc={proc2.returncode} "
              f"err={proc2.stderr[:200]!r}", file=sys.stderr)
        return 1
    if b"denied-open rc=-1" not in proc2.stdout:
        print(f"FAIL: denial did not happen daemon-absent: "
              f"{proc2.stdout[:120]!r}", file=sys.stderr)
        return 1
    print("phase 2 ok: daemon absent -> target unchanged (exit 0, "
          "denial enforced locally)")

    print("PASS: agent (delivery + schema + attribution + "
          "fail-open liveness)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
