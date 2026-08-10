---
/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Recipe Builder (TODO.complete/36 P1).
 *
 * Three-pane Vue island:
 *   - Left: function picker (categorised shortcuts + free-text)
 *   - Middle: action chain composer (drag actions from palette,
 *     configure params inline)
 *   - Right: live JSON output that flows into the existing
 *     ConfigValidator
 *
 * Drag-and-drop via native HTML5 DnD (no library). The action
 * palette is a fixed list of retrace's built-in actions.
 */
---
<script setup>
import { ref, computed } from "vue";

// Action palette. Each entry has the param schema so the
// composer can render the right input type per param.
const ACTIONS = [
  { name: "log_params",     desc: "Log call args + retval", params: [] },
  { name: "call_real",      desc: "Invoke real libc impl",  params: [] },
  { name: "memory_fuzz",    desc: "Fail malloc randomly",   params: [
    { key: "fail_rate", type: "number", default: 0.05, min: 0, max: 1, step: 0.01 },
  ]},
  { name: "incomplete_io",  desc: "Partial read/write",     params: [
    { key: "fail_rate", type: "number", default: 0.05, min: 0, max: 1, step: 0.01 },
  ]},
  { name: "delay",          desc: "Inject latency",         params: [
    { key: "ms", type: "number", default: 100, min: 0, max: 60000, step: 1 },
  ]},
  { name: "fuzzing_seed",   desc: "Pin RNG seed",           params: [
    { key: "seed", type: "number", default: 42, min: 0, max: 4294967295, step: 1 },
  ]},
  { name: "call_count_limit", desc: "Fail after N calls",   params: [
    { key: "limit", type: "number", default: 100, min: 1, max: 1000000, step: 1 },
  ]},
  { name: "modify_in_param_str", desc: "Rewrite string arg", params: [
    { key: "param_name", type: "text", default: "buf" },
    { key: "new_value",  type: "text", default: "" },
  ]},
  { name: "modify_in_param_int", desc: "Rewrite int arg",    params: [
    { key: "param_name", type: "text",   default: "flags" },
    { key: "new_value",  type: "number", default: 0 },
  ]},
  { name: "modify_return_value_int", desc: "Override retval", params: [
    { key: "retval_int", type: "number", default: 0 },
  ]},
  { name: "filter", desc: "Gate next actions on a param",    params: [
    { key: "param_name", type: "text",   default: "ret_val" },
    { key: "op",         type: "select", default: "==", options: ["==", "!=", "<", "<=", ">", ">="] },
    { key: "value",      type: "number", default: 0 },
  ]},
  { name: "decode_http", desc: "Parse buf as HTTP",          params: [
    { key: "param_name", type: "text", default: "buf" },
  ]},
  { name: "decode_dns",  desc: "Parse buf as DNS",           params: [
    { key: "param_name", type: "text", default: "buf" },
  ]},
  { name: "sandbox",     desc: "Path deny-list",             params: [
    { key: "deny_paths", type: "text", default: "/etc/shadow,/root/.ssh/" },
  ]},
];

// Per-script state. One Recipe Builder produces one script.
// Multi-script configs would need a list of these (TODO P2).
const funcName = ref("malloc");
const actionChain = ref([
  { name: "log_params", params: {} },
  { name: "call_real",  params: {} },
]);

// DnD state
const draggedAction = ref(null);

function onDragStart(action) {
  draggedAction.value = action;
}
function onDragOver(ev) {
  // Necessary to allow drop.
  ev.preventDefault();
}
function onDrop() {
  if (draggedAction.value) {
    // Add to chain with default param values.
    const params = {};
    for (const p of draggedAction.value.params) {
      params[p.key] = p.default;
    }
    actionChain.value.push({
      name: draggedAction.value.name,
      params: { ...params },
    });
    draggedAction.value = null;
  }
}

function moveUp(idx) {
  if (idx > 0) {
    const chain = actionChain.value;
    [chain[idx - 1], chain[idx]] = [chain[idx], chain[idx - 1]];
  }
}
function moveDown(idx) {
  if (idx < actionChain.value.length - 1) {
    const chain = actionChain.value;
    [chain[idx], chain[idx + 1]] = [chain[idx + 1], chain[idx]];
  }
}
function remove(idx) {
  actionChain.value.splice(idx, 1);
}

function actionDef(name) {
  return ACTIONS.find((a) => a.name === name);
}

const jsonConfig = computed(() => {
  const actions = actionChain.value.map((a) => {
    const def = actionDef(a.name);
    if (!def || def.params.length === 0) {
      return { action_name: a.name };
    }
    return { action_name: a.name, action_params: { ...a.params } };
  });
  return JSON.stringify({
    intercept_scripts: [{ func_name: funcName.value, actions }],
  }, null, 2);
});

const copied = ref(false);
async function copyJSON() {
  try {
    await navigator.clipboard.writeText(jsonConfig.value);
    copied.value = true;
    setTimeout(() => { copied.value = false; }, 1500);
  } catch (e) {}
}

const VALIDATOR_URL = "#config-validator";
</script>

<template>
  <div class="rb">
    <!-- Left: function picker -->
    <div class="pane">
      <h4>Function</h4>
      <input v-model="funcName" placeholder="malloc / free / open / * / ..." />
      <p class="hint">
        Use <code>*</code> for wildcard. Common:
        <button class="chip" @click="funcName = 'malloc'">malloc</button>
        <button class="chip" @click="funcName = 'open'">open</button>
        <button class="chip" @click="funcName = 'connect'">connect</button>
        <button class="chip" @click="funcName = '*'">*</button>
      </p>
    </div>

    <!-- Middle: action composer -->
    <div class="pane">
      <h4>Action chain</h4>
      <ol class="chain">
        <li v-for="(a, idx) in actionChain" :key="idx" class="step">
          <div class="step-head">
            <strong>{{ idx + 1 }}. {{ a.name }}</strong>
            <span class="step-actions">
              <button class="mini" :disabled="idx === 0" @click="moveUp(idx)">↑</button>
              <button class="mini" :disabled="idx === actionChain.length - 1" @click="moveDown(idx)">↓</button>
              <button class="mini" @click="remove(idx)">✕</button>
            </span>
          </div>
          <div v-for="p in actionDef(a.name)?.params || []" :key="p.key" class="param">
            <label>
              {{ p.key }}:
              <select v-if="p.type === 'select'" v-model="a.params[p.key]">
                <option v-for="opt in p.options" :key="opt" :value="opt">{{ opt }}</option>
              </select>
              <input v-else-if="p.type === 'number'"
                     type="number"
                     v-model.number="a.params[p.key]"
                     :min="p.min" :max="p.max" :step="p.step" />
              <input v-else
                     type="text"
                     v-model="a.params[p.key]" />
            </label>
          </div>
        </li>
      </ol>

      <div class="palette"
           @dragover="onDragOver"
           @drop="onDrop">
        <p class="hint">Drag actions here to add, or click:</p>
        <div class="palette-grid">
          <button v-for="a in ACTIONS" :key="a.name"
                  class="palette-item"
                  draggable="true"
                  @dragstart="onDragStart(a)"
                  @click="onDragStart(a); onDrop()">
            <strong>{{ a.name }}</strong>
            <small>{{ a.desc }}</small>
          </button>
        </div>
      </div>
    </div>

    <!-- Right: live JSON -->
    <div class="pane">
      <h4>Generated config
        <button class="ghost" @click="copyJSON">
          {{ copied ? "Copied!" : "Copy" }}
        </button>
      </h4>
      <pre class="json-out"><code>{{ jsonConfig }}</code></pre>
      <p class="hint">
        Paste into the
        <a :href="VALIDATOR_URL">Config Validator</a>
        to verify, then save as <code>/tmp/recipe.json</code>.
      </p>
    </div>
  </div>
</template>

<style scoped>
.rb {
  display: grid;
  grid-template-columns: 1fr 1.5fr 1.5fr;
  gap: 16px;
  padding: 16px;
  background: var(--surface, #fafafa);
  border: 1px solid var(--border, #ddd);
  border-radius: 8px;
}
@media (max-width: 900px) {
  .rb { grid-template-columns: 1fr; }
}

.pane {
  background: var(--surface-elev, #fff);
  border: 1px solid var(--border, #ddd);
  border-radius: 6px;
  padding: 12px;
  display: flex;
  flex-direction: column;
  gap: 8px;
}
h4 {
  margin: 0 0 4px;
  font-size: 13px;
  display: flex;
  justify-content: space-between;
  align-items: center;
}

input, select {
  width: 100%;
  padding: 6px 8px;
  font-family: var(--mono, monospace);
  font-size: 12px;
  border: 1px solid var(--border, #ddd);
  border-radius: 4px;
  background: var(--surface-elev, #fff);
  color: var(--text, #222);
  box-sizing: border-box;
}

.hint {
  font-size: 11px;
  color: var(--muted, #888);
  margin: 4px 0;
}

.chip {
  display: inline-block;
  padding: 2px 6px;
  margin: 0 2px;
  font-size: 11px;
  background: var(--surface, #eee);
  border: 1px solid var(--border, #ddd);
  border-radius: 3px;
  cursor: pointer;
}

.chain {
  list-style: none;
  padding: 0;
  margin: 0;
  display: flex;
  flex-direction: column;
  gap: 6px;
}
.step {
  background: var(--surface, #f8f9fa);
  border: 1px solid var(--border, #ecf0f1);
  border-radius: 4px;
  padding: 6px 8px;
  font-size: 12px;
}
.step-head {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 4px;
}
.step-actions { display: flex; gap: 2px; }

button.mini {
  padding: 2px 6px;
  font-size: 11px;
  background: transparent;
  border: 1px solid var(--border, #ddd);
  border-radius: 3px;
  cursor: pointer;
}
button.mini:disabled { opacity: 0.3; cursor: not-allowed; }

.param {
  margin: 2px 0;
}
.param label {
  display: flex;
  gap: 4px;
  align-items: center;
  font-size: 11px;
  color: var(--muted, #666);
}

.palette {
  border: 1px dashed var(--border, #ccc);
  border-radius: 4px;
  padding: 8px;
  margin-top: 8px;
}
.palette-grid {
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(120px, 1fr));
  gap: 4px;
}
.palette-item {
  display: flex;
  flex-direction: column;
  padding: 4px 6px;
  background: var(--surface-elev, #fff);
  border: 1px solid var(--border, #ddd);
  border-radius: 3px;
  cursor: grab;
  font-size: 11px;
  text-align: left;
}
.palette-item:hover { border-color: var(--accent, #6ed6ff); }
.palette-item:active { cursor: grabbing; }
.palette-item strong { font-size: 11px; }
.palette-item small { color: var(--muted, #888); }

button.ghost {
  padding: 2px 8px;
  font-size: 11px;
  background: transparent;
  border: 1px solid var(--border, #ddd);
  border-radius: 3px;
  cursor: pointer;
}

.json-out {
  background: var(--code-bg, #1a1a1a);
  color: var(--code-fg, #e8e8e8);
  padding: 8px 10px;
  border-radius: 4px;
  font-size: 11px;
  line-height: 1.4;
  max-height: 360px;
  overflow: auto;
  flex-grow: 1;
  margin: 0;
}

a { color: var(--accent, #2c3e50); }
code { font-family: var(--mono, monospace); }
</style>
