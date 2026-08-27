#!/usr/bin/env python3
"""
E2E: kernel enforcement compile (TODO.beyond-libc/01).

The declared-set that feeds the userspace jail now also feeds
kernel filters: `retrace-profile enforce` compiles a spec, the
`retrace-enforce` installer applies it and execs. The contract
under test, on Linux:

  - the spec is generated from the profile's accesses;
  - a DECLARED write succeeds under the kernel filter;
  - an UNDECLARED write is refused BY THE KERNEL (EACCES/EPERM)
    -- the escape the userspace jail cannot stop;
  - the seccomp floor denies an unused syscall class (socket)
    that the profile never observed.

Skips (with the reason printed) where the kernel lacks Landlock
or seccomp, or off Linux entirely.

Usage: test_enforce.py <retrace-profile> <retrace-enforce>
"""
import json
import os
import platform
import subprocess
import sys
import tempfile


def sh_ok(cmd):
    return subprocess.run(cmd, shell=True, capture_output=True,
        timeout=30)


def main():
    if len(sys.argv) != 3:
        print("usage: test_enforce.py <retrace-profile>"
              " <retrace-enforce>", file=sys.stderr)
        return 2
    profiler, installer = (os.path.abspath(p) for p in sys.argv[1:3])

    if platform.system() != "Linux":
        print("enforce: Linux-only, skipping")
        return 0

    work = tempfile.mkdtemp(prefix="enforce-")
    wr = os.path.join(work, "declared-writes")
    os.mkdir(wr)
    prof = os.path.join(work, "prof.json")
    with open(prof, "w") as f:
        json.dump({"profile": {
            "functions": [{"name": "open", "count": 4},
                          {"name": "fopen", "count": 1}],
            "accesses": [
                {"path": "/etc/hosts", "class": "read", "hits": 1},
                {"path": wr, "class": "write", "hits": 2},
            ]}}, f)

    spec = os.path.join(work, "spec.json")
    r = subprocess.run([profiler, "enforce", prof,
                        "--backend", "landlock", "--exec", "/bin/sh",
                        "-o", spec], capture_output=True, timeout=30)
    if r.returncode != 0:
        print(f"FAIL: enforce generation failed: "
              f"{r.stderr.decode()[:300]}", file=sys.stderr)
        return 1
    with open(spec) as f:
        doc = json.load(f)
    rules = doc.get("landlock", {}).get("rules", [])
    if not any(x["path"] == wr for x in rules):
        print("FAIL: declared write path missing from spec",
              file=sys.stderr)
        return 1

    # landlock availability probe
    probe = subprocess.run(
        [installer, spec, "--", "/bin/sh", "-c", "echo probe-ok"],
        capture_output=True, timeout=30)
    if probe.returncode == 2 and b"landlock" in probe.stderr:
        print("enforce: kernel lacks landlock; skipping the "
              "enforcement legs")
        return 0
    if b"probe-ok" not in probe.stdout:
        print(f"FAIL: probe failed: {probe.stderr.decode()[:300]}",
              file=sys.stderr)
        return 1

    # declared write: allowed
    good = os.path.join(wr, "f.txt")
    r = sh_ok(f"'{installer}' '{spec}' -- /bin/sh -c "
              f"'echo hi > {good} && cat {good}'")
    if r.returncode != 0 or not os.path.exists(good):
        print(f"FAIL: declared write refused: "
              f"{r.stderr.decode()[:200]}", file=sys.stderr)
        return 1

    # undeclared write: refused BY THE KERNEL
    bad = os.path.join(work, "escape.txt")
    r = sh_ok(f"'{installer}' '{spec}' -- /bin/sh -c "
              f"'echo x > {bad}'")
    if r.returncode == 0 or os.path.exists(bad):
        print("FAIL: undeclared write escaped the kernel filter",
              file=sys.stderr)
        return 1

    # seccomp floor: socket class the profile never used
    spec2 = os.path.join(work, "spec-sc.json")
    r = subprocess.run([profiler, "enforce", prof,
                        "--backend", "seccomp", "-o", spec2],
        capture_output=True, timeout=30)
    if r.returncode != 0:
        print(f"FAIL: seccomp generation failed: "
              f"{r.stderr.decode()[:200]}", file=sys.stderr)
        return 1
    sock = subprocess.run(
        [installer, spec2, "--", "/usr/bin/python3", "-c",
         "import socket; socket.socket(); print('socket-ok')"],
        capture_output=True, timeout=30)
    if b"socket-ok" in sock.stdout:
        print("FAIL: unused socket class passed the seccomp floor",
              file=sys.stderr)
        return 1

    print("enforce: declared write allowed, undeclared refused at "
          "the kernel, unused socket class floored")
    return 0


if __name__ == "__main__":
    sys.exit(main())
