<script setup>
import { ref, computed } from "vue";

// Each function is verified against src/v2/funcs_symbols.def.
// Categories reflect how the retrace community actually uses them.

const categories = [
  {
    id: "io",
    label: "File I/O",
    accent: "control",
    desc: "The hot path for most programs. Trace these to see what a binary actually reads or writes.",
    funcs: [
      { name: "open",     sig: "int open(const char *path, int flags, ...)" },
      { name: "openat",   sig: "int openat(int dirfd, const char *path, int flags, ...)" },
      { name: "close",    sig: "int close(int fd)" },
      { name: "read",     sig: "ssize_t read(int fd, void *buf, size_t count)" },
      { name: "pread",    sig: "ssize_t pread(int fd, void *buf, size_t count, off_t offset)" },
      { name: "write",    sig: "ssize_t write(int fd, const void *buf, size_t count)" },
      { name: "fopen",    sig: "FILE *fopen(const char *path, const char *mode)" },
      { name: "fclose",   sig: "int fclose(FILE *stream)" },
      { name: "fread",    sig: "size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream)" },
      { name: "fwrite",   sig: "size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream)" },
      { name: "fgets",    sig: "char *fgets(char *s, int size, FILE *stream)" },
      { name: "fputs",    sig: "int fputs(const char *s, FILE *stream)" },
      { name: "dup",      sig: "int dup(int oldfd)" },
      { name: "dup2",     sig: "int dup2(int oldfd, int newfd)" },
      { name: "pipe",     sig: "int pipe(int pipefd[2])" },
      { name: "popen",    sig: "FILE *popen(const char *command, const char *type)" },
    ],
  },
  {
    id: "mem",
    label: "Memory",
    accent: "see",
    desc: "Allocators and bulk byte operations. The first stop for memory-leak and OOM investigations.",
    funcs: [
      { name: "malloc",   sig: "void *malloc(size_t size)" },
      { name: "calloc",   sig: "void *calloc(size_t nmemb, size_t size)" },
      { name: "realloc",  sig: "void *realloc(void *ptr, size_t size)" },
      { name: "free",     sig: "void free(void *ptr)" },
      { name: "brk",      sig: "int brk(void *addr)" },
      { name: "memcpy",   sig: "void *memcpy(void *dest, const void *src, size_t n)" },
      { name: "memset",   sig: "void *memset(void *s, int c, size_t n)" },
      { name: "memmove",  sig: "void *memmove(void *dest, const void *src, size_t n)" },
      { name: "memcmp",   sig: "int memcmp(const void *s1, const void *s2, size_t n)" },
    ],
  },
  {
    id: "proc",
    label: "Process & exec",
    accent: "break",
    desc: "Fork, exec, system. Audit these for command injection, sandbox escape, and unexpected subprocesses.",
    funcs: [
      { name: "fork",     sig: "pid_t fork(void)" },
      { name: "execve",   sig: "int execve(const char *path, char *const argv[], char *const envp[])" },
      { name: "execvp",   sig: "int execvp(const char *file, char *const argv[])" },
      { name: "execl",    sig: "int execl(const char *path, const char *arg, ...)" },
      { name: "execlp",   sig: "int execlp(const char *file, const char *arg, ...)" },
      { name: "system",   sig: "int system(const char *command)" },
      { name: "popen",    sig: "FILE *popen(const char *command, const char *type)" },
      { name: "exit",     sig: "void exit(int status)" },
      { name: "_exit",    sig: "void _exit(int status)" },
      { name: "abort",    sig: "void abort(void)" },
      { name: "chroot",   sig: "int chroot(const char *path)" },
      { name: "nice",     sig: "int nice(int inc)" },
    ],
  },
  {
    id: "time",
    label: "Time",
    accent: "control",
    desc: "Read the clock or sleep. Mock these to freeze time, replay schedules, or test expiry.",
    funcs: [
      { name: "time",         sig: "time_t time(time_t *t)" },
      { name: "ctime_r",      sig: "char *ctime_r(const time_t *timep, char *buf)" },
      { name: "localtime_r",  sig: "struct tm *localtime_r(const time_t *timep, struct tm *result)" },
      { name: "alarm",        sig: "unsigned int alarm(unsigned int seconds)" },
      { name: "pause",        sig: "int pause(void)" },
    ],
  },
  {
    id: "id",
    label: "Identity",
    accent: "control",
    desc: "Who am I? Mock these to make a binary think it's root, in a different login session, etc.",
    funcs: [
      { name: "getuid",     sig: "uid_t getuid(void)" },
      { name: "geteuid",    sig: "uid_t geteuid(void)" },
      { name: "getgid",     sig: "gid_t getgid(void)" },
      { name: "getegid",    sig: "gid_t getegid(void)" },
      { name: "getgroups",  sig: "int getgroups(int size, gid_t list[])" },
      { name: "getlogin",   sig: "char *getlogin(void)" },
      { name: "getpid",     sig: "pid_t getpid(void)" },
      { name: "getppid",    sig: "pid_t getppid(void)" },
    ],
  },
  {
    id: "fs",
    label: "File system",
    accent: "see",
    desc: "Paths the binary touches. Pair with the sandbox action to deny sensitive locations.",
    funcs: [
      { name: "chdir",      sig: "int chdir(const char *path)" },
      { name: "getcwd",     sig: "char *getcwd(char *buf, size_t size)" },
      { name: "chown",      sig: "int chown(const char *path, uid_t owner, gid_t group)" },
      { name: "fchown",     sig: "int fchown(int fd, uid_t owner, gid_t group)" },
      { name: "lchown",     sig: "int lchown(const char *path, uid_t owner, gid_t group)" },
      { name: "link",       sig: "int link(const char *oldpath, const char *newpath)" },
      { name: "ftruncate",  sig: "int ftruncate(int fd, off_t length)" },
      { name: "access",     sig: "int access(const char *path, int mode)" },
    ],
  },
  {
    id: "pthread",
    label: "Pthreads",
    accent: "see",
    desc: "Locks, condition variables, rwlocks. Profile these to find contention and deadlocks.",
    funcs: [
      { name: "pthread_create",         sig: "int pthread_create(pthread_t *thread, ...)" },
      { name: "pthread_join",           sig: "int pthread_join(pthread_t thread, void **retval)" },
      { name: "pthread_mutex_lock",     sig: "int pthread_mutex_lock(pthread_mutex_t *mutex)" },
      { name: "pthread_mutex_unlock",   sig: "int pthread_mutex_unlock(pthread_mutex_t *mutex)" },
      { name: "pthread_cond_wait",      sig: "int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex)" },
      { name: "pthread_cond_signal",    sig: "int pthread_cond_signal(pthread_cond_t *cond)" },
      { name: "pthread_rwlock_rdlock",  sig: "int pthread_rwlock_rdlock(pthread_rwlock_t *rwlock)" },
      { name: "pthread_rwlock_wrlock",  sig: "int pthread_rwlock_wrlock(pthread_rwlock_t *rwlock)" },
    ],
  },
  {
    id: "env",
    label: "Environment",
    accent: "see",
    desc: "Env var reads. Audit these to find a binary sniffing for secrets, debug flags, or backdoors.",
    funcs: [
      { name: "getenv",    sig: "char *getenv(const char *name)" },
    ],
  },
  {
    id: "ctype",
    label: "Strings & ctype",
    accent: "see",
    desc: "Character classification and bulk byte ops. Often surprisingly hot.",
    funcs: [
      { name: "isalpha",   sig: "int isalpha(int c)" },
      { name: "isdigit",   sig: "int isdigit(int c)" },
      { name: "isspace",   sig: "int isspace(int c)" },
      { name: "isprint",   sig: "int isprint(int c)" },
      { name: "tolower",   sig: "int tolower(int c)" },
      { name: "toupper",   sig: "int toupper(int c)" },
      { name: "mblen",     sig: "int mblen(const char *s, size_t n)" },
      { name: "mbtowc",    sig: "int mbtowc(wchar_t *pwc, const char *s, size_t n)" },
    ],
  },
  {
    id: "dyn",
    label: "Dynamic loading",
    accent: "break",
    desc: "Plugins, drivers, runtime-loaded libraries. Audit these to find supply-chain attack surface.",
    funcs: [
      { name: "dlopen",    sig: "void *dlopen(const char *filename, int flags)" },
      { name: "dlclose",   sig: "int dlclose(void *handle)" },
      { name: "dlerror",   sig: "char *dlerror(void)" },
      { name: "dlsym",     sig: "void *dlsym(void *handle, const char *symbol)" },
    ],
  },
];

// Flatten for total count.
const allFuncs = categories.reduce((acc, c) => {
  for (const f of c.funcs) acc.push({ ...f, cat: c.id, catLabel: c.label, accent: c.accent });
  return acc;
}, []);

const query = ref("");
const activeCats = ref([]);
const selectedFunc = ref(null);

function toggleCat(id) {
  const i = activeCats.value.indexOf(id);
  if (i === -1) activeCats.value = [...activeCats.value, id];
  else activeCats.value = activeCats.value.filter((c) => c !== id);
}

const visibleCategories = computed(() => {
  const q = query.value.trim().toLowerCase();
  return categories
    .filter((c) => activeCats.value.length === 0 || activeCats.value.includes(c.id))
    .map((c) => ({
      ...c,
      funcs: c.funcs.filter((f) => !q || f.name.toLowerCase().includes(q) || f.sig.toLowerCase().includes(q)),
    }))
    .filter((c) => c.funcs.length > 0);
});

const totalVisible = computed(() =>
  visibleCategories.value.reduce((acc, c) => acc + c.funcs.length, 0)
);

function clearFilters() {
  query.value = "";
  activeCats.value = [];
  selectedFunc.value = null;
}

function pick(fn) {
  selectedFunc.value = selectedFunc.value && selectedFunc.value.name === fn.name ? null : fn;
}
</script>

<template>
  <div class="catalog">
    <div class="catalog-controls glass-tight">
      <div class="search-wrap">
        <input
          v-model="query"
          type="search"
          class="search-input"
          placeholder="Search by function name or signature…"
          aria-label="Search functions"
        />
        <span class="search-icon" aria-hidden="true">⌕</span>
      </div>
      <div class="cat-chips">
        <button
          v-for="c in categories"
          :key="c.id"
          :class="['cat-chip', `accent-${c.accent}`, { active: activeCats.includes(c.id) }]"
          @click="toggleCat(c.id)"
        >
          {{ c.label }}
        </button>
      </div>
      <div class="filter-meta">
        <span>{{ totalVisible }} of {{ allFuncs.length }} functions</span>
        <button v-if="query || activeCats.length" class="clear-btn" @click="clearFilters">clear</button>
      </div>
    </div>

    <div class="catalog-grid">
      <div v-for="c in visibleCategories" :key="c.id" :class="['cat-card', 'glass-tight', `accent-${c.accent}`]">
        <header class="cat-head">
          <div :class="['cat-label', `accent-${c.accent}`]">{{ c.label }}</div>
          <div class="cat-count">{{ c.funcs.length }}</div>
        </header>
        <p class="cat-desc">{{ c.desc }}</p>
        <ul class="fn-list">
          <li
            v-for="f in c.funcs"
            :key="f.name"
            :class="['fn-item', { selected: selectedFunc && selectedFunc.name === f.name }]"
            @click="pick(f)"
          >
            <span class="fn-name">{{ f.name }}</span>
            <span class="fn-sig">{{ f.sig }}</span>
          </li>
        </ul>
      </div>

      <div v-if="visibleCategories.length === 0" class="empty glass-tight">
        No functions match. <button class="clear-btn" @click="clearFilters">clear filters</button>
      </div>
    </div>

    <transition name="slide">
      <div v-if="selectedFunc" :class="['detail-pane', 'glass', `accent-${selectedFunc.accent}`]">
        <button class="close-btn" @click="selectedFunc = null" aria-label="Close">×</button>
        <div :class="['detail-tag', `accent-${selectedFunc.accent}`]">{{ selectedFunc.catLabel }}</div>
        <h3 class="detail-name">{{ selectedFunc.name }}</h3>
        <pre class="detail-sig">{{ selectedFunc.sig }}</pre>
        <p class="detail-tip">
          Try it:
          <code>retrace trace {{ selectedFunc.name }} -- ./your-binary</code>
        </p>
      </div>
    </transition>
  </div>
</template>

<style scoped>
.catalog {
  display: flex;
  flex-direction: column;
  gap: 16px;
}

.catalog-controls {
  padding: 16px 18px;
}

.search-wrap {
  position: relative;
  margin-bottom: 12px;
}
.search-input {
  width: 100%;
  background: rgba(7, 9, 13, 0.7);
  border: 1px solid rgba(255, 255, 255, 0.08);
  border-radius: 6px;
  padding: 9px 14px 9px 32px;
  color: var(--color-text);
  font-family: var(--font-mono);
  font-size: 12.5px;
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
  left: 11px;
  top: 50%;
  transform: translateY(-50%);
  color: var(--color-dim);
  font-size: 14px;
  pointer-events: none;
}

.cat-chips {
  display: flex;
  flex-wrap: wrap;
  gap: 5px;
  margin-bottom: 8px;
}
.cat-chip {
  background: rgba(255, 255, 255, 0.03);
  border: 1px solid rgba(255, 255, 255, 0.08);
  color: var(--color-dim);
  font-family: var(--font-mono);
  font-size: 10.5px;
  letter-spacing: 0.04em;
  padding: 3px 9px;
  border-radius: 10px;
  cursor: pointer;
  transition: all 0.18s var(--ease-glass);
}
.cat-chip:hover {
  background: rgba(255, 255, 255, 0.06);
  color: var(--color-text);
}
.cat-chip.accent-see.active {
  background: rgba(94, 227, 255, 0.15);
  border-color: rgba(94, 227, 255, 0.5);
  color: var(--color-see);
}
.cat-chip.accent-control.active {
  background: rgba(242, 180, 65, 0.15);
  border-color: rgba(242, 180, 65, 0.5);
  color: var(--color-control);
}
.cat-chip.accent-break.active {
  background: rgba(255, 107, 92, 0.15);
  border-color: rgba(255, 107, 92, 0.5);
  color: var(--color-break);
}

.filter-meta {
  display: flex;
  justify-content: space-between;
  align-items: center;
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

.catalog-grid {
  display: grid;
  grid-template-columns: repeat(3, 1fr);
  gap: 14px;
}
@media (max-width: 1100px) { .catalog-grid { grid-template-columns: repeat(2, 1fr); } }
@media (max-width: 720px) { .catalog-grid { grid-template-columns: 1fr; } }

.cat-card {
  padding: 20px 22px;
}
.cat-card.accent-see { border-top: 2px solid rgba(94, 227, 255, 0.35); }
.cat-card.accent-control { border-top: 2px solid rgba(242, 180, 65, 0.35); }
.cat-card.accent-break { border-top: 2px solid rgba(255, 107, 92, 0.35); }

.cat-head {
  display: flex;
  justify-content: space-between;
  align-items: baseline;
  margin-bottom: 6px;
}
.cat-label {
  font-family: var(--font-mono);
  font-size: 12px;
  font-weight: 600;
  letter-spacing: 0.12em;
  text-transform: uppercase;
}
.cat-label.accent-see { color: var(--color-see); }
.cat-label.accent-control { color: var(--color-control); }
.cat-label.accent-break { color: var(--color-break); }
.cat-count {
  font-family: var(--font-mono);
  font-size: 11px;
  color: var(--color-dim);
}
.cat-desc {
  font-size: 12.5px;
  color: var(--color-dim);
  line-height: 1.5;
  margin-bottom: 14px;
  min-height: 38px;
}

.fn-list {
  list-style: none;
  padding: 0;
  margin: 0;
  display: flex;
  flex-direction: column;
  gap: 2px;
}
.fn-item {
  padding: 7px 10px;
  border-radius: 5px;
  cursor: pointer;
  transition: background 0.15s;
  display: flex;
  flex-direction: column;
  gap: 1px;
}
.fn-item:hover {
  background: rgba(255, 255, 255, 0.04);
}
.fn-item.selected {
  background: rgba(255, 255, 255, 0.08);
}
.fn-name {
  font-family: var(--font-mono);
  font-size: 12.5px;
  color: var(--color-text);
  font-weight: 500;
}
.fn-sig {
  font-family: var(--font-mono);
  font-size: 10.5px;
  color: var(--color-dim);
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}

.empty {
  grid-column: 1 / -1;
  padding: 28px 16px;
  text-align: center;
  color: var(--color-dim);
  font-size: 13px;
}

.detail-pane {
  padding: 22px 26px;
  position: relative;
  border-top: 2px solid;
}
.detail-pane.accent-see { border-top-color: var(--color-see); }
.detail-pane.accent-control { border-top-color: var(--color-control); }
.detail-pane.accent-break { border-top-color: var(--color-break); }
.close-btn {
  position: absolute;
  top: 14px;
  right: 16px;
  background: transparent;
  border: 1px solid rgba(255, 255, 255, 0.1);
  color: var(--color-dim);
  font-size: 16px;
  width: 26px;
  height: 26px;
  border-radius: 50%;
  cursor: pointer;
  line-height: 1;
  transition: all 0.15s;
}
.close-btn:hover {
  color: var(--color-text);
  border-color: rgba(255, 255, 255, 0.25);
}
.detail-tag {
  font-family: var(--font-mono);
  font-size: 10.5px;
  font-weight: 600;
  letter-spacing: 0.16em;
  text-transform: uppercase;
  margin-bottom: 8px;
}
.detail-tag.accent-see { color: var(--color-see); }
.detail-tag.accent-control { color: var(--color-control); }
.detail-tag.accent-break { color: var(--color-break); }
.detail-name {
  font-family: var(--font-mono);
  font-size: 24px;
  font-weight: 700;
  letter-spacing: -0.02em;
  margin-bottom: 10px;
}
.detail-sig {
  background: rgba(7, 9, 13, 0.7);
  border: 1px solid rgba(255, 255, 255, 0.06);
  border-radius: 6px;
  padding: 10px 14px;
  font-family: var(--font-mono);
  font-size: 12.5px;
  color: var(--color-text);
  margin: 0 0 12px;
  overflow-x: auto;
}
.detail-tip {
  font-size: 13px;
  color: var(--color-dim);
}
.detail-tip code {
  font-family: var(--font-mono);
  font-size: 12px;
  color: var(--color-text);
  background: rgba(255, 255, 255, 0.05);
  padding: 1px 5px;
  border-radius: 3px;
}

.slide-enter-active, .slide-leave-active {
  transition: all 0.25s var(--ease-glass);
}
.slide-enter-from, .slide-leave-to {
  opacity: 0;
  transform: translateY(-8px);
}
</style>
