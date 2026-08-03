<script setup>
import { ref, computed, watch } from "vue";

// Config Validator — paste a JSON config, get immediate feedback.
// Mirrors what `retrace validate <config.json>` does in the CLI:
// parses JSON, checks the top-level shape, validates every action_name,
// verifies required action_params. Runs in the browser; nothing leaves
// the page.

const VALID_ACTIONS = {
  log_params:                 { required: [],         optional: ["omit_params"] },
  call_real:                  { required: [],         optional: [] },
  fuzzing_seed:               { required: ["seed"],   optional: [] },
  modify_in_param_str:        { required: ["param_name", "new_str"],     optional: ["match_str"] },
  modify_in_param_int:        { required: ["param_name", "new_int"],     optional: ["match_int"] },
  modify_in_param_arr:        { required: ["param_name", "new_arr"],     optional: ["match_arr"] },
  modify_return_value_int:    { required: ["retval_int"],                optional: [] },
  delay:                      { required: ["ms"],     optional: [] },
  memory_fuzz:                { required: ["fail_rate"], optional: [] },
  incomplete_io:              { required: ["rate"],   optional: [] },
  call_count_limit:           { required: ["limit"],  optional: [] },
  sandbox:                    { required: ["deny_paths"], optional: [] },
};

const EXAMPLES = [
  {
    name: "Trace everything",
    json: `{
  "intercept_scripts": [
    {
      "func_name": "*",
      "actions": [
        { "action_name": "log_params" },
        { "action_name": "call_real" }
      ]
    }
  ]
}`,
  },
  {
    name: "Fuzz malloc at 10%",
    json: `{
  "intercept_scripts": [
    {
      "func_name": "malloc",
      "actions": [
        { "action_name": "call_real" },
        { "action_name": "memory_fuzz",
          "action_params": { "fail_rate": 0.1 } }
      ]
    }
  ]
}`,
  },
  {
    name: "Sandbox sensitive paths",
    json: `{
  "intercept_scripts": [
    {
      "func_name": "open",
      "actions": [
        { "action_name": "sandbox",
          "action_params": {
            "deny_paths": ["/etc/shadow", "/root/.ssh/"]
          } }
      ]
    }
  ]
}`,
  },
  {
    name: "Mock getuid",
    json: `{
  "intercept_scripts": [
    {
      "func_name": "getuid",
      "actions": [
        { "action_name": "modify_return_value_int",
          "action_params": { "retval_int": 0 } }
      ]
    }
  ]
}`,
  },
  {
    name: "Invalid (typo)",
    json: `{
  "intercept_scripts": [
    {
      "func_name": "malloc",
      "actions": [
        { "action_name": "modify_return_value",
          "action_params": { "retval_int": 0 } }
      ]
    }
  ]
}`,
  },
];

const input = ref(EXAMPLES[0].json);
const result = ref(null);

function setResult(level, summary, details) {
  result.value = { level, summary, details };
}

function validate() {
  const text = input.value.trim();
  if (!text) {
    setResult("error", "Empty input.", ["Paste a JSON config on the left."]);
    return;
  }

  let parsed;
  try {
    parsed = JSON.parse(text);
  } catch (e) {
    setResult("error", "Invalid JSON.", [e.message]);
    return;
  }

  if (typeof parsed !== "object" || parsed === null) {
    setResult("error", "Top level must be an object.", ["Wrap your config in { ... }."]);
    return;
  }

  if (!parsed.intercept_scripts) {
    setResult("error", "Missing top-level field.", ["'intercept_scripts' array is required."]);
    return;
  }

  if (!Array.isArray(parsed.intercept_scripts)) {
    setResult("error", "'intercept_scripts' must be an array.", ["Wrap each script in { ... } inside the array."]);
    return;
  }

  if (parsed.intercept_scripts.length === 0) {
    setResult("warn", "No scripts defined.", ["Add at least one entry to 'intercept_scripts' or retrace won't intercept anything."]);
    return;
  }

  const details = [];
  let scriptCount = 0;
  let actionCount = 0;
  let hasError = false;

  parsed.intercept_scripts.forEach((script, i) => {
    scriptCount++;
    if (!script.func_name) {
      details.push({ level: "error", msg: `script ${i + 1}: missing 'func_name'. Use a function name or "*" for wildcard.` });
      hasError = true;
      return;
    }
    if (!script.actions || !Array.isArray(script.actions)) {
      details.push({ level: "error", msg: `script ${i + 1} (func_name="${script.func_name}"): missing or non-array 'actions'.` });
      hasError = true;
      return;
    }
    script.actions.forEach((action, j) => {
      actionCount++;
      if (!action.action_name) {
        details.push({ level: "error", msg: `script ${i + 1}, action ${j + 1}: missing 'action_name'.` });
        hasError = true;
        return;
      }
      const spec = VALID_ACTIONS[action.action_name];
      if (!spec) {
        const hint = closestAction(action.action_name);
        details.push({
          level: "error",
          msg: `script ${i + 1}, action ${j + 1}: unknown action '${action.action_name}'.${hint ? ` Did you mean '${hint}'?` : ""}`,
        });
        hasError = true;
        return;
      }
      const params = action.action_params || {};
      for (const req of spec.required) {
        if (!(req in params)) {
          details.push({
            level: "error",
            msg: `script ${i + 1}, action '${action.action_name}': missing required param '${req}'.`,
          });
          hasError = true;
        }
      }
      const known = new Set([...spec.required, ...spec.optional]);
      for (const key of Object.keys(params)) {
        if (!known.has(key)) {
          details.push({
            level: "warn",
            msg: `script ${i + 1}, action '${action.action_name}': unknown param '${key}' (will be ignored).`,
          });
        }
      }
    });
  });

  if (hasError) {
    setResult("error", `${scriptCount} script(s), ${actionCount} action(s) — ${details.filter(d => d.level === "error").length} error(s).`, details.map(d => d.msg));
  } else {
    const warns = details.filter(d => d.level === "warn");
    setResult(
      warns.length === 0 ? "ok" : "warn",
      `ok: ${scriptCount} script(s), ${actionCount} action(s)${warns.length ? `, ${warns.length} warning(s)` : ""}`,
      details.map(d => d.msg)
    );
  }
}

function closestAction(name) {
  // Tiny Levenshtein-like check for common typos.
  const known = Object.keys(VALID_ACTIONS);
  const lower = name.toLowerCase();
  for (const k of known) {
    if (k.includes(lower) || lower.includes(k)) return k;
  }
  return null;
}

function loadExample(ex) {
  input.value = ex.json;
  validate();
}

// Auto-validate on first mount and whenever the input changes.
validate();
watch(input, () => validate());
</script>

<template>
  <div class="validator">
    <div class="validator-grid">
      <div class="input-pane glass-tight">
        <div class="pane-head">
          <div class="pane-label">Your JSON config</div>
          <div class="example-row">
            <span class="example-hint">Load an example:</span>
            <button
              v-for="ex in EXAMPLES"
              :key="ex.name"
              class="example-btn"
              @click="loadExample(ex)"
            >{{ ex.name }}</button>
          </div>
        </div>
        <textarea
          v-model="input"
          class="json-input"
          spellcheck="false"
          aria-label="JSON config input"
        />
      </div>

      <div :class="['result-pane', 'glass', `level-${result?.level || 'idle'}`]">
        <div class="pane-head">
          <div class="pane-label">Result</div>
          <div v-if="result" :class="['result-summary', `level-${result.level}`]">
            <span class="result-glyph">
              {{ result.level === "ok" ? "✓" : result.level === "warn" ? "▲" : result.level === "error" ? "✗" : "" }}
            </span>
            {{ result.summary }}
          </div>
        </div>
        <ul v-if="result && result.details.length" class="result-list">
          <li
            v-for="(d, i) in result.details"
            :key="i"
            :class="['result-line', `level-${result.level}`]"
          >{{ d }}</li>
        </ul>
        <div v-else-if="result && result.level === 'ok'" class="result-empty">
          This config will run. Save it as <code>config.json</code> and launch with
          <code>retrace run --config config.json -- ./your-binary</code>.
        </div>
      </div>
    </div>
  </div>
</template>

<style scoped>
.validator-grid {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 16px;
  align-items: stretch;
}
@media (max-width: 860px) {
  .validator-grid { grid-template-columns: 1fr; }
}

.input-pane, .result-pane {
  padding: 18px 20px 20px;
  display: flex;
  flex-direction: column;
  min-height: 460px;
}

.pane-head {
  margin-bottom: 12px;
  padding-bottom: 10px;
  border-bottom: 1px solid rgba(255, 255, 255, 0.05);
}
.pane-label {
  font-family: var(--font-mono);
  font-size: 10.5px;
  font-weight: 600;
  letter-spacing: 0.18em;
  text-transform: uppercase;
  color: var(--color-dim);
  margin-bottom: 8px;
}

.example-row {
  display: flex;
  flex-wrap: wrap;
  align-items: center;
  gap: 6px;
}
.example-hint {
  font-size: 11px;
  color: var(--color-dim);
  font-family: var(--font-mono);
  margin-right: 4px;
}
.example-btn {
  background: rgba(255, 255, 255, 0.03);
  border: 1px solid rgba(255, 255, 255, 0.08);
  color: var(--color-text);
  font-family: var(--font-mono);
  font-size: 10.5px;
  padding: 3px 8px;
  border-radius: 4px;
  cursor: pointer;
  transition: all 0.18s;
}
.example-btn:hover {
  background: rgba(255, 255, 255, 0.07);
  color: var(--color-see);
  border-color: rgba(94, 227, 255, 0.3);
}

.json-input {
  flex: 1;
  background: rgba(7, 9, 13, 0.7);
  border: 1px solid rgba(255, 255, 255, 0.06);
  border-radius: 6px;
  padding: 12px 14px;
  color: var(--color-text);
  font-family: var(--font-mono);
  font-size: 12px;
  line-height: 1.6;
  resize: vertical;
  min-height: 360px;
  width: 100%;
}
.json-input:focus {
  outline: none;
  border-color: rgba(94, 227, 255, 0.4);
  box-shadow: 0 0 0 3px rgba(94, 227, 255, 0.1);
}

.result-pane.level-ok { border-top: 2px solid var(--color-see); }
.result-pane.level-warn { border-top: 2px solid var(--color-control); }
.result-pane.level-error { border-top: 2px solid var(--color-break); }

.result-summary {
  font-family: var(--font-mono);
  font-size: 13px;
  font-weight: 600;
  display: flex;
  align-items: center;
  gap: 8px;
}
.result-summary.level-ok { color: var(--color-see); }
.result-summary.level-warn { color: var(--color-control); }
.result-summary.level-error { color: var(--color-break); }
.result-glyph {
  font-size: 14px;
}

.result-list {
  list-style: none;
  padding: 0;
  margin: 0;
  flex: 1;
  overflow-y: auto;
  font-family: var(--font-mono);
  font-size: 12px;
  line-height: 1.55;
}
.result-line {
  padding: 6px 0;
  border-bottom: 1px solid rgba(255, 255, 255, 0.04);
}
.result-line:last-child { border-bottom: none; }
.result-line.level-error { color: var(--color-break); }
.result-line.level-warn { color: var(--color-control); }

.result-empty {
  margin-top: 20px;
  padding: 14px 16px;
  background: rgba(94, 227, 255, 0.04);
  border: 1px solid rgba(94, 227, 255, 0.18);
  border-radius: 6px;
  font-size: 13px;
  color: var(--color-dim);
  line-height: 1.55;
}
.result-empty code {
  font-family: var(--font-mono);
  font-size: 11.5px;
  color: var(--color-text);
  background: rgba(255, 255, 255, 0.05);
  padding: 1px 5px;
  border-radius: 3px;
}
</style>
