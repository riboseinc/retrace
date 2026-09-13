#!/usr/bin/env python3
"""
E2E: the coverage-guided bridge (TODO.impl/17). Two seeds whose
runs die with the SAME signature (the same marker, the same
last logged call) but DIFFERENT call histories: the call-hash
id separates them into two clusters where the stack signature
saw one; --emit-corpus reports the minimized corpus (one seed
per cluster).

Usage: test_fuzz_cov.py <retrace-fuzz-report> <retrace-lib>
       <target-binary>
"""
import json
import os
import shutil
import subprocess
import sys
import tempfile

FNV_OFFSET = 0x811C9DC5
FNV_PRIME = 0x01000193


def fnv1a(data: bytes) -> int:
    h = FNV_OFFSET
    for b in data:
        h = ((h ^ b) * FNV_PRIME) & 0xFFFFFFFF
    return h


def seed_with_parity(prefix: str, want_odd: bool) -> bytes:
    n = 0
    while True:
        content = f"{prefix}-{n}\n".encode()
        if (fnv1a(content) % 2 == 1) == want_odd:
            return content
        n += 1


def main():
    if len(sys.argv) != 4:
        print("usage: test_fuzz_cov.py <fuzz-report> <retrace-lib>"
              " <target>", file=sys.stderr)
        return 2
    tool, lib, target = (os.path.abspath(p) for p in sys.argv[1:4])
    if sys.platform == "win32":
        print("SKIP: fuzz-report is POSIX-only in v1")
        return 0

    work = tempfile.mkdtemp(prefix="fzcov-")
    seeds = os.path.join(work, "seeds")
    corpus = os.path.join(work, "corpus")
    cfg = os.path.join(work, "cfg.json")
    os.makedirs(seeds)
    with open(os.path.join(seeds, "even"), "wb") as f:
        f.write(seed_with_parity("even", want_odd=False))
    with open(os.path.join(seeds, "odd"), "wb") as f:
        f.write(seed_with_parity("odd", want_odd=True))
    with open(cfg, "w") as f:
        f.write(json.dumps({"intercept_scripts": [{
            "func_name": "strlen",
            "actions": [{"action_name": "log_params"},
                        {"action_name": "call_real"}],
        }]}))

    report = os.path.join(work, "report.json")
    r = subprocess.run(
        [tool, "--config", cfg, "--seeds", seeds,
         "--lib", lib, "--marker", "FUZZASSERT",
         "--emit-corpus", corpus, "-o", report,
         "--", target],
        capture_output=True, text=True, timeout=120)
    if r.returncode == 0:
        print("FAIL: expected crash clusters (exit 1), got 0",
              file=sys.stderr)
        print(r.stdout[-300:], file=sys.stderr)
        return 1
    if r.returncode not in (0, 1):
        print(f"FAIL: tool rc={r.returncode}: {r.stderr[:300]}",
              file=sys.stderr)
        return 1

    with open(report) as f:
        rep = json.load(f)
    clusters = rep.get("clusters", [])
    crash_clusters = [c for c in clusters if c.get("count", 0) > 0
                      and c.get("is_crash", 0) == 0]
    covs = sorted(c.get("coverage", 0) for c in clusters
                  if c.get("coverage", 0) != 0)
    if len(clusters) < 2:
        print(f"FAIL: same-signature deaths did not separate: "
              f"{clusters}", file=sys.stderr)
        return 1
    if len(covs) < 2 or covs[0] == covs[-1]:
        print(f"FAIL: clusters carry no differing coverage: "
              f"{clusters}", file=sys.stderr)
        return 1

    minimized = sorted(os.listdir(corpus))
    if len(minimized) < 2:
        print(f"FAIL: minimized corpus lost a cluster's seed: "
              f"{minimized}", file=sys.stderr)
        return 1

    shutil.rmtree(work, ignore_errors=True)
    print("fuzz-cov: same signature separated by coverage "
          f"({len(covs)} distinct histories); minimized corpus "
          f"= {len(minimized)} seeds -- OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
