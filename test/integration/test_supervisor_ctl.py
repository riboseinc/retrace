#!/usr/bin/env python3
"""
E2E: retrace-ctl, the fleet CLI (TODO.supervisor/07 P0).

A daemon with --ctl supervises one long-running target while the
CLI drives the whole lifecycle: status/ps discovery, live policy
push (tighten), freeze/thaw (the IR hold, with kernel truth: the
creatable-file probe stops and RESUMES), and kill.

Usage: test_supervisor_ctl.py <retraced> <retrace-ctl> <lib> <target>
"""
import json
import os
import signal
import subprocess
import sys
import tempfile
import time

DENIED = "/etc/hosts"
ALLOWED = "/etc/protocols"


def wait_sock(path, deadline=5.0):
    end = time.time() + deadline
    while time.time() < end:
        if os.path.exists(path):
            return True
        time.sleep(0.1)
    return False


def ctl(ctl_bin, sock, *args):
    p = subprocess.run([ctl_bin, "--sock", sock, *args],
                       stdout=subprocess.PIPE,
                       stderr=subprocess.PIPE, timeout=15)
    return p.returncode, p.stdout.decode()


def stop_daemon(d, sock):
    if d is None:
        return
    d.send_signal(signal.SIGTERM)
    try:
        d.wait(timeout=5)
    except subprocess.TimeoutExpired:
        d.kill()
    deadline = time.time() + 2
    while os.path.exists(sock) and time.time() < deadline:
        time.sleep(0.1)


def files_count(dirpath):
    try:
        return len(os.listdir(dirpath))
    except OSError:
        return 0


def wait_files(dirpath, predicate, deadline):
    end = time.time() + deadline
    last = -1
    while time.time() < end:
        n = files_count(dirpath)
        if n != last:
            last = n
        if predicate(n):
            return n
        time.sleep(0.25)
    return None


def main():
    if len(sys.argv) != 5:
        print("usage: test_supervisor_ctl.py <retraced> "
              "<retrace-ctl> <lib> <target>", file=sys.stderr)
        return 2
    daemon, ctl_bin, lib, target = (
        os.path.abspath(p) for p in sys.argv[1:5])

    work = tempfile.mkdtemp(prefix="sup-ctl-")
    sock = os.path.join(work, "agent.sock")
    ctl_sock = os.path.join(work, "ctl.sock")
    journal = os.path.join(work, "journal.jsonl")
    dlog = os.path.join(work, "daemon.log")
    creatable_dir = tempfile.mkdtemp(prefix="sup-ctl-files-")

    boot_cfg = os.path.join(work, "boot.json")
    with open(boot_cfg, "w") as f:
        json.dump({"intercept_scripts": [
            {"func_name": "open",
             "actions": [{"action_name": "call_real"}]}]}, f)

    pol1 = os.path.join(work, "policy1.json")
    with open(pol1, "w") as f:
        json.dump({"policy": {"epoch": 1},
                   "intercept_scripts": [{
                       "func_name": "open",
                       "actions": [
                           {"action_name": "sandbox",
                            "action_params": {
                                "deny_paths": [DENIED]}},
                           {"action_name": "call_real"}]}]}, f)

    # epoch 2: deny the creatable prefix -- live push has its own
    # kernel truth (the probe file stops appearing)
    pol2 = os.path.join(work, "policy2.json")
    with open(pol2, "w") as f:
        json.dump({"policy": {"epoch": 2},
                   "intercept_scripts": [{
                       "func_name": "open",
                       "actions": [
                           {"action_name": "sandbox",
                            "action_params": {
                                "deny_paths": [
                                    DENIED,
                                    creatable_dir + "/"]}},
                           {"action_name": "call_real"}]}]}, f)

    # epoch 5 (after freeze/thaw): back to deny-only -- work
    # resumes, proving a SECOND push after thaw
    pol3 = os.path.join(work, "policy3.json")
    with open(pol3, "w") as f:
        json.dump({"policy": {"epoch": 5},
                   "intercept_scripts": [{
                       "func_name": "open",
                       "actions": [
                           {"action_name": "sandbox",
                            "action_params": {
                                "deny_paths": [DENIED]}},
                           {"action_name": "call_real"}]}]}, f)

    log_f = open(dlog, "w")
    d = subprocess.Popen(
        [daemon, "--sock", sock, "--journal", journal,
         "--ctl", ctl_sock, "--policy", pol1],
        stdout=log_f, stderr=subprocess.STDOUT)
    log_f.close()
    if not wait_sock(ctl_sock):
        d.kill()
        print("FAIL: daemon ctl socket never appeared", file=sys.stderr)
        return 1

    env = dict(os.environ)
    env.update({
        "RETRACE_JSON_CONFIG": boot_cfg,
        "RETRACE_SUPERVISOR": "1",
        "RETRACE_SUPERVISOR_EAGER": "1",
        "RETRACE_SUPERVISOR_SOCK": sock,
        "RETRACE_LOGGER_DEF_ENA": "0",
    })
    if sys.platform == "darwin":
        env["DYLD_INSERT_LIBRARIES"] = lib
    else:
        env["LD_PRELOAD"] = lib
    proc = subprocess.Popen(
        [target, "40", DENIED, ALLOWED, creatable_dir],
        env=env, stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL)

    # ---- status/ps: the fleet becomes visible -----------------
    rc, out = ctl(ctl_bin, ctl_sock, "status")
    if rc != 0 or '"agents":0' in out:
        # agents arrive asynchronously; poll briefly
        end = time.time() + 8
        while time.time() < end:
            rc, out = ctl(ctl_bin, ctl_sock, "status")
            if rc == 0 and '"agents":0' not in out:
                break
            time.sleep(0.4)
    if rc != 0 or '"agents":0' in out:
        stop_daemon(d, ctl_sock)
        proc.kill()
        print(f"FAIL: status shows no agents: rc={rc} out={out!r}",
              file=sys.stderr)
        return 1
    st = json.loads(out)
    if not st.get("ok") or st.get("policy_epoch") != 1:
        stop_daemon(d, ctl_sock)
        proc.kill()
        print(f"FAIL: status wrong: {st}", file=sys.stderr)
        return 1
    print(f"status ok: {st.get('agents')} agent(s), epoch "
          f"{st.get('policy_epoch')}")

    rc, out = ctl(ctl_bin, ctl_sock, "ps")
    if rc != 0 or proc.pid not in map(
            lambda a: a.get("pid"), json.loads(out).get(
                "registry", {}).get("agents", [])):
        stop_daemon(d, ctl_sock)
        proc.kill()
        print(f"FAIL: ps lacks the target pid {proc.pid}: {out!r}",
              file=sys.stderr)
        return 1
    print("ps ok: target visible in the registry")

    # ---- live policy push: the probe file stops appearing ------
    n = wait_files(creatable_dir, lambda n: n >= 2, 15)
    if n is None:
        stop_daemon(d, ctl_sock)
        proc.kill()
        print("FAIL: target never created files (baseline)",
              file=sys.stderr)
        return 1
    rc, out = ctl(ctl_bin, ctl_sock, "policy-push", pol2)
    if rc != 0:
        stop_daemon(d, ctl_sock)
        proc.kill()
        print(f"FAIL: policy-push rc={rc}: {out!r}", file=sys.stderr)
        return 1
    pushed_at = files_count(creatable_dir)
    time.sleep(2.5)
    if files_count(creatable_dir) != pushed_at:
        stop_daemon(d, ctl_sock)
        proc.kill()
        print("FAIL: files kept appearing after the deny push",
              file=sys.stderr)
        return 1
    print(f"push ok: epoch 2 live, probe denied at {pushed_at} files")

    rc, out = ctl(ctl_bin, ctl_sock, "freeze")
    if rc != 0:
        stop_daemon(d, ctl_sock)
        proc.kill()
        print(f"FAIL: freeze rc={rc}: {out!r}", file=sys.stderr)
        return 1
    fr = json.loads(out)
    print(f"freeze ok: epoch {fr.get('epoch')} -> {fr.get('pushed')} "
          f"agent(s)")

    n = wait_files(creatable_dir, lambda n: False, 4)
    frozen_at = files_count(creatable_dir)
    time.sleep(2.0)
    if files_count(creatable_dir) != frozen_at:
        stop_daemon(d, ctl_sock)
        proc.kill()
        print("FAIL: files kept appearing under freeze",
              file=sys.stderr)
        return 1
    print(f"kernel truth: frozen at {frozen_at} files, none since")

    rc, out = ctl(ctl_bin, ctl_sock, "thaw")
    if rc != 0:
        stop_daemon(d, ctl_sock)
        proc.kill()
        print(f"FAIL: thaw rc={rc}: {out!r}", file=sys.stderr)
        return 1
    time.sleep(1.5)
    if files_count(creatable_dir) != frozen_at:
        stop_daemon(d, ctl_sock)
        proc.kill()
        print("FAIL: files appeared between thaw and push-back",
              file=sys.stderr)
        return 1
    print(f"thaw ok: thawed to the saved policy "
          f"({frozen_at} files still held)")

    rc, out = ctl(ctl_bin, ctl_sock, "policy-push", pol3)
    if rc != 0:
        stop_daemon(d, ctl_sock)
        proc.kill()
        print(f"FAIL: second push rc={rc}: {out!r}", file=sys.stderr)
        return 1
    resumed = wait_files(creatable_dir,
                         lambda n: n > frozen_at, 20)
    if resumed is None:
        stop_daemon(d, ctl_sock)
        proc.kill()
        print("FAIL: files never resumed after the push-back",
              file=sys.stderr)
        return 1
    print(f"push-back ok: work resumed "
          f"({frozen_at} -> {resumed} files)")

    rc, out = ctl(ctl_bin, ctl_sock, "kill", str(proc.pid))
    if rc != 0:
        stop_daemon(d, ctl_sock)
        proc.kill()
        print(f"FAIL: kill rc={rc}: {out!r}", file=sys.stderr)
        return 1
    try:
        proc.wait(timeout=8)
    except subprocess.TimeoutExpired:
        stop_daemon(d, ctl_sock)
        proc.kill()
        print("FAIL: target survived ctl kill", file=sys.stderr)
        return 1
    print("kill ok: target terminated")

    stop_daemon(d, ctl_sock)
    with open(journal) as f:
        names = [json.loads(ln).get("ev", {}).get("name")
                 for ln in f if ln.strip()]
    for expect in ("retrace.policy.pushed", "retrace.policy.freeze",
                   "retrace.policy.thaw", "retrace.ctl.kill"):
        if expect not in names:
            print(f"FAIL: journal missing {expect}", file=sys.stderr)
            return 1
    print("journal ok: push/freeze/thaw/kill all audited")

    print("PASS: ctl (status + ps + freeze/thaw kernel truth + "
          "kill + audit trail)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
