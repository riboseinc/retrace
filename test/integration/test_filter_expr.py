#!/usr/bin/env python3
"""
E2E: the filter expression language (TODO.impl/08) on the live
preload lane. A target opens .log and .txt files; a filter
expression admits only the .log opens to log_params -- the log
carries foo.log, never foo.txt. A second run pins the config
contract: a bad expression is a CONFIG ERROR (refused at boot,
error in the evidence), not a silent no-match.

Usage: test_filter_expr.py <lib-path>
"""
import json
import os
import subprocess
import sys
import tempfile

TARGET_SRC = r"""
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

int main(void)
{
	int i;

	for (i = 0; i < 3; i++) {
		int a = open("foo.log", O_RDONLY);
		int b = open("foo.txt", O_RDONLY);
		int c = open("bar.log", O_RDONLY);

		if (a >= 0) close(a);
		if (b >= 0) close(b);
		if (c >= 0) close(c);
	}
	printf("done\n");
	return 0;
}
"""


def run_with_config(lib, work, cfg_obj):
    cfg = os.path.join(work, "cfg.json")
    with open(cfg, "w") as f:
        json.dump(cfg_obj, f)

    log = os.path.join(work, "evidence.log")
    env = dict(os.environ)
    env["RETRACE_JSON_CONFIG"] = cfg
    env["RETRACE_LOGGER_DEF_ENA"] = "1"
    env["RETRACE_LOGGER_DEF_STDOUT_ENA"] = "0"
    env["RETRACE_LOGGER_DEF_FN"] = log
    if sys.platform == "darwin":
        env["DYLD_INSERT_LIBRARIES"] = lib
    else:
        env["LD_PRELOAD"] = lib
    return subprocess.run([os.path.join(work, "target")],
                          env=env, stdout=subprocess.PIPE,
                          stderr=subprocess.STDOUT,
                          timeout=30), log


def main():
    if len(sys.argv) != 2:
        print("usage: test_filter_expr.py <lib-path>", file=sys.stderr)
        return 2

    lib = os.path.abspath(sys.argv[1])
    work = tempfile.mkdtemp(prefix="filter-expr-")

    with open(os.path.join(work, "target.c"), "w") as f:
        f.write(TARGET_SRC)
    build = subprocess.run(
        ["cc", "-o", os.path.join(work, "target"),
         os.path.join(work, "target.c")],
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if build.returncode != 0:
        print("FAIL: target build failed", file=sys.stderr)
        return 1

    script = {
        "func_name": "open",
        "actions": [
            {"action_name": "filter",
             "action_params": {"expr": "path ~ \"*.log\""}},
            {"action_name": "log_params"},
            {"action_name": "call_real"}]}
    p, log = run_with_config(lib, work, {"intercept_scripts": [script]})
    if p.returncode != 0 or b"done" not in p.stdout:
        print(f"FAIL: target died:\n{p.stdout.decode()[-500:]}",
              file=sys.stderr)
        return 1

    with open(log, "r", errors="replace") as f:
        ev = f.read().replace("\\/", "/")

    logged_opens = ev.count('"func": "open"')
    if logged_opens == 0:
        print("FAIL: nothing logged (filter matched nothing)",
              file=sys.stderr)
        return 1
    if "foo.txt" in ev:
        print("FAIL: foo.txt was logged -- the expression let it "
              "through", file=sys.stderr)
        return 1
    if "foo.log" not in ev or "bar.log" not in ev:
        print("FAIL: the .log opens were not logged", file=sys.stderr)
        return 1
    print(f"filter held: {logged_opens} .log opens logged, "
          "foo.txt absent")

    bad = {"intercept_scripts": [{
        "func_name": "open",
        "actions": [
            {"action_name": "filter",
             "action_params": {"expr": "path ~ "}},
            {"action_name": "call_real"}]}]}
    p, log = run_with_config(lib, work, bad)
    with open(log, "r", errors="replace") as f:
        ev = f.read()

    if "filter expr" not in ev:
        print(f"FAIL: bad expression accepted silently:\n{ev[-500:]}",
              file=sys.stderr)
        return 1
    print("config contract held: bad expression refused at boot")
    print("PASS: filter expression language works end to end")
    return 0


if __name__ == "__main__":
    sys.exit(main())
