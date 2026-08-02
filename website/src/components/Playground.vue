<script setup>
import { ref, nextTick, onBeforeUnmount } from "vue";

// Simulated retrace playground. Visitors click a command chip; the
// terminal types the command and then streams pre-defined realistic
// output. Not a real retrace — purely educational. Each output is
// hand-curated to match what retrace actually produces.

const PROMPT = "$ ";

const commands = [
  {
    id: "trace",
    label: "trace malloc",
    cmd: "retrace trace malloc,free -- /bin/ls",
    accent: "see",
    out: [
      { t: "MEM  ", c: "tag-see" },
      { t: "malloc", c: "fn" },
      { t: "(", c: "" },
      { t: "size=1024", c: "arg" },
      { t: ")", c: "" },
      { t: "  → ", c: "" },
      { t: "0x7f8e2a3b4000", c: "ret" },
      { t: "                                          <1µs", c: "dur" },
    ],
    more: [
      [{ t: "MEM  ", c: "tag-see" }, { t: "free", c: "fn" }, { t: "(", c: "" }, { t: "ptr=0x7f8e2a3b4000", c: "arg" }, { t: ")", c: "" }, { t: "                                          <1µs", c: "dur" }],
      [{ t: "MEM  ", c: "tag-see" }, { t: "malloc", c: "fn" }, { t: "(", c: "" }, { t: "size=512", c: "arg" }, { t: ")", c: "" }, { t: "    → ", c: "" }, { t: "0x7f8e2a3b4200", c: "ret" }, { t: "                                          <1µs", c: "dur" }],
      [{ t: "⋮ ", c: "c" }, { t: "(247 more calls)", c: "c" }],
      [{ t: "✓ ", c: "hdr" }, { t: "249 calls captured · 8.3ms total · 5 functions", c: "c" }],
    ],
  },
  {
    id: "html",
    label: "trace --html",
    cmd: "retrace trace malloc --html -- /bin/ls",
    accent: "control",
    out: [
      { t: "MEM  ", c: "tag-see" },
      { t: "malloc", c: "fn" },
      { t: "(", c: "" },
      { t: "size=1024", c: "arg" },
      { t: ")", c: "" },
      { t: "  → ", c: "" },
      { t: "0x7f8e2a3b4000", c: "ret" },
      { t: "                                          <1µs", c: "dur" },
    ],
    more: [
      [{ t: "⋮ ", c: "c" }, { t: "(generating HTML report as the trace streams…)", c: "c" }],
      [{ t: "✓ ", c: "hdr" }, { t: "wrote ", c: "c" }, { t: "/tmp/retrace-43892.html", c: "ret" }],
      [{ t: "  → ", c: "" }, { t: "open in your browser; no Python, no server, no CDN.", c: "c" }],
    ],
  },
  {
    id: "fuzz",
    label: "fuzz malloc",
    cmd: "retrace fuzz malloc --rate 0.3 -- ./server",
    accent: "break",
    out: [
      { t: "MEM  ", c: "tag-see" },
      { t: "malloc", c: "fn" },
      { t: "(", c: "" },
      { t: "size=2048", c: "arg" },
      { t: ")", c: "" },
      { t: "  → ", c: "" },
      { t: "0x7f8e2a3b4400", c: "ret" },
      { t: "                                           3µs", c: "dur" },
    ],
    more: [
      [{ t: "MEM  ", c: "tag-see" }, { t: "malloc", c: "fn" }, { t: "(", c: "" }, { t: "size=512", c: "arg" }, { t: ")", c: "" }, { t: "    → ", c: "" }, { t: "NULL", c: "tag-break" }, { t: "  ✗ injected (fail_rate=0.3)", c: "c" }],
      [{ t: "MEM  ", c: "tag-see" }, { t: "malloc", c: "fn" }, { t: "(", c: "" }, { t: "size=1024", c: "arg" }, { t: ")", c: "" }, { t: "  → ", c: "" }, { t: "0x7f8e2a3b4600", c: "ret" }, { t: "                                          <1µs", c: "dur" }],
      [{ t: "MEM  ", c: "tag-see" }, { t: "malloc", c: "fn" }, { t: "(", c: "" }, { t: "size=4096", c: "arg" }, { t: ")", c: "" }, { t: "  → ", c: "" }, { t: "NULL", c: "tag-break" }, { t: "  ✗ injected", c: "c" }],
      [{ t: "server: ", c: "fn" }, { t: "panic: nil pointer dereference (you found a bug)", c: "tag-break" }],
    ],
  },
  {
    id: "mock",
    label: "mock getuid",
    cmd: "retrace mock getuid 0 -- ./check-root",
    accent: "control",
    out: [
      { t: "welcome, root", c: "ret" },
    ],
    more: [
      [{ t: "(check-root believed getuid() returned ", c: "c" }, { t: "0", c: "tag-ctrl" }, { t: " and took the root code path)", c: "c" }],
    ],
  },
  {
    id: "actions",
    label: "list-actions",
    cmd: "retrace list-actions",
    accent: "see",
    out: [
      { t: "log_params", c: "fn" }, { t: "           observe · log args + return", c: "c" },
    ],
    more: [
      [{ t: "call_real", c: "fn" }, { t: "          observe · invoke real libc", c: "c" }],
      [{ t: "modify_in_param_str", c: "fn" }, { t: "  modify · rewrite string arg", c: "c" }],
      [{ t: "modify_in_param_int", c: "fn" }, { t: "  modify · rewrite integer arg", c: "c" }],
      [{ t: "modify_in_param_arr", c: "fn" }, { t: "  modify · rewrite byte buffer", c: "c" }],
      [{ t: "modify_return_value_int", c: "fn" }, { t: "modify · override return value", c: "c" }],
      [{ t: "memory_fuzz", c: "fn" }, { t: "        fault · random OOM injection", c: "c" }],
      [{ t: "incomplete_io", c: "fn" }, { t: "       fault · short read/write", c: "c" }],
      [{ t: "fuzzing_seed", c: "fn" }, { t: "        observe · pin RNG for reproducibility", c: "c" }],
      [{ t: "delay", c: "fn" }, { t: "                control · inject latency", c: "c" }],
      [{ t: "call_count_limit", c: "fn" }, { t: "    control · exhaust resource", c: "c" }],
      [{ t: "sandbox", c: "fn" }, { t: "              control · path deny-list", c: "c" }],
      [{ t: "(12 actions registered)", c: "c" }],
    ],
  },
  {
    id: "sandbox",
    label: "sandbox",
    cmd: 'retrace run --config sandbox.json -- ./untrusted',
    accent: "break",
    out: [
      { t: "I/O  ", c: "tag-ctrl" },
      { t: "open", c: "fn" },
      { t: "(", c: "" },
      { t: "path=\"/etc/shadow\"", c: "arg" },
      { t: ")", c: "" },
      { t: " → ", c: "" },
      { t: "-1 (ENOENT)", c: "tag-break" },
      { t: "  ✗ DENIED by sandbox", c: "c" },
    ],
    more: [
      [{ t: "I/O  ", c: "tag-ctrl" }, { t: "open", c: "fn" }, { t: "(", c: "" }, { t: "path=\"/etc/passwd\"", c: "arg" }, { t: ")", c: "" }, { t: " → ", c: "" }, { t: "3", c: "ret" }, { t: "   (allowed)", c: "c" }],
      [{ t: "I/O  ", c: "tag-ctrl" }, { t: "open", c: "fn" }, { t: "(", c: "" }, { t: "path=\"/root/.ssh/id_rsa\"", c: "arg" }, { t: ")", c: "" }, { t: " → ", c: "" }, { t: "-1 (ENOENT)", c: "tag-break" }, { t: "  ✗ DENIED", c: "c" }],
      [{ t: "untrusted: ", c: "fn" }, { t: "error: cannot read credentials (good)", c: "tag-break" }],
    ],
  },
];

const terminal = ref(null);
const currentCmdId = ref(null);
const isRunning = ref(false);
let timers = [];

function clearTimers() {
  timers.forEach((id) => clearTimeout(id));
  timers = [];
}

function appendLine(parts) {
  const root = terminal.value;
  if (!root) return;
  const line = document.createElement("div");
  line.className = "p-line";
  if (typeof parts === "string") {
    line.textContent = parts;
  } else {
    for (const p of parts) {
      const span = document.createElement("span");
      if (p.c) span.className = p.c;
      span.textContent = p.t;
      line.appendChild(span);
    }
  }
  root.appendChild(line);
  root.scrollTop = root.scrollHeight;
}

function appendBlank() {
  appendLine("");
}

async function typeLine(parts, done) {
  const root = terminal.value;
  if (!root) {
    done();
    return;
  }
  const line = document.createElement("div");
  line.className = "p-line";
  root.appendChild(line);

  // Render all spans instantly except the last; animate the last
  // one char by char.
  const lastIdx = parts.length - 1;
  for (let i = 0; i < lastIdx; i++) {
    const span = document.createElement("span");
    if (parts[i].c) span.className = parts[i].c;
    span.textContent = parts[i].t;
    line.appendChild(span);
  }
  const last = parts[lastIdx];
  const lastSpan = document.createElement("span");
  if (last.c) lastSpan.className = last.c;
  lastSpan.textContent = "";
  line.appendChild(lastSpan);

  let ci = 0;
  function step() {
    if (ci <= last.t.length) {
      lastSpan.textContent = last.t.slice(0, ci);
      ci++;
      root.scrollTop = root.scrollHeight;
      timers.push(setTimeout(step, 18));
    } else {
      done();
    }
  }
  step();
}

async function runCommand(cmd) {
  if (isRunning.value) return;
  isRunning.value = true;
  currentCmdId.value = cmd.id;
  clearTimers();

  const root = terminal.value;
  if (root) root.innerHTML = "";

  // Type the command line.
  await new Promise((resolve) => {
    typeLine(
      [
        { t: PROMPT, c: "p" },
        { t: cmd.cmd, c: "fn" },
      ],
      resolve
    );
  });

  await new Promise((r) => timers.push(setTimeout(r, 180)));

  // First output line, typed.
  if (cmd.out) {
    await new Promise((r) => typeLine(cmd.out, r));
    await new Promise((r) => timers.push(setTimeout(r, 120)));
  }

  // Subsequent lines, faster.
  if (cmd.more) {
    for (const line of cmd.more) {
      appendLine(line);
      root.scrollTop = root.scrollHeight;
      await new Promise((r) => timers.push(setTimeout(r, 90)));
    }
  }

  // Final cursor line.
  appendLine([{ t: PROMPT, c: "p" }, { t: "▋", c: "cursor" }]);

  isRunning.value = false;
}

async function onClickCommand(cmd) {
  await runCommand(cmd);
}

onBeforeUnmount(clearTimers);

// Auto-run the first command on mount so the playground isn't empty.
import { onMounted } from "vue";
onMounted(async () => {
  await nextTick();
  runCommand(commands[0]);
});
</script>

<template>
  <div class="playground">
    <div class="cmd-bar">
      <div class="cmd-bar-label">Try a command:</div>
      <div class="cmd-chips">
        <button
          v-for="cmd in commands"
          :key="cmd.id"
          :class="['cmd-chip', `accent-${cmd.accent}`, { active: currentCmdId === cmd.id && !isRunning }]"
          :disabled="isRunning"
          @click="onClickCommand(cmd)"
        >
          <span class="chip-glyph">{{ cmd.accent === "see" ? "▸" : cmd.accent === "control" ? "▸" : "▸" }}</span>
          {{ cmd.label }}
        </button>
      </div>
      <p class="playground-note">
        Simulated. Output mirrors what the real retrace binary produces — try the
        <a href="#install">install</a> to run it on your own binary.
      </p>
    </div>

    <div class="term-glass">
      <div class="term-bar">
        <div class="term-dots"><span></span><span></span><span></span></div>
        <div class="term-title">~/playground — retrace</div>
      </div>
      <div ref="terminal" class="term-body"></div>
    </div>
  </div>
</template>

<style scoped>
.playground {
  display: flex;
  flex-direction: column;
  gap: 16px;
}

.cmd-bar {
  background: rgba(11, 13, 18, 0.4);
  backdrop-filter: blur(14px) saturate(160%);
  -webkit-backdrop-filter: blur(14px) saturate(160%);
  border: 1px solid rgba(255, 255, 255, 0.05);
  border-radius: 12px;
  padding: 14px 18px;
}
.cmd-bar-label {
  font-family: var(--font-mono);
  font-size: 10px;
  letter-spacing: 0.18em;
  text-transform: uppercase;
  color: var(--color-dim);
  margin-bottom: 10px;
}
.cmd-chips {
  display: flex;
  flex-wrap: wrap;
  gap: 6px;
}
.cmd-chip {
  background: rgba(255, 255, 255, 0.03);
  border: 1px solid rgba(255, 255, 255, 0.08);
  color: var(--color-text);
  font-family: var(--font-mono);
  font-size: 12px;
  padding: 6px 12px;
  border-radius: 6px;
  cursor: pointer;
  transition: all 0.2s var(--ease-glass);
  display: inline-flex;
  align-items: center;
  gap: 6px;
}
.cmd-chip:disabled {
  opacity: 0.5;
  cursor: not-allowed;
}
.cmd-chip:not(:disabled):hover {
  background: rgba(255, 255, 255, 0.07);
  transform: translateY(-1px);
}
.chip-glyph {
  font-size: 10px;
  opacity: 0.7;
}
.cmd-chip.accent-see.active {
  background: rgba(94, 227, 255, 0.12);
  border-color: rgba(94, 227, 255, 0.4);
  color: var(--color-see);
}
.cmd-chip.accent-control.active {
  background: rgba(242, 180, 65, 0.12);
  border-color: rgba(242, 180, 65, 0.4);
  color: var(--color-control);
}
.cmd-chip.accent-break.active {
  background: rgba(255, 107, 92, 0.12);
  border-color: rgba(255, 107, 92, 0.4);
  color: var(--color-break);
}
.playground-note {
  margin-top: 12px;
  font-size: 12px;
  color: var(--color-dim);
  font-family: var(--font-mono);
}
.playground-note a {
  color: var(--color-see);
  border-bottom: 1px solid rgba(94, 227, 255, 0.3);
}

.term-glass {
  background: linear-gradient(180deg, rgba(11, 13, 18, 0.78) 0%, rgba(7, 9, 13, 0.72) 100%);
  backdrop-filter: blur(28px) saturate(180%);
  -webkit-backdrop-filter: blur(28px) saturate(180%);
  border: 1px solid rgba(255, 255, 255, 0.07);
  border-radius: 14px;
  overflow: hidden;
  position: relative;
  box-shadow:
    inset 0 1px 0 rgba(255, 255, 255, 0.08),
    0 30px 80px -28px rgba(0, 0, 0, 0.75),
    0 0 0 1px rgba(94, 227, 255, 0.03);
}
.term-bar {
  display: flex;
  align-items: center;
  gap: 8px;
  padding: 10px 14px;
  border-bottom: 1px solid rgba(255, 255, 255, 0.05);
  background: rgba(255, 255, 255, 0.015);
}
.term-dots {
  display: flex;
  gap: 6px;
}
.term-dots span {
  width: 10px;
  height: 10px;
  border-radius: 50%;
  background: rgba(255, 255, 255, 0.08);
}
.term-title {
  margin-left: auto;
  font-family: var(--font-mono);
  font-size: 11px;
  color: var(--color-dim);
  letter-spacing: 0.02em;
}
.term-body {
  font-family: var(--font-mono);
  font-size: 12.5px;
  line-height: 1.7;
  padding: 18px 22px;
  height: 360px;
  overflow-y: auto;
  position: relative;
}
.term-body :deep(.p-line) {
  white-space: pre;
}
.term-body :deep(.p) { color: var(--color-break); }
.term-body :deep(.c) { color: var(--color-dim); }
.term-body :deep(.fn) { color: var(--color-text); font-weight: 500; }
.term-body :deep(.arg) { color: var(--color-see); }
.term-body :deep(.ret) { color: var(--color-control); }
.term-body :deep(.dur) { color: var(--color-dim); }
.term-body :deep(.hdr) { color: var(--color-see); }
.term-body :deep(.tag-see) { color: var(--color-see); }
.term-body :deep(.tag-ctrl) { color: var(--color-control); }
.term-body :deep(.tag-break) { color: var(--color-break); }
.term-body :deep(.cursor) {
  display: inline-block;
  color: var(--color-text);
  animation: blink 1s steps(2) infinite;
}
@keyframes blink {
  50% { opacity: 0; }
}
</style>
