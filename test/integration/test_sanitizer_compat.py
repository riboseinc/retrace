#!/usr/bin/env python3
"""
E2E: the sanitizer compatibility matrix (TODO.impl/05), as a
tripwire -- the test IS the matrix's evidence.

The fuzzing persona wants retrace fault injection ON a
sanitizer-instrumented target. The per-OS truth:

- Linux: LD_PRELOAD composes with sanitizer runtimes (the
  binary's linked runtime wins the allocator; retrace's
  trampolines ride above it). This test PROVES the combo: an
  ASAN target survives under retrace with a fault firing.
- macOS: DYLD_INSERT_LIBRARIES + ANY sanitizer runtime is a
  dyld-level conflict -- ASAN/TSAN die with SIGILL at init,
  UBSAN with SIGSEGV, every insertion ordering, with or
  without a fault config (30/30 local runs, 2026-09-12). The
  test asserts the documented incompatibility so the day a
  toolchain fixes it, this test fails and the docs follow.

Usage: test_sanitizer_compat.py <lib-path>
"""
import json
import os
import subprocess
import sys
import tempfile

TARGET_SRC = r"""
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
int main(void)
{
	char *a = malloc(16);
	char *b = malloc(16);

	if (a == NULL || b == NULL) {
		printf("oom\n");
		return 0;
	}
	strcpy(a, "hello");
	strcpy(b, "world");
	printf("%s%s\n", a, b);
	free(a);
	free(b);
	return 0;
}
"""


def main():
    if len(sys.argv) != 2:
        print("usage: test_sanitizer_compat.py <lib-path>",
              file=sys.stderr)
        return 2
    lib = os.path.abspath(sys.argv[1])
    work = tempfile.mkdtemp(prefix="san-")

    with open(os.path.join(work, "t.c"), "w") as f:
        f.write(TARGET_SRC)
    build = subprocess.run(
        ["cc", "-fsanitize=address", "-o",
         os.path.join(work, "t"), os.path.join(work, "t.c")],
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if build.returncode != 0:
        print("SKIP: toolchain lacks -fsanitize=address")
        return 0
    print("built: ASAN-instrumented target")

    # a fault config: the first malloc fails; a sanitizer that
    # respects the interposition sees a clean NULL path
    cfg = os.path.join(work, "fz.json")
    with open(cfg, "w") as f:
        json.dump({"intercept_scripts": [
            {"func_name": "malloc",
             "actions": [
                 {"action_name": "fail_first",
                  "action_params": {"count": 1, "error": 12}},
                 {"action_name": "call_real"}]}]}, f)

    env = dict(os.environ)
    env["RETRACE_JSON_CONFIG"] = cfg
    env["RETRACE_LOGGER_DEF_ENA"] = "1"
    if sys.platform == "darwin":
        env["DYLD_INSERT_LIBRARIES"] = lib
    else:
        # the documented rule (platforms.md): with any LD_PRELOAD
        # set, ASAN requires ITS runtime first -- "ASan runtime
        # does not come first in initial library list" is the
        # failure this prevents. retrace rides second. The
        # runtime's name is toolchain-dependent (clang:
        # libclang_rt.asan-<arch>.so.1; gcc: libasan.so.6), and
        # -print-file-name echoes a DIRECTORY when the name is
        # unknown -- a file check, not an exists check.
        arch = os.uname().machine
        rt = None
        for name in (f"libclang_rt.asan-{arch}.so.1",
                     "libasan.so.6"):
            cand = subprocess.run(
                ["cc", f"-print-file-name={name}"],
                stdout=subprocess.PIPE,
            ).stdout.decode().strip()
            if os.path.isfile(cand):
                rt = cand
                break
        if rt is None:
            print("SKIP: ASAN runtime not found for this "
                  "toolchain")
            return 0
        env["LD_PRELOAD"] = rt + ":" + lib
    p = subprocess.run([os.path.join(work, "t")], env=env,
                       stdout=subprocess.PIPE,
                       stderr=subprocess.STDOUT, timeout=30)
    out = p.stdout.decode(errors="replace")

    if sys.platform == "darwin":
        # the documented matrix cell: dyld kills the process
        # before main. If this ever passes, the platform truth
        # changed -- update docs/platforms.md with it.
        if p.returncode == 0:
            print("FAIL: macOS + DYLD_INSERT + ASAN succeeded; "
                  "the documented incompatibility is stale -- "
                  "update the matrix",
                  file=sys.stderr)
            return 1
        print(f"macOS cell holds: sanitizer runtime + "
              f"DYLD_INSERT is fatal (rc={p.returncode})")
        print("PASS: matrix tripwire (macOS: unsupported, "
              "as documented)")
        return 0

    # the Linux cell: the combo must WORK -- target survives,
    # the fault fired (the first malloc returned NULL)
    if p.returncode != 0:
        print(f"FAIL: ASAN target died under retrace "
              f"(rc={p.returncode}):\n{out[-800:]}",
              file=sys.stderr)
        return 1
    if "oom" not in out:
        print(f"FAIL: fault never surfaced:\n{out[-800:]}",
              file=sys.stderr)
        return 1
    print("linux cell holds: ASAN target survived, fault fired")
    print("PASS: matrix tripwire (Linux: supported)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
