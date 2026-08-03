<script setup>
import { ref, computed } from "vue";

// Pre-defined tutorial paths. Each is a curated sequence through the
// 22 scenarios in TutorialPicker.vue. Visitors who don't know where
// to start can pick a path that matches their role or goal; the path
// highlights which scenarios to do, in what order, with what rationale.

const paths = [
  {
    id: "beginner",
    title: "Beginner: first hour with retrace",
    icon: "🌱",
    accent: "see",
    summary: "Three tutorials that take you from zero to your first HTML trace report.",
    audience: "New to retrace? Start here.",
    stops: [
      { id: "source",    why: "Build retrace from source so you have the latest. (Or skip to step 2 if you curl-installed.)" },
      { id: "slow",      why: "Run your first trace, generate the HTML report, see the category breakdown." },
      { id: "oom",       why: "Now break something. Inject 10% OOM into malloc and watch your binary's error paths." },
    ],
  },
  {
    id: "security",
    title: "Security auditor",
    icon: "🛡️",
    accent: "break",
    summary: "Four tutorials that cover the audit workflows security researchers actually run.",
    audience: "For pentesters, vulnerability researchers, blue-team analysts.",
    stops: [
      { id: "sandbox",    why: "Deny-list sensitive paths before running an untrusted binary." },
      { id: "envaudit",   why: "Map every getenv() call. Catch secret leaks and debug backdoors." },
      { id: "auditexec",  why: "Find every system()/execve() call. CWE-78 (command injection)." },
      { id: "airgap",     why: "Confirm the binary makes zero outbound network connections." },
    ],
  },
  {
    id: "qa",
    title: "QA engineer",
    icon: "🧪",
    accent: "control",
    summary: "Three tutorials for shipping fault-injection into your CI pipeline.",
    audience: "For QA leads, test automation engineers, build/release engineers.",
    stops: [
      { id: "oom",      why: "Inject OOM locally first. Confirm your tests handle NULL returns." },
      { id: "ci",       why: "Drop the GitHub Actions workflow in. Every PR runs under fuzz." },
      { id: "build",    why: "Wire fuzz-test into your build system so it runs everywhere, not just CI." },
    ],
  },
  {
    id: "perf",
    title: "Performance investigator",
    icon: "⚡",
    accent: "see",
    summary: "Three tutorials for finding why your program is slow.",
    audience: "For backend devs, SREs, performance engineers.",
    stops: [
      { id: "slow",       why: "Trace with HTML output. The category breakdown shows your hot path." },
      { id: "flamegraph", why: "Generate an SVG flamegraph from the trace. Widest bars = biggest costs." },
      { id: "locks",      why: "Profile pthread_mutex_lock/unlock to find contention in threaded code." },
    ],
  },
  {
    id: "re",
    title: "Reverse engineer",
    icon: "🔍",
    accent: "control",
    summary: "Four tutorials for understanding a binary you didn't build.",
    audience: "For RE, malware analysts, CTF players.",
    stops: [
      { id: "reverse",  why: "Trace every libc call. Build a behavioral map without a disassembler." },
      { id: "traffic",  why: "Capture every send/recv as JSON. A pcap-style stream without tcpdump." },
      { id: "auditexec", why: "Find what the binary tries to spawn. Shell commands reveal intent." },
      { id: "static",   why: "Hit a static binary? Switch to the ptrace backend." },
    ],
  },
  {
    id: "extend",
    title: "Extend retrace",
    icon: "🛠️",
    accent: "control",
    summary: "Two tutorials for developers who want to push retrace further.",
    audience: "For library authors, engine developers, contributors.",
    stops: [
      { id: "custom",  why: "Write your own action in a single .c file. No engine change needed." },
      { id: "source",  why: "Build from source so you can iterate on the action you just wrote." },
    ],
  },
];

const activeId = ref(paths[0].id);
const active = computed(() => paths.find((p) => p.id === activeId.value));

function activate(id) {
  activeId.value = id;
}
</script>

<template>
  <div class="paths">
    <div class="path-chips">
      <button
        v-for="p in paths"
        :key="p.id"
        :class="['path-chip', `accent-${p.accent}`, { active: p.id === activeId }]"
        @click="activate(p.id)"
      >
        <span class="chip-icon">{{ p.icon }}</span>
        <span class="chip-text">{{ p.title }}</span>
      </button>
    </div>

    <div :class="['path-detail', 'glass', `accent-${active.accent}`]">
      <header class="path-head">
        <div class="path-eyebrow">
          <span class="path-icon">{{ active.icon }}</span>
          <span :class="['path-tag', `accent-${active.accent}`]">{{ active.title }}</span>
        </div>
        <p class="path-summary">{{ active.summary }}</p>
        <p class="path-audience">{{ active.audience }}</p>
      </header>

      <ol class="stops">
        <li v-for="(stop, i) in active.stops" :key="i" class="stop">
          <a class="stop-num" :href="`#tutorials`">{{ String(i + 1).padStart(2, "0") }}</a>
          <div class="stop-body">
            <a class="stop-target" :href="`#tutorials`">
              <span class="stop-arrow">→</span> tutorial: <code>{{ stop.id }}</code>
            </a>
            <p class="stop-why">{{ stop.why }}</p>
          </div>
        </li>
      </ol>
    </div>
  </div>
</template>

<style scoped>
.paths {
  display: flex;
  flex-direction: column;
  gap: 16px;
}

.path-chips {
  display: flex;
  flex-wrap: wrap;
  gap: 8px;
}
.path-chip {
  background: rgba(255, 255, 255, 0.03);
  border: 1px solid rgba(255, 255, 255, 0.08);
  color: var(--color-text);
  font-family: var(--font-mono);
  font-size: 12px;
  padding: 8px 14px;
  border-radius: 8px;
  cursor: pointer;
  transition: all 0.2s var(--ease-glass);
  display: inline-flex;
  align-items: center;
  gap: 8px;
}
.path-chip:hover {
  background: rgba(255, 255, 255, 0.06);
  transform: translateY(-1px);
}
.chip-icon { font-size: 14px; }
.chip-text { font-weight: 500; }

.path-chip.accent-see.active {
  background: rgba(94, 227, 255, 0.12);
  border-color: rgba(94, 227, 255, 0.4);
  color: var(--color-see);
}
.path-chip.accent-control.active {
  background: rgba(242, 180, 65, 0.12);
  border-color: rgba(242, 180, 65, 0.4);
  color: var(--color-control);
}
.path-chip.accent-break.active {
  background: rgba(255, 107, 92, 0.12);
  border-color: rgba(255, 107, 92, 0.4);
  color: var(--color-break);
}

.path-detail {
  padding: 28px 32px;
  border-top: 2px solid;
}
.path-detail.accent-see { border-top-color: var(--color-see); }
.path-detail.accent-control { border-top-color: var(--color-control); }
.path-detail.accent-break { border-top-color: var(--color-break); }

.path-head { margin-bottom: 22px; }
.path-eyebrow {
  display: flex;
  align-items: center;
  gap: 12px;
  margin-bottom: 12px;
}
.path-icon { font-size: 20px; }
.path-tag {
  font-family: var(--font-mono);
  font-size: 13px;
  font-weight: 600;
  letter-spacing: 0.12em;
  text-transform: uppercase;
}
.path-tag.accent-see { color: var(--color-see); }
.path-tag.accent-control { color: var(--color-control); }
.path-tag.accent-break { color: var(--color-break); }

.path-summary {
  font-size: 16px;
  color: var(--color-text);
  line-height: 1.45;
  margin-bottom: 6px;
  letter-spacing: -0.005em;
}
.path-audience {
  font-family: var(--font-mono);
  font-size: 11.5px;
  color: var(--color-dim);
  letter-spacing: 0.04em;
}

.stops {
  list-style: none;
  padding: 0;
  margin: 0;
  display: flex;
  flex-direction: column;
  gap: 16px;
}
.stop {
  display: grid;
  grid-template-columns: 40px 1fr;
  gap: 16px;
  position: relative;
}
.stop:not(:last-child)::before {
  content: "";
  position: absolute;
  left: 19px;
  top: 36px;
  bottom: -16px;
  width: 1px;
  background: linear-gradient(180deg, rgba(255, 255, 255, 0.1), rgba(255, 255, 255, 0));
}
.stop-num {
  width: 40px;
  height: 40px;
  border-radius: 50%;
  background: rgba(7, 9, 13, 0.7);
  border: 1px solid rgba(255, 255, 255, 0.1);
  display: flex;
  align-items: center;
  justify-content: center;
  font-family: var(--font-mono);
  font-size: 13px;
  font-weight: 600;
  color: var(--color-text);
  text-decoration: none;
  flex-shrink: 0;
  z-index: 1;
  transition: all 0.2s;
}
.stop-num:hover {
  border-color: rgba(94, 227, 255, 0.4);
  color: var(--color-see);
}
.stop-body {
  padding-top: 8px;
}
.stop-target {
  display: inline-block;
  font-family: var(--font-mono);
  font-size: 13px;
  color: var(--color-text);
  text-decoration: none;
  margin-bottom: 6px;
}
.stop-target:hover { color: var(--color-see); }
.stop-target .stop-arrow {
  color: var(--color-dim);
  margin-right: 6px;
}
.stop-target code {
  color: var(--color-see);
  background: rgba(94, 227, 255, 0.06);
  padding: 1px 6px;
  border-radius: 3px;
  border: 1px solid rgba(94, 227, 255, 0.18);
  font-size: 12px;
}
.stop-why {
  font-size: 13.5px;
  color: var(--color-dim);
  line-height: 1.55;
}

@media (max-width: 540px) {
  .path-detail { padding: 22px 20px; }
  .path-chip { font-size: 11px; padding: 6px 10px; }
  .chip-text { display: none; }
}
</style>
