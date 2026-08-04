<script setup>
import { ref, computed, onMounted, onBeforeUnmount, nextTick, watch } from "vue";

// Site-wide search index. Each entry has:
//   label    — what to display
//   kind     — section / tutorial / faq / glossary / persona / action
//   anchor   — URL fragment to jump to (#something)
//   keywords  — extra searchable text (lowercased)
//
// Built by hand from the page composition. Update when sections are
// added or renamed.

const INDEX = [
  // Top-level sections
  { label: "Why retrace exists",          kind: "section", anchor: "#why",          keywords: "problem statement competitor strace frida libfuzzer" },
  { label: "Trust (license, telemetry)",  kind: "section", anchor: "#trust",        keywords: "bsd license dependencies cve security maintainer" },
  { label: "Install",                      kind: "section", anchor: "#install",      keywords: "curl download setup get started" },
  { label: "Playground (try commands)",    kind: "section", anchor: "#playground",   keywords: "try simulate terminal interactive demo" },
  { label: "How it works (anatomy)",       kind: "section", anchor: "#anatomy",      keywords: "trampoline wrapper caller libc engine frame" },
  { label: "Performance / overhead",       kind: "section", anchor: "#performance",  keywords: "speed cost tuning env vars production slow" },
  { label: "Actions (12 built-in)",        kind: "section", anchor: "#actions",      keywords: "log_params call_real modify memory_fuzz sandbox delay" },
  { label: "Function catalog",             kind: "section", anchor: "#functions",    keywords: "interceptable open malloc connect pthread" },
  { label: "Use cases (who uses retrace)", kind: "section", anchor: "#use-cases",    keywords: "audience personas security qa devops researcher" },
  { label: "Tutorial paths",               kind: "section", anchor: "#paths",        keywords: "learning sequence beginner guided" },
  { label: "Tutorials",                     kind: "section", anchor: "#tutorials",    keywords: "scenario walkthrough step by step how to" },
  { label: "Platforms",                     kind: "section", anchor: "#platforms",    keywords: "linux macos windows freebsd android arm64 x86_64" },
  { label: "Comparison (vs alternatives)",  kind: "section", anchor: "",              keywords: "strace frida libfuzzer ebpf compare" },
  { label: "Recipes (cookbook highlights)", kind: "section", anchor: "",              keywords: "json config snippets examples" },
  { label: "Config validator",              kind: "section", anchor: "#validator",    keywords: "validate json check syntax" },
  { label: "What's new / roadmap",          kind: "section", anchor: "#whats-new",    keywords: "changelog release v2.1 v2.2 v2.3 roadmap shipping" },
  { label: "Community / contribute",        kind: "section", anchor: "#community",    keywords: "contribute discuss support ribose commercial" },
  { label: "FAQ",                           kind: "section", anchor: "#faq",          keywords: "questions language stripped pie overhead production" },
  { label: "Glossary",                      kind: "section", anchor: "#glossary",     keywords: "terms definitions trampoline action script prototype" },
  { label: "Quick reference",               kind: "section", anchor: "#quick-ref",    keywords: "cheat sheet commands env vars patterns" },

  // Tutorials (22)
  { label: "Find what's making my program slow",          kind: "tutorial", anchor: "#tutorials", keywords: "slow performance trace timing flamegraph" },
  { label: "Test how my code handles malloc failures",    kind: "tutorial", anchor: "#tutorials", keywords: "oom memory fuzz null fail" },
  { label: "Sandbox an untrusted binary",                  kind: "tutorial", anchor: "#tutorials", keywords: "sandbox deny paths shadow" },
  { label: "See what a binary actually does",              kind: "tutorial", anchor: "#tutorials", keywords: "reverse engineer map behavior" },
  { label: "Make a binary think it's root",                kind: "tutorial", anchor: "#tutorials", keywords: "mock getuid root sudo" },
  { label: "Find a memory leak",                           kind: "tutorial", anchor: "#tutorials", keywords: "leak malloc free count diff" },
  { label: "Add fuzzing to my CI pipeline",                kind: "tutorial", anchor: "#tutorials", keywords: "ci github actions workflow fuzz" },
  { label: "Trace an Android app's native code",           kind: "tutorial", anchor: "#tutorials", keywords: "android mobile ndk arm64 wrap.sh" },
  { label: "Test how my code handles network failures",    kind: "tutorial", anchor: "#tutorials", keywords: "network connect econnrefused inject" },
  { label: "Mock the system clock",                        kind: "tutorial", anchor: "#tutorials", keywords: "time clock mock freeze expiry ttl" },
  { label: "Redirect a file path",                         kind: "tutorial", anchor: "#tutorials", keywords: "redirect open path swap" },
  { label: "Audit environment variables",                  kind: "tutorial", anchor: "#tutorials", keywords: "getenv audit secret leak env" },
  { label: "Capture network traffic as JSON",              kind: "tutorial", anchor: "#tutorials", keywords: "send recv network capture pcap" },
  { label: "Trace a statically-linked binary",             kind: "tutorial", anchor: "#tutorials", keywords: "static ptrace binary" },
  { label: "Audit system() and execve() calls",            kind: "tutorial", anchor: "#tutorials", keywords: "system exec command injection cwe-78" },
  { label: "Verify a binary makes no outbound network",    kind: "tutorial", anchor: "#tutorials", keywords: "airgap offline network verify telemetry" },
  { label: "Generate a flamegraph",                        kind: "tutorial", anchor: "#tutorials", keywords: "flamegraph svg bar chart perf" },
  { label: "Profile lock contention",                      kind: "tutorial", anchor: "#tutorials", keywords: "pthread mutex lock contention" },
  { label: "Write a custom action",                        kind: "tutorial", anchor: "#tutorials", keywords: "custom action extend src/core/actions" },
  { label: "Debug a production issue (safely)",            kind: "tutorial", anchor: "#tutorials", keywords: "production debug trace safe scrub" },
  { label: "Integrate retrace into your build",            kind: "tutorial", anchor: "#tutorials", keywords: "build cmake make ci fuzz target" },
  { label: "Build retrace from source",                    kind: "tutorial", anchor: "#tutorials", keywords: "build source cmake ninja clone compile" },

  // Glossary terms (17)
  { label: "Glossary: Interception",            kind: "glossary", anchor: "#glossary", keywords: "intercept definition" },
  { label: "Glossary: Trampoline",              kind: "glossary", anchor: "#glossary", keywords: "assembly bounce wrapper" },
  { label: "Glossary: Wrapper",                 kind: "glossary", anchor: "#glossary", keywords: "symbol replace caller" },
  { label: "Glossary: Engine",                  kind: "glossary", anchor: "#glossary", keywords: "retrace_engine_wrapper dispatch" },
  { label: "Glossary: Frame",                   kind: "glossary", anchor: "#glossary", keywords: "wrappersystemvframe registers" },
  { label: "Glossary: Real impl",               kind: "glossary", anchor: "#glossary", keywords: "real libc dlsym rtld_next" },
  { label: "Glossary: Real-impl indirection",   kind: "glossary", anchor: "#glossary", keywords: "reentrancy guard struct" },
  { label: "Glossary: Script",                  kind: "glossary", anchor: "#glossary", keywords: "intercept script func_name actions" },
  { label: "Glossary: Action",                  kind: "glossary", anchor: "#glossary", keywords: "primitive log_params call_real modify" },
  { label: "Glossary: Prototype",               kind: "glossary", anchor: "#glossary", keywords: "funcprototype abi param meta" },
  { label: "Glossary: JSON config",             kind: "glossary", anchor: "#glossary", keywords: "config file retrace_json_config" },
  { label: "Glossary: Log",                     kind: "glossary", anchor: "#glossary", keywords: "json lines output trace" },
  { label: "Glossary: Preload",                 kind: "glossary", anchor: "#glossary", keywords: "ld_preload dyld_insert_libraries" },
  { label: "Glossary: Backend",                 kind: "glossary", anchor: "#glossary", keywords: "preload_elf preload_macho ptrace" },
  { label: "Glossary: ptrace",                  kind: "glossary", anchor: "#glossary", keywords: "kernel debug static binary" },
  { label: "Glossary: ABI",                     kind: "glossary", anchor: "#glossary", keywords: "application binary interface" },
  { label: "Glossary: AAPCS64",                 kind: "glossary", anchor: "#glossary", keywords: "aarch64 procedure call standard" },

  // Personas (25, condensed)
  { label: "Persona: Security researcher",      kind: "persona", anchor: "#use-cases", keywords: "security reverse engineer binary" },
  { label: "Persona: QA engineer",              kind: "persona", anchor: "#use-cases", keywords: "qa test fuzz" },
  { label: "Persona: Backend developer",        kind: "persona", anchor: "#use-cases", keywords: "backend hot path trace" },
  { label: "Persona: DevOps / SRE",             kind: "persona", anchor: "#use-cases", keywords: "devops sre container" },
  { label: "Persona: Penetration tester",       kind: "persona", anchor: "#use-cases", keywords: "pentest sandbox untrusted" },
  { label: "Persona: Educator / student",       kind: "persona", anchor: "#use-cases", keywords: "education student teach" },
  { label: "Persona: CTF player",               kind: "persona", anchor: "#use-cases", keywords: "ctf challenge flag" },
  { label: "Persona: Reverse engineer",         kind: "persona", anchor: "#use-cases", keywords: "re closed-source ida" },
  { label: "Persona: Forensics analyst",        kind: "persona", anchor: "#use-cases", keywords: "forensics post-incident replay" },
  { label: "Persona: Bug bounty hunter",        kind: "persona", anchor: "#use-cases", keywords: "bounty vuln exhaust" },
  { label: "Persona: Mobile developer",         kind: "persona", anchor: "#use-cases", keywords: "mobile android ios" },
  { label: "Persona: System administrator",     kind: "persona", anchor: "#use-cases", keywords: "sysadmin diagnose service" },
  { label: "Persona: Cryptography auditor",     kind: "persona", anchor: "#use-cases", keywords: "crypto ssl rand_bytes" },
  { label: "Persona: Embedded developer",       kind: "persona", anchor: "#use-cases", keywords: "embedded firmware iot" },
  { label: "Persona: Library author",           kind: "persona", anchor: "#use-cases", keywords: "library abi author" },
  { label: "Persona: Red team operator",        kind: "persona", anchor: "#use-cases", keywords: "red team defensive controls" },
  { label: "Persona: Incident responder",       kind: "persona", anchor: "#use-cases", keywords: "ir incident evidence" },
  { label: "Persona: Performance engineer",     kind: "persona", anchor: "#use-cases", keywords: "perf overhead optimize" },
  { label: "Persona: Vulnerability researcher", kind: "persona", anchor: "#use-cases", keywords: "vr zero-day boundary" },
  { label: "Persona: Game developer",           kind: "persona", anchor: "#use-cases", keywords: "game native asset" },
  { label: "Persona: Compiler engineer",        kind: "persona", anchor: "#use-cases", keywords: "compiler libc intercept" },
  { label: "Persona: Build / release engineer", kind: "persona", anchor: "#use-cases", keywords: "build release ci" },
  { label: "Persona: Database engineer",        kind: "persona", anchor: "#use-cases", keywords: "database db slow query" },
  { label: "Persona: Distributed systems eng",  kind: "persona", anchor: "#use-cases", keywords: "distributed consensus raft" },
  { label: "Persona: ML engineer",              kind: "persona", anchor: "#use-cases", keywords: "ml python native binding" },

  // Key external links
  { label: "Cookbook (21 recipes)",             kind: "link", anchor: "https://github.com/riboseinc/retrace/blob/main/docs/cookbook/README.md", keywords: "cookbook recipe json config" },
  { label: "GitHub repository",                  kind: "link", anchor: "https://github.com/riboseinc/retrace", keywords: "github source code repo" },
  { label: "CLI reference (docs/cli.md)",        kind: "link", anchor: "https://github.com/riboseinc/retrace/blob/main/docs/cli.md", keywords: "cli subcommand reference env var" },
  { label: "Configuration reference",            kind: "link", anchor: "https://github.com/riboseinc/retrace/blob/main/docs/configuration.md", keywords: "config json schema action_params" },
];

const KIND_META = {
  section:  { label: "Section",  accent: "see" },
  tutorial: { label: "Tutorial", accent: "control" },
  glossary: { label: "Glossary", accent: "control" },
  persona:  { label: "Persona",  accent: "see" },
  link:     { label: "External", accent: "break" },
};

const open = ref(false);
const query = ref("");
const activeIdx = ref(0);
const inputEl = ref(null);
const listEl = ref(null);

const results = computed(() => {
  const q = query.value.trim().toLowerCase();
  if (!q) return INDEX.slice(0, 8); // show top sections by default
  const qs = q.split(/\s+/);
  return INDEX.filter((entry) => {
    const hay = (entry.label + " " + entry.keywords).toLowerCase();
    return qs.every((token) => hay.includes(token));
  }).slice(0, 12);
});

watch(results, () => { activeIdx.value = 0; });

function openSearch() {
  open.value = true;
  query.value = "";
  activeIdx.value = 0;
  nextTick(() => inputEl.value && inputEl.value.focus());
}

function closeSearch() {
  open.value = false;
}

function pick(entry) {
  if (!entry) return;
  if (entry.anchor && entry.anchor.startsWith("#")) {
    const el = document.querySelector(entry.anchor);
    if (el) el.scrollIntoView({ behavior: "smooth", block: "start" });
  } else if (entry.anchor) {
    window.open(entry.anchor, "_blank");
  }
  closeSearch();
}

function onKeydown(e) {
  if (!open.value) {
    if ((e.metaKey || e.ctrlKey) && e.key === "k") {
      e.preventDefault();
      openSearch();
    }
    return;
  }
  switch (e.key) {
    case "Escape":
      e.preventDefault();
      closeSearch();
      break;
    case "ArrowDown":
      e.preventDefault();
      activeIdx.value = Math.min(activeIdx.value + 1, results.value.length - 1);
      scrollActiveIntoView();
      break;
    case "ArrowUp":
      e.preventDefault();
      activeIdx.value = Math.max(activeIdx.value - 1, 0);
      scrollActiveIntoView();
      break;
    case "Enter":
      e.preventDefault();
      pick(results.value[activeIdx.value]);
      break;
  }
}

function scrollActiveIntoView() {
  nextTick(() => {
    if (!listEl.value) return;
    const active = listEl.value.querySelector("[data-active='true']");
    if (active && typeof active.scrollIntoView === "function") {
      active.scrollIntoView({ block: "nearest" });
    }
  });
}

onMounted(() => {
  document.addEventListener("keydown", onKeydown);
});

onBeforeUnmount(() => {
  document.removeEventListener("keydown", onKeydown);
});
</script>

<template>
  <div class="search-slot">
    <button class="search-trigger" @click="openSearch" aria-label="Search the site">
      <span class="trigger-icon">⌕</span>
      <span class="trigger-text">Search</span>
      <kbd class="trigger-kbd">⌘K</kbd>
    </button>

    <Teleport to="body">
      <Transition name="fade">
        <div v-if="open" class="search-overlay" @click.self="closeSearch">
          <div class="search-modal glass">
            <div class="search-input-row">
              <span class="modal-icon">⌕</span>
              <input
                ref="inputEl"
                v-model="query"
                type="search"
                class="modal-input"
                placeholder="Search sections, tutorials, glossary, personas…"
                aria-label="Search query"
                @input="activeIdx = 0"
              />
              <kbd class="modal-kbd">ESC</kbd>
            </div>

            <ul ref="listEl" class="result-list">
              <li v-if="results.length === 0" class="empty">
                No matches. Try a different term, or hit ESC to close.
              </li>
              <li
                v-for="(r, i) in results"
                :key="r.label"
                :data-active="i === activeIdx ? 'true' : 'false'"
                :class="['result-item', `kind-${r.kind}`, { active: i === activeIdx }]"
                @click="pick(r)"
                @mousemove="activeIdx = i"
              >
                <div :class="['result-kind', `accent-${KIND_META[r.kind].accent}`]">
                  {{ KIND_META[r.kind].label }}
                </div>
                <div class="result-text">{{ r.label }}</div>
              </li>
            </ul>

            <div class="modal-footer">
              <span><kbd>↑</kbd><kbd>↓</kbd> navigate</span>
              <span><kbd>↵</kbd> select</span>
              <span><kbd>ESC</kbd> close</span>
              <span class="count">{{ results.length }} result{{ results.length === 1 ? "" : "s" }}</span>
            </div>
          </div>
        </div>
      </Transition>
    </Teleport>
  </div>
</template>

<style scoped>
.search-slot {
  display: inline-flex;
}

.search-trigger {
  display: inline-flex;
  align-items: center;
  gap: 8px;
  background: rgba(255, 255, 255, 0.04);
  border: 1px solid rgba(255, 255, 255, 0.08);
  color: var(--color-dim);
  font-family: var(--font-mono);
  font-size: 12px;
  padding: 6px 10px 6px 12px;
  border-radius: 6px;
  cursor: pointer;
  transition: all 0.18s var(--ease-glass);
}
.search-trigger:hover {
  background: rgba(255, 255, 255, 0.08);
  color: var(--color-text);
  border-color: rgba(255, 255, 255, 0.18);
}
.trigger-icon {
  font-size: 13px;
  color: var(--color-see);
}
.trigger-text { letter-spacing: 0.04em; }
.trigger-kbd {
  font-family: var(--font-mono);
  font-size: 10px;
  padding: 1px 5px;
  border-radius: 3px;
  background: rgba(255, 255, 255, 0.06);
  border: 1px solid rgba(255, 255, 255, 0.1);
  color: var(--color-dim);
  letter-spacing: 0.02em;
}
@media (max-width: 860px) {
  .trigger-text { display: none; }
  .trigger-kbd { display: none; }
  .search-trigger { padding: 6px 8px; }
}

.search-overlay {
  position: fixed;
  inset: 0;
  background: rgba(7, 9, 13, 0.7);
  backdrop-filter: blur(6px);
  -webkit-backdrop-filter: blur(6px);
  z-index: 100;
  display: flex;
  align-items: flex-start;
  justify-content: center;
  padding: 12vh 24px 24px;
}

.search-modal {
  width: 100%;
  max-width: 640px;
  background: linear-gradient(180deg, rgba(28, 33, 42, 0.85) 0%, rgba(20, 24, 30, 0.75) 100%);
  backdrop-filter: blur(28px) saturate(180%);
  -webkit-backdrop-filter: blur(28px) saturate(180%);
  border: 1px solid rgba(255, 255, 255, 0.1);
  border-radius: 14px;
  box-shadow:
    inset 0 1px 0 rgba(255, 255, 255, 0.1),
    0 32px 80px -20px rgba(0, 0, 0, 0.8);
  overflow: hidden;
  display: flex;
  flex-direction: column;
  max-height: 76vh;
}

.search-input-row {
  display: flex;
  align-items: center;
  gap: 12px;
  padding: 14px 18px;
  border-bottom: 1px solid rgba(255, 255, 255, 0.05);
}
.modal-icon {
  color: var(--color-dim);
  font-size: 14px;
}
.modal-input {
  flex: 1;
  background: transparent;
  border: none;
  color: var(--color-text);
  font-family: var(--font-mono);
  font-size: 14px;
}
.modal-input:focus { outline: none; }
.modal-input::placeholder { color: var(--color-dim); }
.modal-kbd {
  font-family: var(--font-mono);
  font-size: 10px;
  padding: 2px 6px;
  border-radius: 3px;
  background: rgba(255, 255, 255, 0.06);
  border: 1px solid rgba(255, 255, 255, 0.1);
  color: var(--color-dim);
}

.result-list {
  list-style: none;
  padding: 6px;
  margin: 0;
  overflow-y: auto;
  flex: 1;
}
.empty {
  padding: 24px 16px;
  text-align: center;
  color: var(--color-dim);
  font-size: 13px;
}
.result-item {
  display: grid;
  grid-template-columns: 80px 1fr;
  align-items: center;
  gap: 14px;
  padding: 10px 14px;
  border-radius: 8px;
  cursor: pointer;
  transition: background 0.12s;
}
.result-item.active {
  background: rgba(94, 227, 255, 0.08);
}
.result-kind {
  font-family: var(--font-mono);
  font-size: 9.5px;
  font-weight: 600;
  letter-spacing: 0.16em;
  text-transform: uppercase;
  padding: 2px 8px;
  border-radius: 3px;
  border: 1px solid;
  text-align: center;
  justify-self: start;
}
.result-kind.accent-see {
  color: var(--color-see);
  border-color: rgba(94, 227, 255, 0.3);
  background: rgba(94, 227, 255, 0.06);
}
.result-kind.accent-control {
  color: var(--color-control);
  border-color: rgba(242, 180, 65, 0.3);
  background: rgba(242, 180, 65, 0.06);
}
.result-kind.accent-break {
  color: var(--color-break);
  border-color: rgba(255, 107, 92, 0.3);
  background: rgba(255, 107, 92, 0.06);
}
.result-text {
  font-size: 13.5px;
  color: var(--color-text);
  letter-spacing: -0.005em;
}

.modal-footer {
  display: flex;
  gap: 14px;
  padding: 10px 18px;
  border-top: 1px solid rgba(255, 255, 255, 0.05);
  font-family: var(--font-mono);
  font-size: 10.5px;
  color: var(--color-dim);
  background: rgba(7, 9, 13, 0.3);
}
.modal-footer kbd {
  font-family: var(--font-mono);
  font-size: 10px;
  padding: 1px 5px;
  border-radius: 3px;
  background: rgba(255, 255, 255, 0.06);
  border: 1px solid rgba(255, 255, 255, 0.08);
  margin-right: 4px;
}
.modal-footer .count {
  margin-left: auto;
}

.fade-enter-active, .fade-leave-active {
  transition: opacity 0.18s;
}
.fade-enter-from, .fade-leave-to {
  opacity: 0;
}
</style>
