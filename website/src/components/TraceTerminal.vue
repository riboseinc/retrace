<script setup>
import { ref, onMounted, onBeforeUnmount } from "vue";

const body = ref(null);
const reduceMotion = ref(false);
let timers = [];

// Each entry renders as one line in the terminal. `parts` is an array
// of { t: text, c: class } spans. `type: true` animates the last span
// character by character.
const lines = [
  {
    type: true,
    pause: 500,
    parts: [
      { t: "$ ", c: "p" },
      { t: "retrace trace malloc,free,open,read -- /bin/ls", c: "fn" },
    ],
  },
  {
    pause: 260,
    parts: [{ t: "1,247 calls captured · 48.3ms total · 42 functions", c: "hdr" }],
  },
  {
    pause: 90,
    parts: [
      { t: "MEM  ", c: "tag-see" },
      { t: "malloc", c: "fn" },
      { t: "(", c: "" },
      { t: "size=1024", c: "arg" },
      { t: ")", c: "" },
      { t: "  → ", c: "" },
      { t: "0x7f8e2a3b4000", c: "ret" },
      { t: "                                          <1µs", c: "dur" },
    ],
  },
  {
    pause: 70,
    parts: [
      { t: "MEM  ", c: "tag-see" },
      { t: "free", c: "fn" },
      { t: "(", c: "" },
      { t: "ptr=0x7f8e2a3b4000", c: "arg" },
      { t: ")", c: "" },
      { t: "                                          <1µs", c: "dur" },
    ],
  },
  {
    pause: 70,
    parts: [
      { t: "I/O  ", c: "tag-ctrl" },
      { t: "open", c: "fn" },
      { t: "(", c: "" },
      { t: "path=\"/etc/passwd\", flags=O_RDONLY", c: "arg" },
      { t: ")", c: "" },
      { t: "  → ", c: "" },
      { t: "3", c: "ret" },
      { t: "                                          12µs", c: "dur" },
    ],
  },
  {
    pause: 70,
    parts: [
      { t: "I/O  ", c: "tag-ctrl" },
      { t: "read", c: "fn" },
      { t: "(", c: "" },
      { t: "fd=3, count=4096", c: "arg" },
      { t: ")", c: "" },
      { t: "  → ", c: "" },
      { t: "4096", c: "ret" },
      { t: "                                           8µs", c: "dur" },
    ],
  },
  {
    pause: 70,
    parts: [
      { t: "I/O  ", c: "tag-ctrl" },
      { t: "write", c: "fn" },
      { t: "(", c: "" },
      { t: "fd=1, count=8192", c: "arg" },
      { t: ")", c: "" },
      { t: " → ", c: "" },
      { t: "8192", c: "ret" },
      { t: "                                          11µs", c: "dur" },
    ],
  },
  {
    pause: 70,
    parts: [
      { t: "MEM  ", c: "tag-see" },
      { t: "malloc", c: "fn" },
      { t: "(", c: "" },
      { t: "size=512", c: "arg" },
      { t: ")", c: "" },
      { t: "    → ", c: "" },
      { t: "0x7f8e2a3b4200", c: "ret" },
      { t: "                                          <1µs", c: "dur" },
    ],
  },
  {
    pause: 70,
    parts: [
      { t: "EXEC ", c: "tag-break" },
      { t: "ioctl", c: "fn" },
      { t: "(", c: "" },
      { t: "fd=1, req=TIOCGWINSZ", c: "arg" },
      { t: ")", c: "" },
      { t: "      → ", c: "" },
      { t: "0", c: "ret" },
      { t: "                                           4µs", c: "dur" },
    ],
  },
  {
    pause: 70,
    parts: [
      { t: "I/O  ", c: "tag-ctrl" },
      { t: "close", c: "fn" },
      { t: "(", c: "" },
      { t: "fd=3", c: "arg" },
      { t: ")", c: "" },
      { t: "  → ", c: "" },
      { t: "0", c: "ret" },
      { t: "                                          <1µs", c: "dur" },
    ],
  },
  {
    pause: 130,
    parts: [
      { t: "⋮ ", c: "c" },
      { t: "(1,240 more calls)", c: "c" },
    ],
  },
  {
    pause: 320,
    parts: [
      { t: "✓ ", c: "hdr" },
      { t: "wrote /tmp/retrace-43892.html — ", c: "c" },
      { t: "open in browser", c: "ret" },
    ],
  },
];

function clearTimers() {
  timers.forEach((id) => clearTimeout(id));
  timers = [];
}

function paintAll() {
  const root = body.value;
  if (!root) return;
  root.innerHTML = "";
  for (const ln of lines) {
    const line = document.createElement("div");
    line.className = "t-line";
    for (const part of ln.parts) {
      const span = document.createElement("span");
      if (part.c) span.className = part.c;
      span.textContent = part.t;
      line.appendChild(span);
    }
    root.appendChild(line);
  }
  const last = document.createElement("div");
  last.className = "t-line";
  const cursor = document.createElement("span");
  cursor.className = "cursor";
  last.appendChild(cursor);
  root.appendChild(last);
}

function makeLine(parts) {
  const line = document.createElement("div");
  line.className = "t-line";
  for (const part of parts) {
    const span = document.createElement("span");
    if (part.c) span.className = part.c;
    span.textContent = part.t;
    line.appendChild(span);
  }
  return line;
}

function typeLine(parts, done) {
  const root = body.value;
  if (!root) {
    done();
    return;
  }
  const line = makeLine(parts);
  root.appendChild(line);

  const spans = line.querySelectorAll("span");
  const lastIdx = parts.length - 1;
  const lastSpan = spans[lastIdx];
  const fullText = lastSpan.textContent;
  lastSpan.textContent = "";
  let ci = 0;

  function step() {
    if (ci <= fullText.length) {
      lastSpan.textContent = fullText.slice(0, ci);
      ci++;
      timers.push(setTimeout(step, 32));
    } else {
      done();
    }
  }
  step();
}

function playLine(i) {
  if (i >= lines.length) {
    const root = body.value;
    if (root) {
      const last = document.createElement("div");
      last.className = "t-line";
      const cursor = document.createElement("span");
      cursor.className = "cursor";
      last.appendChild(cursor);
      root.appendChild(last);
    }
    return;
  }
  const ln = lines[i];
  timers.push(
    setTimeout(() => {
      const root = body.value;
      if (!root) return;
      if (root.children.length > 7) {
        root.removeChild(root.firstChild);
      }
      if (ln.type) {
        typeLine(ln.parts, () => playLine(i + 1));
      } else {
        root.appendChild(makeLine(ln.parts));
        playLine(i + 1);
      }
    }, ln.pause || 80)
  );
}

onMounted(() => {
  reduceMotion.value =
    window.matchMedia &&
    window.matchMedia("(prefers-reduced-motion: reduce)").matches;
  if (reduceMotion.value) {
    paintAll();
  } else {
    body.value.innerHTML = "";
    playLine(0);
  }
});

onBeforeUnmount(clearTimers);
</script>

<template>
  <div class="term-glass" aria-label="Animated retrace trace example">
    <div class="term-bar">
      <div class="term-dots"><span></span><span></span><span></span></div>
      <div class="term-title">~/proj — retrace trace</div>
    </div>
    <div ref="body" class="term-body"></div>
  </div>
</template>

<style scoped>
.term-glass {
  background: linear-gradient(
    180deg,
    rgba(11, 13, 18, 0.78) 0%,
    rgba(7, 9, 13, 0.72) 100%
  );
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
.term-glass::before {
  content: "";
  position: absolute;
  inset: 0;
  border-radius: inherit;
  padding: 1px;
  background: linear-gradient(
    180deg,
    rgba(94, 227, 255, 0.22) 0%,
    rgba(255, 255, 255, 0.04) 40%,
    rgba(255, 255, 255, 0) 70%
  );
  -webkit-mask:
    linear-gradient(#000 0 0) content-box,
    linear-gradient(#000 0 0);
  -webkit-mask-composite: xor;
  mask-composite: exclude;
  pointer-events: none;
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
  height: 372px;
  overflow: hidden;
  position: relative;
}
.term-body :deep(.t-line) {
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
  width: 8px;
  height: 14px;
  background: var(--color-text);
  vertical-align: text-bottom;
  margin-left: 2px;
  animation: blink 1s steps(2) infinite;
}
@keyframes blink {
  50% { opacity: 0; }
}
</style>
