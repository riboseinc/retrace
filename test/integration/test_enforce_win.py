#!/usr/bin/env python3
"""
E2E: the AppContainer kernel-enforcement backend
(TODO.beyond-libc/01 P1) -- Windows only, self-skips elsewhere.

The compile side runs everywhere (the spec carries the container
name + declared read/write paths). This test drives the INSTALLER:
retrace-enforce launches the command inside the AppContainer; the
container's deny-by-default is the kernel floor -- a write to an
undeclared path must be refused BY THE OS.

Usage: test_enforce_win.py <retrace-profile> <retrace-enforce>
"""
import json
import os
import subprocess
import sys
import tempfile


def main():
    if len(sys.argv) != 3:
        print("usage: test_enforce_win.py <retrace-profile>"
              " <retrace-enforce>", file=sys.stderr)
        return 2
    if os.name != "nt":
        print("SKIP: appcontainer exists only on Windows",
              file=sys.stderr)
        return 0
    profile, enforce = (os.path.abspath(p) for p in sys.argv[1:3])

    work = tempfile.mkdtemp(prefix="ac-")
    denied = os.path.join(work, "denied")
    os.mkdir(denied)
    target = os.path.join(denied, "f.txt")

    app = {
        "profile": {
            "entries": 1,
            "accesses": [
                {"path": os.path.join(work, "allowed").replace("\\", "/"),
                 "class": "write", "hits": 1},
            ],
        }
    }
    app_path = os.path.join(work, "app.json")
    with open(app_path, "w") as f:
        json.dump(app, f)
    spec_path = os.path.join(work, "spec.json")

    r = subprocess.run(
        [profile, "enforce", app_path, "--backend", "appcontainer",
         "--exec", "C:/Windows/System32/cmd.exe", "-o", spec_path],
        capture_output=True, text=True, timeout=60)
    if r.returncode != 0:
        print(f"FAIL: compile rc={r.returncode}: {r.stderr[:300]}",
              file=sys.stderr)
        return 1
    with open(spec_path) as f:
        spec = json.load(f)
    if not spec.get("appcontainer", {}).get("name"):
        print("FAIL: no appcontainer section in the spec",
              file=sys.stderr)
        return 1

    # inside the container: the UNDECLARED write must be refused.
    # cmd /c with one argument string carries the redirection.
    r = subprocess.run(
        [enforce, spec_path, "--",
         "C:/Windows/System32/cmd.exe", "/c",
         f'echo x > "{target}"'],
        capture_output=True, text=True, timeout=60)
    if os.path.exists(target):
        print("FAIL: the container wrote an UNDECLARED path",
              file=sys.stderr)
        return 1

    print("appcontainer: spec compiled; undeclared write refused "
          "by the container -- OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
