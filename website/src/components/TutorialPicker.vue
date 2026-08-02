<script setup>
import { ref, computed } from "vue";

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
];

const selectedId = ref(scenarios[0].id);
const selected = computed(() =>
  scenarios.find((s) => s.id === selectedId.value)
);

function select(id) {
  selectedId.value = id;
}

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
      <div class="rail-label">Pick your scenario</div>
      <ul class="scenario-list">
        <li
          v-for="s in scenarios"
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
