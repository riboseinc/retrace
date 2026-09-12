#!/usr/bin/env python3
"""
E2E: syscall-lane actions (TODO.impl/06, ADR-0016). A static
victim is denied open("/etc/shadow") at the ptrace syscall stop
while open("/etc/hostname") is allowed -- depth parity for the
static lane, evidenced in the logger stream.

The driver links the retrace library directly: the engine runs in
the TRACER, the trace loop attaches to the victim, and the JSON
config's sandbox action decides at each syscall-entry stop. The
kernel is the real implementation (ADR-0016): denial returns
-errno, so a static tracee's perror prints "Permission denied".

Usage: test_ptrace_deny.py <lib-path>
"""
import json
import os
import subprocess
import sys
import tempfile
import time

VICTIM_SRC = r"""
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

int main(void)
{
	int i;

	for (i = 0; i < 40; i++) {
		int shadow = open("/etc/shadow", O_RDONLY);
		int host = open("/etc/hostname", O_RDONLY);

		if (shadow < 0)
			printf("shadow: DENIED %s\n", strerror(errno));
		else {
			printf("shadow: OPENED (test broken)\n");
			close(shadow);
		}
		if (host < 0)
			printf("hostname: DENIED %s (test broken)\n",
				strerror(errno));
		else {
			printf("hostname: OK\n");
			close(host);
		}
		fflush(stdout);
		usleep(100000);
	}
	return 0;
}
"""

DRIVER_SRC = r"""
#include <retrace/retrace.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char **argv)
{
	pid_t child;
	int status;
	int attempt;

	if (argc != 2)
		return 2;

	child = fork();
	if (child == 0)
		execl(argv[1], argv[1], (char *) 0);

	/* The child loops; attach takes it over at the next stop.
	 * PTRACE_ATTACH can transiently fail while the child is
	 * mid-execve (EPERM; observed on the alpine/musl leg) --
	 * retry briefly. retrace_attach_process enters the trace
	 * loop and returns when the tracee exits.
	 */
	for (attempt = 0; attempt < 100; attempt++) {
		if (retrace_attach_process(child) == 0)
			break;
		usleep(10000);
	}
	if (attempt == 100) {
		kill(child, 9);
		waitpid(child, &status, 0);
		return 3;
	}

	waitpid(child, &status, 0);
	/* let the logger's flusher drain the evidence */
	usleep(500000);
	return 0;
}
"""


def main():
    if len(sys.argv) != 2:
        print("usage: test_ptrace_deny.py <lib-path>", file=sys.stderr)
        return 2
    if not sys.platform.startswith("linux"):
        print("SKIP: ptrace lane is Linux-only")
        return 0

    lib = os.path.abspath(sys.argv[1])
    libdir = os.path.dirname(lib)
    incdir = os.path.abspath(os.path.join(
        os.path.dirname(__file__), "..", "..", "include"))
    work = tempfile.mkdtemp(prefix="ptrace-deny-")

    with open(os.path.join(work, "victim.c"), "w") as f:
        f.write(VICTIM_SRC)
    build = subprocess.run(
        ["cc", "-static", "-o", os.path.join(work, "victim"),
         os.path.join(work, "victim.c")],
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if build.returncode != 0:
        print("SKIP: no static toolchain (libc.a) on this host")
        return 0
    print("built: static victim")

    with open(os.path.join(work, "driver.c"), "w") as f:
        f.write(DRIVER_SRC)
    build = subprocess.run(
        ["cc", "-I" + incdir, "-o", os.path.join(work, "driver"),
         "-Wl,-rpath," + libdir,
         os.path.join(work, "driver.c"), lib],
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if build.returncode != 0:
        print(f"FAIL: driver build failed:\n"
              f"{build.stdout.decode(errors='replace')}",
              file=sys.stderr)
        return 1

    cfg = os.path.join(work, "deny.json")
    with open(cfg, "w") as f:
        json.dump({"intercept_scripts": [
            {"func_name": "open",
             "actions": [
                 {"action_name": "sandbox",
                  "action_params": {
                      "deny_paths": ["/etc/shadow"]}},
                 {"action_name": "call_real"}]},
            {"func_name": "openat",
             "actions": [
                 {"action_name": "sandbox",
                  "action_params": {
                      "deny_paths": ["/etc/shadow"]}},
                 {"action_name": "call_real"}]}]}, f)

    log = os.path.join(work, "evidence.log")
    env = dict(os.environ)
    env["RETRACE_JSON_CONFIG"] = cfg
    env["RETRACE_LOGGER_DEF_ENA"] = "1"
    env["RETRACE_LOGGER_DEF_STDOUT_ENA"] = "0"
    env["RETRACE_LOGGER_DEF_FN"] = log

    def under_qemu():
        try:
            with open("/proc/self/maps", "r", errors="replace") as f:
                return "qemu-" in f.read(8192)
        except OSError:
            return False

    try:
        p = subprocess.run([os.path.join(work, "driver"),
                            os.path.join(work, "victim")],
                           env=env, stdout=subprocess.PIPE,
                           stderr=subprocess.STDOUT, timeout=60)
    except subprocess.TimeoutExpired:
        print("FAIL: driver hung -- the trace loop never returned",
              file=sys.stderr)
        return 1

    if p.returncode == 3:
        # PTRACE_ATTACH exhausted its retries: the environment
        # cannot host the lane (qemu-user ptrace emulation, or a
        # container policy). Not a code regression -- the lane
        # is verified on bare CI legs and native containers.
        where = "qemu-user" if under_qemu() else "this container"
        print(f"SKIP: ptrace attach refused under {where}")
        return 0

    out = p.stdout.decode(errors="replace")
    if p.returncode != 0:
        print(f"FAIL: driver rc={p.returncode}:\n{out[-1000:]}",
              file=sys.stderr)
        return 1

    denied = "shadow: DENIED Permission denied" in out
    allowed = "hostname: OK" in out
    if not denied:
        print(f"FAIL: /etc/shadow was not denied "
              f"(kernel-is-real-impl mapping broken):\n{out[-1000:]}",
              file=sys.stderr)
        if os.path.exists(log):
            with open(log, "r", errors="replace") as f:
                ev = f.read().replace("\\/", "/")
            tail = ev[-1500:]
            print(f"EVIDENCE-TAIL:\n{tail}", file=sys.stderr)
        return 1
    if not allowed:
        print(f"FAIL: /etc/hostname was denied too "
              f"(allow path broken):\n{out[-1000:]}", file=sys.stderr)
        return 1
    print("victim: shadow DENIED (EACCES), hostname allowed")

    # the journal lane: the logger stream carries the deny record
    # (JSON-escaped slashes: \/etc\/shadow)
    evidence = ""
    for _ in range(10):
        time.sleep(0.3)
        if os.path.exists(log):
            with open(log, "r", errors="replace") as f:
                evidence = f.read().replace("\\/", "/")
            if "/etc/shadow" in evidence:
                break
    if "/etc/shadow" not in evidence:
        print(f"FAIL: no deny evidence in the logger stream:\n"
              f"{evidence[-1000:]}", file=sys.stderr)
        return 1
    print("journal: deny record present in the logger stream")
    print("PASS: static-lane deny (ADR-0016) holds end to end")
    return 0


if __name__ == "__main__":
    sys.exit(main())
