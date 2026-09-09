#!/usr/bin/env python3
"""
E2E: the replay slice (TODO.impl/03) -- the determinism claim,
asserted. A faulted run recorded with a TIME seed; the replay
run reproduces byte-identical target output with the recorded
seed forced back, and the report says zero divergence. A
tampered record names its drift.

Usage: test_replay.py <lib-path>
"""
import os
import subprocess
import sys
import tempfile

TARGET_SRC = r"""
#include <stdlib.h>
#include <stdio.h>
int main(void)
{
	int i;
	for (i = 0; i < 20; i++) {
		char *p = malloc(8);
		printf("%d:%s ", i, p ? "a" : "F");
		free(p);
	}
	printf("\n");
	return 0;
}
"""


def run(lib, env_extra, work, env_seed=False):
    env = dict(os.environ)
    env["RETRACE_JSON_CONFIG"] = os.path.join(work, "fz.json")
    # the logger stays ON: the replay report rides it
    env["RETRACE_LOGGER_DEF_ENA"] = "1"
    if sys.platform == "darwin":
        env["DYLD_INSERT_LIBRARIES"] = lib
    else:
        env["LD_PRELOAD"] = lib
    # NOTE: no RETRACE_FUZZ_SEED -- the time fallback must ride
    # the record, or the claim is hollow
    env.update(env_extra)
    p = subprocess.run([os.path.join(work, "t")], env=env,
                       stdout=subprocess.PIPE,
                       stderr=subprocess.STDOUT, timeout=30)
    return p.stdout.decode(errors="replace")


def main():
    if len(sys.argv) != 2:
        print("usage: test_replay.py <lib-path>", file=sys.stderr)
        return 2
    lib = os.path.abspath(sys.argv[1])
    work = tempfile.mkdtemp(prefix="replay-")

    with open(os.path.join(work, "t.c"), "w") as f:
        f.write(TARGET_SRC)
    if subprocess.run(["cc", "-o", os.path.join(work, "t"),
                       os.path.join(work, "t.c")]).returncode:
        print("FAIL: cannot compile target", file=sys.stderr)
        return 1
    with open(os.path.join(work, "fz.json"), "w") as f:
        f.write('{"intercept_scripts":[{"func_name":"malloc",'
                '"actions":[{"action_name":"memory_fuzz",'
                '"action_params":{"fail_rate":0.3}},'
                '{"action_name":"call_real"}]}]}')

    rec = os.path.join(work, "rec")
    out1 = run(lib, {"RETRACE_REPLAY_OUT": rec}, work)
    if "seed " not in open(rec).read():
        print("FAIL: record carries no seed", file=sys.stderr)
        return 1
    print(f"recorded: {len(open(rec).readlines())} lines")

    out2 = run(lib, {"RETRACE_REPLAY_IN": rec}, work)

    def decisions(out):
        # the target's prints interleave with the JSON log --
        # pull the decision sequence itself
        import re

        return re.findall(r"\d+:[aF]", out)

    if decisions(out1) != decisions(out2):
        print("FAIL: replay decisions differ:",
              file=sys.stderr)
        print(f"  rec: {decisions(out1)}", file=sys.stderr)
        print(f"  got: {decisions(out2)}", file=sys.stderr)
        return 1
    if "0 diverged" not in out2:
        print(f"FAIL: replay reports divergence: {out2!r}",
              file=sys.stderr)
        return 1
    print("replay: identical output, zero divergence")

    # tamper one outcome: the drift must be NAMED
    lines = open(rec).read().split("\n")
    for i, l in enumerate(lines):
        if l.endswith(" memfuzz ok"):
            lines[i] = l[: -len("ok")] + "fail"
            break
    else:
        print("FAIL: no memfuzz outcome to tamper", file=sys.stderr)
        return 1
    open(rec, "w").write("\n".join(lines))
    out3 = run(lib, {"RETRACE_REPLAY_IN": rec}, work)
    if "diverged" not in out3 or "diverged, 0" in out3.replace(
            "matched, ", ""):
        if "1 diverged" not in out3:
            print(f"FAIL: tampered record not flagged: {out3!r}",
                  file=sys.stderr)
            return 1
    print("tamper: drift named")
    print("PASS: record/replay holds the determinism claim")
    return 0


if __name__ == "__main__":
    sys.exit(main())
