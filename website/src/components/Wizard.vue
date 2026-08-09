---
/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Decision Wizard (TODO.complete/36 P0).
 *
 * A multi-step Vue wizard that asks:
 *   1. What do you want to do? (5 top-level goals)
 *   2. More specifically? (sub-options per goal)
 *   3. What's your target? (function name or binary path)
 *   4. Result: recommended CLI command + JSON config snippet
 *
 * Deterministic state machine. Any valid path produces a
 * runnable command + a copy-paste JSON config that the
 * existing ConfigValidator accepts.
 */
---
<script setup>
import { ref, computed } from "vue";

// ----- Step 1: top-level goal -----
const GOALS = [
  { id: "debug",       label: "Debug a problem",       blurb: "Trace what libc calls a binary makes" },
  { id: "test",        label: "Test for robustness",   blurb: "Inject OOM, short I/O, latency" },
  { id: "audit",       label: "Audit for security",    blurb: "Find unsafe calls, sensitive file access" },
  { id: "reverse",     label: "Reverse engineer",      blurb: "Understand an unknown binary's behavior" },
  { id: "extend",      label: "Extend retrace",        blurb: "Write a custom action or backend" },
];

// ----- Step 2: sub-options per goal -----
const SUBOPTIONS = {
  debug: [
    { id: "trace-all",  label: "Trace every libc call",   action: "log_params+call_real", scope: "*" },
    { id: "slow",       label: "Find slow calls",         action: "log_params+call_real", scope: "*" },
    { id: "filter",     label: "Trace specific functions", action: "log_params+call_real", scope: "user-pick" },
  ],
  test: [
    { id: "oom",        label: "Fail malloc randomly",    action: "memory_fuzz",          scope: "malloc" },
    { id: "short-io",   label: "Partial read/write",      action: "incomplete_io",        scope: "*" },
    { id: "delay",      label: "Inject latency",          action: "delay",                scope: "user-pick" },
  ],
  audit: [
    { id: "system",     label: "Find system() calls",     action: "log_params+call_real", scope: "system" },
    { id: "open",       label: "Find sensitive file opens", action: "log_params+call_real", scope: "open" },
    { id: "net",        label: "Find outbound connects",  action: "log_params+call_real", scope: "connect" },
  ],
  reverse: [
    { id: "full",       label: "Full trace, no actions",  action: "log_params+call_real", scope: "*" },
    { id: "net",        label: "Network only",            action: "log_params+call_real", scope: "send,recv,connect,sendto,recvfrom" },
  ],
  extend: [
    { id: "actions",    label: "Write a new action",      action: "(see docs)",           scope: "" },
    { id: "backends",   label: "Write a new backend",     action: "(see docs)",           scope: "" },
  ],
};

// ----- Wizard state -----
const step = ref(1);
const goal = ref("");
const sub  = ref("");
const target = ref("./your-binary");
const funcs = ref("malloc,free");

// ----- Reset handlers -----
function pickGoal(g) {
  goal.value = g;
  sub.value = "";
  step.value = 2;
}

function pickSub(s) {
  sub.value = s;
  step.value = 3;
}

function back() {
  if (step.value > 1) step.value--;
}

function restart() {
  step.value = 1;
  goal.value = "";
  sub.value = "";
  target.value = "./your-binary";
  funcs.value = "malloc,free";
}

// ----- Result builders -----
const chosenGoal = computed(() => GOALS.find((g) => g.id === goal.value));
const chosenSub  = computed(() => SUBOPTIONS[goal.value]?.find((s) => s.id === sub.value));
const isExtend   = computed(() => goal.value === "extend");

function funcsArray() {
  if (!chosenSub.value) return [];
  if (chosenSub.value.scope === "*") return ["*"];
  if (chosenSub.value.scope === "user-pick")
    return funcs.value.split(",").map((s) => s.trim()).filter(Boolean);
  return chosenSub.value.scope.split(",");
}

function actionsArray() {
  if (!chosenSub.value) return [];
  return chosenSub.value.action.split("+");
}

const jsonConfig = computed(() => {
  if (isExtend.value) return "";
  const fns = funcsArray();
  const acts = actionsArray();
  const scripts = fns.map((f) => ({
    func_name: f,
    actions: acts.map((a) => ({ action_name: a })),
  }));
  return JSON.stringify({ intercept_scripts: scripts }, null, 2);
});

const cliCommand = computed(() => {
  if (isExtend.value) return "# See docs/extend/ for action and backend authoring guides";
  return `retrace run --config /tmp/wizard-config.json -- ${target.value}`;
});

const tutorialLink = computed(() => {
  const map = {
    debug:   "/#tutorials",
    test:    "/#tutorials",
    audit:   "/#tutorials",
    reverse: "/#tutorials",
    extend:  "/#community",
  };
  return map[goal.value] ?? "/";
});

const recipeNumber = computed(() => {
  const r = {
    "debug.trace-all": "01",
    "debug.slow":      "04",
    "debug.filter":    "02",
    "test.oom":        "09",
    "test.short-io":   "10",
    "test.delay":      "08",
    "audit.system":    "13",
    "audit.open":      "06",
    "audit.net":       "15",
    "reverse.full":    "01",
    "reverse.net":     "15",
    "extend.actions":  null,
    "extend.backends": null,
  };
  const k = `${goal.value}.${sub.value}`;
  return r[k];
});

// ----- Copy to clipboard -----
const copied = ref(false);
async function copyJSON() {
  try {
    await navigator.clipboard.writeText(jsonConfig.value);
    copied.value = true;
    setTimeout(() => { copied.value = false; }, 1500);
  } catch (e) {
    /* clipboard API not available */
  }
}
</script>

<template>
  <div class="wizard">
    <div class="wizard-progress">
      <span :class="{ active: step >= 1 }">1. Goal</span>
      <span :class="{ active: step >= 2 }">2. Sub-goal</span>
      <span :class="{ active: step >= 3 }">3. Target</span>
      <span :class="{ active: step >= 4 }">4. Result</span>
    </div>

    <!-- Step 1 -->
    <div v-if="step === 1" class="step">
      <h3>What do you want to do?</h3>
      <div class="grid">
        <button v-for="g in GOALS" :key="g.id" class="card" @click="pickGoal(g.id)">
          <strong>{{ g.label }}</strong>
          <small>{{ g.blurb }}</small>
        </button>
      </div>
    </div>

    <!-- Step 2 -->
    <div v-else-if="step === 2" class="step">
      <h3>More specifically?</h3>
      <div class="grid">
        <button
          v-for="s in SUBOPTIONS[goal]"
          :key="s.id"
          class="card"
          @click="pickSub(s.id)"
        >
          <strong>{{ s.label }}</strong>
          <small>actions: {{ s.action }}</small>
        </button>
      </div>
      <button class="ghost" @click="back">Back</button>
    </div>

    <!-- Step 3 -->
    <div v-else-if="step === 3" class="step">
      <h3>What's your target?</h3>
      <label>
        Binary path + args:
        <input v-model="target" placeholder="./your-binary --flag" />
      </label>
      <label v-if="chosenSub?.scope === 'user-pick'">
        Functions to intercept (comma-separated):
        <input v-model="funcs" placeholder="malloc,free,open" />
      </label>
      <div class="row">
        <button class="ghost" @click="back">Back</button>
        <button class="primary" @click="step = 4">See result</button>
      </div>
    </div>

    <!-- Step 4: Result -->
    <div v-else class="step result">
      <h3>Recommended approach</h3>
      <p class="summary">
        Goal: <strong>{{ chosenGoal?.label }}</strong> &middot;
        Strategy: <strong>{{ chosenSub?.label }}</strong>
      </p>

      <div v-if="isExtend">
        <p>Extending retrace means writing either a new action (C) or a new backend (per-arch). See:</p>
        <ul>
          <li><code>src/core/actions/</code> for the action API</li>
          <li><code>src/backends/</code> for the backend plugin API</li>
          <li><a href="#community">Community</a> for discussion</li>
        </ul>
      </div>

      <div v-else>
        <h4>1. Save this config to <code>/tmp/wizard-config.json</code></h4>
        <pre class="json-block"><code>{{ jsonConfig }}</code></pre>
        <button class="ghost" @click="copyJSON">
          {{ copied ? "Copied!" : "Copy JSON" }}
        </button>

        <h4>2. Run the command</h4>
        <pre class="cmd-block"><code>{{ cliCommand }}</code></pre>

        <h4>3. Read more</h4>
        <p>
          <a :href="tutorialLink">Tutorial</a>
          <span v-if="recipeNumber">
            &middot; <a :href="`/docs/cookbook/${recipeNumber}-`">Cookbook recipe {{ recipeNumber }}</a>
          </span>
        </p>
      </div>

      <button class="ghost restart" @click="restart">Start over</button>
    </div>
  </div>
</template>

<style scoped>
.wizard {
  border: 1px solid var(--border, #ddd);
  border-radius: 8px;
  padding: 24px;
  background: var(--surface, #fafafa);
}

.wizard-progress {
  display: flex;
  gap: 12px;
  font-size: 12px;
  margin-bottom: 24px;
  text-transform: uppercase;
  letter-spacing: 0.5px;
}
.wizard-progress span {
  color: var(--muted, #888);
}
.wizard-progress span.active {
  color: var(--text, #222);
  font-weight: 600;
}

.step h3 { margin-top: 0; }
.step h4 { margin: 20px 0 8px; font-size: 14px; }

.grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(220px, 1fr));
  gap: 12px;
  margin-bottom: 16px;
}

.card {
  display: flex;
  flex-direction: column;
  gap: 4px;
  padding: 12px 14px;
  text-align: left;
  background: var(--surface-elev, #fff);
  border: 1px solid var(--border, #ddd);
  border-radius: 6px;
  cursor: pointer;
  transition: border-color 0.15s, transform 0.05s;
}
.card:hover { border-color: var(--accent, #6ed6ff); }
.card:active { transform: translateY(1px); }
.card strong { font-size: 14px; }
.card small { color: var(--muted, #888); font-size: 12px; }

label {
  display: block;
  margin-bottom: 12px;
  font-size: 13px;
}
input {
  display: block;
  width: 100%;
  padding: 8px 10px;
  margin-top: 4px;
  font-family: var(--mono, monospace);
  font-size: 13px;
  border: 1px solid var(--border, #ddd);
  border-radius: 4px;
  background: var(--surface-elev, #fff);
  color: var(--text, #222);
  box-sizing: border-box;
}

.row { display: flex; gap: 8px; }

button.primary, button.ghost {
  padding: 8px 14px;
  border-radius: 4px;
  font-size: 13px;
  cursor: pointer;
}
button.primary {
  background: var(--accent, #2c3e50);
  color: #fff;
  border: 1px solid var(--accent, #2c3e50);
}
button.ghost {
  background: transparent;
  color: var(--text, #222);
  border: 1px solid var(--border, #ddd);
}
button.ghost:hover { background: var(--surface-elev, #eee); }
.restart { margin-top: 16px; }

.json-block, .cmd-block {
  background: var(--code-bg, #1a1a1a);
  color: var(--code-fg, #e8e8e8);
  padding: 12px 14px;
  border-radius: 4px;
  overflow-x: auto;
  font-size: 12px;
  line-height: 1.45;
}

.summary { color: var(--muted, #666); font-size: 14px; }

a { color: var(--accent, #2c3e50); }
code { font-family: var(--mono, monospace); }
</style>
