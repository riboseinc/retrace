#!/usr/bin/env python3
"""
E2E: the reference compose stack (TODO.impl/16) -- CI validates
the compose file itself (docker compose config: interpolation,
service graph, volume wiring); the full up/down flow is the
laptop path documented in the example's README. Self-skips
without a compose binary (probe trouble is a skip, never a
failure).

Usage: test_compose.py <examples-dir>
"""
import os
import shutil
import subprocess
import sys


def main():
    if len(sys.argv) != 2:
        print("usage: test_compose.py <examples-dir>",
              file=sys.stderr)
        return 2
    exdir = os.path.abspath(os.path.join(sys.argv[1],
                                         "supervisor-compose"))
    if not os.path.exists(os.path.join(exdir, "compose.yaml")):
        print("FAIL: compose.yaml missing", file=sys.stderr)
        return 1

    compose = shutil.which("docker")
    if compose is None:
        print("SKIP: docker not available")
        return 0

    env = dict(os.environ)
    env.setdefault("RETRACE_BUILD", "/build")
    env.setdefault("RETRACE_NONCE",
                   "0123456789abcdef0123456789abcdef")
    p = subprocess.run(
        [compose, "compose", "-f",
         os.path.join(exdir, "compose.yaml"), "config", "--quiet"],
        env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if p.returncode != 0:
        print(f"FAIL: compose config invalid:\n"
              f"{p.stdout.decode()[-800:]}", file=sys.stderr)
        return 1
    print("compose config valid")

    # the service graph the README promises
    p = subprocess.run(
        [compose, "compose", "-f",
         os.path.join(exdir, "compose.yaml"), "config",
         "--services"],
        env=env, stdout=subprocess.PIPE)
    services = p.stdout.decode().split()
    for want in ("otelcol", "retraced", "detonation"):
        if want not in services:
            print(f"FAIL: service {want} missing from the stack",
                  file=sys.stderr)
            return 1
    print(f"services: {', '.join(sorted(services))}")
    print("PASS: the reference stack validates")
    return 0


if __name__ == "__main__":
    sys.exit(main())
