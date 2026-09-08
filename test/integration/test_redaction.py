#!/usr/bin/env python3
"""
E2E: evidence redaction (TODO.impl/01). Secrets never leave the
process: with patterns configured, the leak vectors the evidence
plane actually has -- dereferenced string params (paths with
credentials, env names) and logged string arguments -- come out
as ***; without patterns, the same run logs them verbatim (the
zero-delta side of the contract).

Usage: test_redaction.py <retrace-lib-dir>  (compiles its target)
"""
import json
import os
import subprocess
import sys
import tempfile

TARGET_SRC = r"""
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
int main(void)
{
	const char *v = getenv("SECRET_TOKEN");

	printf("got %s\n", v ? v : "nil");
	open("/tmp/token=x96_file", 0);
	return 0;
}
"""


def run(lib, env_extra, work):
    env = dict(os.environ)
    env["RETRACE_LOGGER_DEF_ENA"] = "1"
    env["SECRET_TOKEN"] = "hunter2x"
    if "DYLD_INSERT_LIBRARIES" not in env and "LD_PRELOAD" not in env:
        if sys.platform == "darwin":
            env["DYLD_INSERT_LIBRARIES"] = lib
        else:
            env["LD_PRELOAD"] = lib
    env.update(env_extra)
    p = subprocess.run([os.path.join(work, "t")], env=env,
                       stdout=subprocess.PIPE,
                       stderr=subprocess.STDOUT, timeout=30)
    return p.stdout.decode(errors="replace")


def parse_log(out):
    """collect every string the evidence plane emitted"""
    strings = []
    dec = json.JSONDecoder()
    i = 0
    while i < len(out):
        while i < len(out) and out[i] != "{":
            i += 1
        if i >= len(out):
            break
        try:
            obj, j = dec.raw_decode(out, i)
            i = j
            strings.append(json.dumps(obj))
        except ValueError:
            i += 1
    return "\n".join(strings)


def main():
    if len(sys.argv) != 2:
        print("usage: test_redaction.py <lib-path>",
              file=sys.stderr)
        return 2
    lib = os.path.abspath(sys.argv[1])
    work = tempfile.mkdtemp(prefix="redact-")

    with open(os.path.join(work, "t.c"), "w") as f:
        f.write(TARGET_SRC)
    cc = subprocess.run(["cc", "-o", os.path.join(work, "t"),
                         os.path.join(work, "t.c")])
    if cc.returncode != 0:
        print("FAIL: cannot compile target", file=sys.stderr)
        return 1

    cfg = os.path.join(work, "redact.json")
    with open(cfg, "w") as f:
        json.dump({
            "redact": ["*_TOKEN*", "token=x96*", "hunter2*"],
            "intercept_scripts": [{
                "func_name": "*",
                "actions": [{"action_name": "log_params"},
                            {"action_name": "call_real"}]}],
        }, f)

    # the armed side: no secret survives the evidence plane
    # (markers never flow into the failure message -- CodeQL's
    # secret-logging rule reads this file too)
    out = parse_log(run(lib, {"RETRACE_JSON_CONFIG": cfg}, work))
    if any(m in out for m in ("hunter2x", "x96_file",
                              "SECRET_TOKEN")):
        print("FAIL: a marker leaked with redaction on:\n"
              f"{out[:800]}", file=sys.stderr)
        return 1
    if "***" not in out:
        print("FAIL: no redaction markers in the log",
              file=sys.stderr)
        return 1
    print("armed: secrets absent, *** present")

    # the zero-delta side: same run, no patterns, verbatim
    plain = os.path.join(work, "plain.json")
    with open(plain, "w") as f:
        json.dump({
            "intercept_scripts": [{
                "func_name": "*",
                "actions": [{"action_name": "log_params"},
                            {"action_name": "call_real"}]}],
        }, f)
    out2 = parse_log(run(lib, {"RETRACE_JSON_CONFIG": plain},
                         work))
    if not all(m in out2 for m in ("hunter2x", "token=x96_file",
                                   "SECRET_TOKEN")):
        print("FAIL: a marker missing without redaction "
              "(zero-delta broken)", file=sys.stderr)
        return 1
    print("unarmed: verbatim, zero-delta holds")

    # the env form rides when the config key is absent
    out3 = parse_log(run(lib, {
        "RETRACE_JSON_CONFIG": plain,
        "RETRACE_REDACT": "*_TOKEN*,token=x96*",
    }, work))
    # only the patterns listed ride the env form: the pair and
    # the name go, the unpatterned printf argument stays
    if any(m in out3 for m in ("x96_file", "SECRET_TOKEN")):
        print("FAIL: RETRACE_REDACT env form ignored",
              file=sys.stderr)
        return 1
    if "hunter2x" not in out3:
        print("FAIL: unpatterned text redacted (overreach)",
              file=sys.stderr)
        return 1
    print("env form: honored, scoped to its patterns")
    print("PASS: evidence redaction holds on all three contracts")
    return 0


if __name__ == "__main__":
    sys.exit(main())
