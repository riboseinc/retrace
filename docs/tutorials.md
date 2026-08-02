# retrace tutorials

Scenario-driven walkthroughs. Each tutorial answers a specific
question: "I have this problem — how do I use retrace to solve it?"
Pick the one that matches your situation.

> The website has an interactive version of these tutorials at
> `https://riboseinc.github.io/retrace/#tutorials`. This page is the
> text-only mirror for offline reading and search engines.

## Tutorials

1. [Find what's making my program slow](#1-find-whats-making-my-program-slow)
2. [Test how my code handles malloc failures](#2-test-how-my-code-handles-malloc-failures)
3. [Sandbox an untrusted binary](#3-sandbox-an-untrusted-binary)
4. [See what a binary actually does](#4-see-what-a-binary-actually-does)
5. [Make a binary think it's root (without sudo)](#5-make-a-binary-think-its-root-without-sudo)
6. [Find a memory leak](#6-find-a-memory-leak)
7. [Add fuzzing to my CI pipeline](#7-add-fuzzing-to-my-ci-pipeline)
8. [Trace an Android app's native code](#8-trace-an-android-apps-native-code)
9. [Test how my code handles network failures](#9-test-how-my-code-handles-network-failures)
10. [Mock the system clock for time-sensitive tests](#10-mock-the-system-clock-for-time-sensitive-tests)
11. [Redirect a file path to somewhere else](#11-redirect-a-file-path-to-somewhere-else)
12. [Audit what environment variables a binary reads](#12-audit-what-environment-variables-a-binary-reads)
13. [Capture all network traffic as JSON](#13-capture-all-network-traffic-as-json)
14. [Trace a statically-linked binary](#14-trace-a-statically-linked-binary)

---

## 1. Find what's making my program slow

**Time:** 5 minutes.
**Goal:** identify which libc calls dominate your program's runtime.

### Step 1 — install

```sh
curl -sSL https://raw.githubusercontent.com/riboseinc/retrace/main/scripts/install.sh | sh
```

### Step 2 — trace with HTML output

```sh
retrace trace --html --log /tmp/trace.json -- ./your-program
# wrote /tmp/retrace-43892.html
```

### Step 3 — open the report

```sh
open /tmp/retrace-43892.html
```

### Step 4 — read the category breakdown

The summary shows total time per category (I/O, MEM, NET, SYNC, …).
The biggest one is your suspect.

```
[ I/O ]   485 calls  32.1 ms (66%)
[ MEM ]   412 calls   8.3 ms (17%)
[ SYNC ]  186 calls   4.1 ms ( 8%)
```

### Step 5 — CLI alternative

For a text-only view of per-function totals:

```sh
retrace pp /tmp/trace.json | head -10
```

```
open          48 calls   12.4ms total
read         124 calls    8.7ms total
write         64 calls    6.1ms total
…
```

---

## 2. Test how my code handles malloc failures

**Time:** 5 minutes.
**Goal:** find unhandled NULL returns from allocators.

### Step 1 — quick fuzz

```sh
retrace fuzz malloc --rate 0.1 -- ./your-program
```

10% of `malloc` calls will return NULL. If your program crashes or
misbehaves, that's a bug.

### Step 2 — capture the trace

```sh
retrace fuzz malloc --rate 0.1 --log /tmp/fuzz.json -- ./your-program
```

### Step 3 — make failures deterministic

Random failures are hard to reproduce. Pin the RNG seed:

```sh
cat > /tmp/fuzz.json <<'EOF'
{
  "intercept_scripts": [
    {
      "func_name": "malloc",
      "actions": [
        { "action_name": "fuzzing_seed",     "action_params": { "seed": 42 } },
        { "action_name": "call_real" },
        { "action_name": "memory_fuzz",      "action_params": { "fail_rate": 0.1 } }
      ]
    }
  ]
}
EOF
```

### Step 4 — reproduce run-to-run

```sh
retrace run --config /tmp/fuzz.json -- ./your-program
```

Every run with seed 42 will fail the same calls. File the bug with
the seed in the report.

### Step 5 — extend to all allocators

See [cookbook recipe 09](cookbook/09-fuzz-malloc.md) for the
multi-allocator config (malloc + calloc + realloc).

---

## 3. Sandbox an untrusted binary

**Time:** 4 minutes.
**Goal:** block specific file accesses at runtime, no SELinux or AppArmor.

### Step 1 — list sensitive paths

Pick the paths the binary must not touch. Examples:
`/etc/shadow`, `/etc/sudoers`, `/root/.ssh/`, `~/.aws/credentials`.

### Step 2 — write the sandbox config

```sh
cat > /tmp/sandbox.json <<'EOF'
{
  "intercept_scripts": [
    { "func_name": "open",
      "actions": [{ "action_name": "sandbox",
        "action_params": { "deny_paths": ["/etc/shadow", "/etc/sudoers", "/root/.ssh/"] } }] },
    { "func_name": "openat",
      "actions": [{ "action_name": "sandbox",
        "action_params": { "deny_paths": ["/etc/shadow", "/etc/sudoers", "/root/.ssh/"] } }] }
  ]
}
EOF
```

### Step 3 — run the binary under the sandbox

```sh
retrace run --config /tmp/sandbox.json -- ./untrusted-binary
# sandbox: DENIED '/etc/shadow'  (open returned -2 / ENOENT)
```

### Step 4 — triage

Any `DENIED` line in the log is an attempted access the sandbox
blocked. See [cookbook recipe 20](cookbook/20-sandbox.md) for prefix
matching and other variations.

---

## 4. See what a binary actually does

**Time:** 3 minutes.
**Goal:** map every libc call of a closed-source binary without a disassembler.

### Step 1 — trace every call

```sh
retrace trace --log /tmp/trace.json -- ./mystery-binary
```

### Step 2 — filter to interesting functions

```sh
RETRACE_LOGGER_ALLOWED_FUNCS=open,openat,read,write,connect,execve,system \
  retrace trace --log /tmp/io.json -- ./mystery-binary
```

### Step 3 — interactive HTML view

```sh
retrace html /tmp/io.json -o /tmp/view.html && open /tmp/view.html
```

### Step 4 — look for surprises

- Files you didn't expect.
- Outbound connects to surprising hosts.
- `system()` calls with shell metacharacters.
- `getenv()` reads of sensitive variables.

Those are your leads.

---

## 5. Make a binary think it's root (without sudo)

**Time:** 2 minutes.
**Goal:** test root-path code without actually being root.

### Step 1 — quick mock

```sh
retrace mock getuid 0 -- ./check-root
# welcome, root
```

### Step 2 — mock getuid AND geteuid

Most root checks consult both. Use a small JSON config:

```sh
cat > /tmp/root.json <<'EOF'
{
  "intercept_scripts": [
    { "func_name": "getuid",
      "actions": [{ "action_name": "modify_return_value_int",
        "action_params": { "retval_int": 0 } }] },
    { "func_name": "geteuid",
      "actions": [{ "action_name": "modify_return_value_int",
        "action_params": { "retval_int": 0 } }] }
  ]
}
EOF
retrace run --config /tmp/root.json -- ./check-root
```

See [cookbook recipe 05](cookbook/05-mock-getuid.md) for variations.

---

## 6. Find a memory leak

**Time:** 6 minutes.
**Goal:** find allocators that aren't being matched by frees.

### Step 1 — trace allocators

```sh
retrace trace malloc,calloc,realloc,free --log /tmp/alloc.json -- ./your-program
```

### Step 2 — per-function counts

```sh
retrace pp /tmp/alloc.json | grep -E 'malloc|calloc|realloc|free'
```

```
malloc    1247 calls
calloc      42 calls
realloc     18 calls
free      1198 calls
```

### Step 3 — do the math

```
(1247 + 42 + 18) - 1198 = 109 outstanding allocations
```

If that number is non-zero and growing across multiple runs, you
have a leak.

### Step 4 — diff two runs of a server

For long-running processes, capture a trace window, hit the server,
then capture another. The diff is the leak signature:

```sh
timeout 10 retrace trace malloc,free --log /tmp/r1.json -- ./server &
# ... send some traffic ...
timeout 10 retrace trace malloc,free --log /tmp/r2.json -- ./server
diff <(retrace pp /tmp/r1.json) <(retrace pp /tmp/r2.json)
```

---

## 7. Add fuzzing to my CI pipeline

**Time:** 10 minutes.
**Goal:** catch OOM and short-IO bugs on every PR.

### Step 1 — drop in the workflow

```sh
mkdir -p .github/workflows
cat > .github/workflows/retrace-fuzz.yml <<'EOF'
name: fuzz
on: [pull_request]
jobs:
  fuzz:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Install retrace
        run: curl -sSL https://raw.githubusercontent.com/riboseinc/retrace/main/scripts/install.sh | sh
      - name: Run tests under fuzz
        run: retrace fuzz malloc --rate 0.05 --log /tmp/fuzz.json -- ./run-tests
      - uses: actions/upload-artifact@v4
        if: always()
        with:
          name: retrace-fuzz-log
          path: /tmp/fuzz.json
EOF
```

### Step 2 — open a PR

The workflow runs your tests with 5% of mallocs failing. Any
unhandled NULL crashes the test run.

### Step 3 — reproduce locally

Download the artifact, extract the seed, replay:

```sh
gh run download <run-id> -n retrace-fuzz-log
retrace run --config fuzz-seed-<N>.json -- ./run-tests
```

### Step 4 — full reference

See [cookbook recipe 19](cookbook/19-ci-fuzzing.md) for the
production-ready workflow with seed extraction and triage steps.

---

## 8. Trace an Android app's native code

**Time:** 20 minutes.
**Goal:** trace libc calls inside an Android app's native libraries.

### Step 1 — cross-compile for arm64

```sh
cmake -B build-android \
  -DCMAKE_TOOLCHAIN_FILE=cmake/android-toolchain.cmake \
  -DANDROID_ABI=arm64-v8a
cmake --build build-android
```

### Step 2 — push to device

```sh
adb push build-android/src/libretrace.so /data/local/tmp/

cat > wrap.sh <<'EOF'
#!/system/bin/sh
export LD_PRELOAD=/data/local/tmp/libretrace.so
exec "$@"
EOF
adb push wrap.sh /data/local/tmp/
```

### Step 3 — for debuggable apps

```sh
adb shell setprop wrap.<package.name> LD_PRELOAD=/data/local/tmp/libretrace.so
adb shell am start -n <package.name>/<activity>
```

### Step 4 — for production apps

Use Magisk's Zygisk module mechanism. See the
[Android guide](android.md) for the full setup.

---

## 9. Test how my code handles network failures

**Time:** 4 minutes.
**Goal:** force connect() to fail with ECONNREFUSED or ENETUNREACH without pulling the network cable.

### Step 1 — decide which errno to inject

Common POSIX network errnos:
- ECONNREFUSED = 111
- ENETUNREACH = 101
- ETIMEDOUT = 110
- EHOSTUNREACH = 113

### Step 2 — write the config

```sh
cat > /tmp/netfail.json <<'EOF'
{
  "intercept_scripts": [
    { "func_name": "connect",
      "actions": [{ "action_name": "modify_return_value_int",
        "action_params": { "retval_int": -111 } }] }
  ]
}
EOF
```

### Step 3 — run your client

```sh
retrace run --config /tmp/netfail.json -- ./your-client
# connect: Connection refused (os error 111)
```

Every outbound `connect()` returns -111 immediately. If your client
has retry or fallback logic, this is how you exercise it without
iptables or flaky WiFi.

---

## 10. Mock the system clock for time-sensitive tests

**Time:** 3 minutes.
**Goal:** freeze or shift time for testing expiry, schedules, TTLs.

### Step 1 — override time()

```sh
retrace mock time 1735689600 -- ./your-program
# (program now believes it's 2025-01-01 00:00:00 UTC)
```

### Step 2 — most programs also call gettimeofday() or clock_gettime()

Mock them too:

```sh
cat > /tmp/freeze.json <<'EOF'
{
  "intercept_scripts": [
    { "func_name": "time",      "actions": [{ "action_name": "modify_return_value_int", "action_params": { "retval_int": 1735689600 } }] },
    { "func_name": "gettimeofday", "actions": [{ "action_name": "call_real" }] }
  ]
}
EOF
retrace run --config /tmp/freeze.json -- ./your-program
```

Note: `clock_gettime` writes to a struct, so fully mocking it
requires a custom action. Mocking `time()` covers most cases.

### Step 3 — verify

Time-sensitive logic — token expiry, schedule triggers, rate-limit
windows — now behaves as if it's the mocked time.

---

## 11. Redirect a file path to somewhere else

**Time:** 3 minutes.
**Goal:** swap `/etc/config` for `/tmp/fake` without modifying the binary or root filesystem.

### Step 1 — write the redirect config

```sh
cat > /tmp/redirect.json <<'EOF'
{
  "intercept_scripts": [
    { "func_name": "open",
      "actions": [
        { "action_name": "modify_in_param_str",
          "action_params": { "param_name": "path", "match_str": "/etc/config", "new_str": "/tmp/fake-config" } },
        { "action_name": "call_real" }
      ] }
  ]
}
EOF
```

### Step 2 — prepare the fake file

```sh
echo 'debug = true' > /tmp/fake-config
```

### Step 3 — run

```sh
retrace run --config /tmp/redirect.json -- ./your-program
```

Every `open("/etc/config")` is transparently redirected to
`/tmp/fake-config`. No root, no chroot, no namespace.

This is also how you A/B test config files, swap certificates, or
feed known-bad inputs to parsers.

---

## 12. Audit what environment variables a binary reads

**Time:** 2 minutes.
**Goal:** map every getenv() call — catch secret leaks, config sniffing, debug backdoors.

### Step 1 — trace getenv

```sh
retrace trace getenv --log /tmp/env.json -- ./your-program
```

### Step 2 — pretty-print

```sh
retrace pp /tmp/env.json
```

```
getenv      42 calls
  getenv(name=PATH)
  getenv(name=HOME)
  getenv(name=DEBUG)         ← suspicious
  getenv(name=SECRET_TOKEN)  ← very suspicious
```

### Step 3 — triage

Anything sensitive in that list is a finding. Document it; report
it; or feed it garbage via `modify_in_param_str` to test handling.

Same pattern works for `execve` (what commands does it spawn?),
`system` (what shell does it run?), `dlopen` (what libraries does it
load at runtime?).

---

## 13. Capture all network traffic as JSON

**Time:** 3 minutes.
**Goal:** log every send/recv with payload — a pcap-style stream without tcpdump or root.

### Step 1 — trace network I/O

```sh
retrace trace send,sendto,recv,recvfrom --log /tmp/net.json -- ./your-server
```

### Step 2 — pretty-print

```sh
retrace pp /tmp/net.json | head -10
```

```
sendto    248 calls   2.1 MB total
recvfrom  247 calls   1.8 MB total
```

### Step 3 — interactive HTML view

```sh
retrace html /tmp/net.json -o /tmp/net.html && open /tmp/net.html
```

### Why this beats tcpdump sometimes

- No root required.
- Payloads are linked to the calling code.
- You can mix in malloc/open traces for full context.

---

## 14. Trace a statically-linked binary

**Time:** 8 minutes.
**Goal:** LD_PRELOAD doesn't work on static binaries. Use the ptrace backend instead.

### Step 1 — confirm it's static

```sh
file ./your-static-binary
# ./your-static-binary: ELF ... statically linked
```

If it has no INTERP segment, LD_PRELOAD can't intercept it.

### Step 2 — build retrace with ptrace backend

```sh
cmake -B build -DRETRACE_BACKEND_PTRACE=ON
cmake --build build
# build/src/backends/ptrace/libretrace_ptrace.so
```

### Step 3 — use the ptrace launcher

```sh
retrace ptrace --log /tmp/trace.json -- ./your-static-binary
```

The ptrace backend attaches to the target, sets breakpoints on libc
entry points, and reconstructs the calls.

### Caveat

ptrace is slower than LD_PRELOAD (each call is a context switch).
Reserve it for static binaries; use preload everywhere else.

See `src/backends/ptrace/README.md` for the full ptrace backend
reference.

---

## See also

- [Cookbook](cookbook/README.md) — 21 recipe-style walkthroughs.
- [CLI reference](cli.md) — every subcommand.
- [Configuration reference](configuration.md) — full JSON schema.
