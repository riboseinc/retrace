/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <errno.h>
#include <string.h>

#include "actions.h"
#include "logger.h"
#include "real_impls.h"
#include "otlp_live.h"
#include "agent.h"

/*
 * sandbox -- jail file access behind an explicit policy.
 *
 * A runtime security policy for file access. When applied to open,
 * openat, fopen, etc., the action checks the path argument against
 * the configured lists and blocks access on a match (or, in
 * allow mode, on a non-match).
 *
 * Run BEFORE call_real (or as the only action) so the real call
 * never executes for denied paths. The action sets ret_val to -1
 * and aborts the script.
 *
 * action_params (at least one policy):
 *   deny_paths  - JSON array of path strings to block. Exact match
 *                 or prefix match (if the entry ends with '/' or
 *                 '\'). Allow-by-default.
 *   allow_paths - JSON array of paths that MAY be touched.
 *                 Deny-by-default: anything not on the list is
 *                 blocked. This is the jail mode retrace-profile
 *                 emits (--jail-out).
 *   deny_classes- JSON array of ACCESS CLASSES to block outright:
 *                 ["write"] = read-only detonation (class from the
 *                 function + mode/flags: fopen "w/a/+", open
 *                 O_WRONLY/O_RDWR, unlink/rename/mkdir/...).
 *                 Independent of any path list.
 *   allow_env / deny_env - env NAME lists for getenv (deny ->
 *                 NULL) and setenv/putenv (deny -> -1).
 *   decoy_dir   - DECEPTION mode: instead of denying an
 *                 allow_paths miss on a READ, rewrite the path to
 *                 decoy_dir/<basename> and let the real call run
 *                 against the decoy. Denial is a detectable
 *                 signal; a plausible fake keeps the sample on
 *                 its happy path (the deception is logged).
 * When both path lists are present a path must pass both checks.
 *
 * Example JSON (block access to /etc/shadow and /root/):
 *   {
 *     "func_name": "open",
 *     "actions": [
 *       { "action_name": "sandbox",
 *         "action_params": {
 *           "deny_paths": ["/etc/shadow", "/etc/sudoers", "/root/"]
 *         }
 *       },
 *       { "action_name": "call_real" }
 *     ]
 *   }
 */

/*
 * One list check. Returns 1 when path_arg matches entry (exact, or
 * prefix when the entry ends with a separator).
 */
static int path_matches(const char *path_arg, const char *entry)
{
	size_t elen = retrace_real_impls.strlen(entry);
	char last;

	if (elen == 0)
		return 0;
	last = entry[elen - 1];
	if (last == '/' || last == '\\') {
		/* Prefix match: everything under this directory */
		return retrace_real_impls.strncmp(path_arg, entry,
			elen) == 0;
	}
	return retrace_real_impls.strcmp(path_arg, entry) == 0;
}

/*
 * The membership deepening (architecture-review F3): a policy
 * list was walked linearly on every path-policed call -- up to
 * N strcmps per intercepted open under an N-path policy. The
 * set is instead COMPILED once per policy array: entries bucket
 * by their first path component's hash, so a lookup touches
 * only its bucket plus the small wildcard list (relative-path
 * and root-prefix rules). Entries are pointers INTO the config
 * tree (no copies); the compiled set is cached against the
 * array pointer and revalidated (count + first entry) per call
 * -- a policy swap that frees the old tree can at worst alias
 * the cache into a rebuild, never a wrong answer.
 */
#define PATH_BUCKETS 32
#define PATH_SETS_MAX 8

struct path_set {
	size_t count;		/* entries, for validation */
	const char *first;	/* first entry ptr, ditto */
	size_t off[PATH_BUCKETS + 2];	/* prefix sums; b..b+1 */
	size_t idx[];		/* entry indices, bucketed */
};

struct path_set_cache {
	JSON_Array *arr;
	struct path_set *set;
};

static struct path_set_cache g_path_sets[PATH_SETS_MAX];
static int g_path_set_next;

static uint32_t first_comp_hash(const char *s)
{
	uint32_t h = 2166136261u;
	const char *p = s;

	if (*p == '/')
		p++;
	for (; *p != '\0' && *p != '/' && *p != '\\'; p++) {
		h ^= (unsigned char)*p;
		h *= 16777619u;
	}
	return h;
}

/*
 * Which bucket an ENTRY belongs to. Separator-terminated rules
 * (prefix rules) bucket by their own first component; relative
 * entries and the bare root rule go to the wildcard bucket
 * (PATH_BUCKETS).
 */
static int entry_bucket(const char *entry)
{
	size_t elen = retrace_real_impls.strlen(entry);
	char last;

	if (elen == 0 || entry[0] != '/')
		return PATH_BUCKETS;
	last = entry[elen - 1];
	if ((last == '/' || last == '\\') && elen == 1)
		return PATH_BUCKETS;	/* the root rule */
	return (int)(first_comp_hash(entry) % PATH_BUCKETS);
}

static struct path_set *path_set_build(JSON_Array *list)
{
	size_t i, n = json_array_get_count(list);
	size_t counts[PATH_BUCKETS + 1] = {0};
	size_t cursor[PATH_BUCKETS + 1];
	struct path_set *set;
	size_t need = sizeof(struct path_set) +
		(PATH_BUCKETS + n) * sizeof(size_t);

	set = retrace_real_impls.malloc != NULL ?
		retrace_real_impls.malloc(need) : malloc(need);
	if (set == NULL)
		return NULL;
	set->count = n;
	set->first = n > 0 ? json_array_get_string(list, 0) : NULL;
	for (i = 0; i < n; i++) {
		const char *e = json_array_get_string(list, i);

		counts[e != NULL ?
			entry_bucket(e) : PATH_BUCKETS]++;
	}
	{
		size_t off = 0;

		for (i = 0; i <= PATH_BUCKETS; i++) {
			set->off[i] = off;
			cursor[i] = off;
			off += counts[i];
		}
		set->off[PATH_BUCKETS + 1] = off;
	}
	for (i = 0; i < n; i++) {
		const char *e = json_array_get_string(list, i);

		set->idx[cursor[e != NULL ?
			entry_bucket(e) : PATH_BUCKETS]++] = i;
	}
	return set;
}

static int list_contains(const char *path_arg, JSON_Array *list)
{
	struct path_set *set = NULL;
	size_t i, n;
	size_t *idx;
	int buckets[2];
	int k;

	/* registry hit, validated against the live array */
	n = json_array_get_count(list);
	for (i = 0; i < PATH_SETS_MAX; i++) {
		if (g_path_sets[i].arr == list &&
		    g_path_sets[i].set != NULL &&
		    g_path_sets[i].set->count == n &&
		    g_path_sets[i].set->first ==
			    (n > 0 ? json_array_get_string(list, 0)
				   : NULL)) {
			set = g_path_sets[i].set;
			break;
		}
	}
	if (set == NULL) {
		int slot = g_path_set_next;

		set = path_set_build(list);
		if (set == NULL) {
			/* OOM: the honest fallback is the old walk */
			size_t j;

			for (j = 0; j < n; j++) {
				const char *e =
					json_array_get_string(list, j);

				if (e != NULL &&
				    path_matches(path_arg, e))
					return 1;
			}
			return 0;
		}
		free(g_path_sets[slot].set);
		g_path_sets[slot].arr = list;
		g_path_sets[slot].set = set;
		g_path_set_next = (slot + 1) % PATH_SETS_MAX;
	}

	/* the caller's own bucket + the wildcard bucket */
	idx = set->idx;
	buckets[0] = (int)(first_comp_hash(path_arg) % PATH_BUCKETS);
	buckets[1] = PATH_BUCKETS;
	for (k = 0; k < 2; k++) {
		size_t lo = set->off[buckets[k]];
		size_t hi = set->off[buckets[k] + 1];

		for (i = lo; i < hi; i++) {
			const char *e =
				json_array_get_string(list, idx[i]);

			if (e != NULL && path_matches(path_arg, e))
				return 1;
		}
	}
	return 0;
}

static intptr_t deny_ret(const struct ThreadContext *t_ctx)
{
	if (t_ctx->prototype != NULL &&
	    retrace_real_impls.strcmp(
		    t_ctx->prototype->type_name, "ptr") == 0)
		return 0;
	return -1;
}

/* Unambiguous write-class functions (name-keyed; mode/flags are
 * checked separately for fopen/open)
 */
static const char *const g_write_funcs[] = {
	"unlink", "unlinkat", "rename", "renameat", "renameat2",
	"remove", "link", "linkat", "symlink", "symlinkat",
	"mkdir", "mkdirat", "rmdir",
	"truncate", "ftruncate", "creat",
	"chmod", "fchmod", "fchmodat", "chown", "lchown", "fchownat",
	"NtWriteFile", NULL
};

static int is_write_func(const char *name)
{
	size_t i;
	char c0 = name[0];

	/* first-char reject: most calls miss on the first compare */
	for (i = 0; g_write_funcs[i] != NULL; i++) {
		if (g_write_funcs[i][0] == c0 &&
		    retrace_real_impls.strcmp(name, g_write_funcs[i])
			    == 0)
			return 1;
	}
	return 0;
}

/*
 * Is THIS call write-class? fopen: the mode string carries
 * w/a/+; open/openat: the low flags bits (O_ACCMODE == 3 --
 * 1 = O_WRONLY, 2 = O_RDWR; same values on POSIX and Windows).
 */
static int call_is_write(const struct ThreadContext *t_ctx)
{
	const char *name = t_ctx->prototype->name;

	if (is_write_func(name))
		return 1;
	if (retrace_real_impls.strcmp(name, "fopen") == 0 &&
	    t_ctx->params_cnt > 1) {
		const struct FuncParam *mode = &t_ctx->params[1];

		if ((mode->param_meta.modifiers & CDM_POINTER) &&
		    mode->val != 0) {
			const char *m = (const char *)mode->val;

			for (; *m != '\0'; m++) {
				if (*m == 'w' || *m == 'a' || *m == '+')
					return 1;
			}
		}
		return 0;
	}
	if ((retrace_real_impls.strcmp(name, "open") == 0 ||
	     retrace_real_impls.strcmp(name, "openat") == 0) &&
	    t_ctx->params_cnt > 1) {
		long flags = (long)t_ctx->params[1].val & 3;

		return flags == 1 || flags == 2;
	}
	return 0;
}

static int classes_deny(const struct ThreadContext *t_ctx,
			JSON_Array *deny_classes)
{
	size_t i, n = json_array_get_count(deny_classes);

	if (!call_is_write(t_ctx))
		return 0;
	for (i = 0; i < n; i++) {
		const char *c = json_array_get_string(deny_classes, i);

		if (c != NULL &&
		    retrace_real_impls.strcmp(c, "write") == 0)
			return 1;
	}
	return 0;
}

static const char *const g_env_funcs[] = {
	"getenv", "setenv", "putenv", "unsetenv", "_putenv", NULL
};

static int is_env_func(const char *name)
{
	size_t i;

	for (i = 0; g_env_funcs[i] != NULL; i++) {
		if (retrace_real_impls.strcmp(name, g_env_funcs[i])
			== 0)
			return 1;
	}
	return 0;
}

static int deny(struct ThreadContext *t_ctx, const char *why,
		const char *arg)
{
	log_warn("sandbox: DENIED '%s' (%s)", arg, why);

	/* TODO.trace-profile/32: the denial is ALSO a live OTLP LOG
	 * record (ERROR) so security teams watch policy violations
	 * in their collector during detonations. Attributes follow
	 * the documented retrace.jail.* schema (docs/reports.md).
	 * No-op when RETRACE_OTLP_ENDPOINT is unset; bounded drop,
	 * never blocks the denied call's return path.
	 */
	{
		const char *func = t_ctx->prototype != NULL ?
			t_ctx->prototype->name : "unknown";
		struct retrace_otlp_event_attr ev[3];

		ev[0].key = "retrace.jail.path";
		ev[0].str_val = arg;
		ev[0].int_val = 0;
		ev[1].key = "retrace.jail.reason";
		ev[1].str_val = why;
		ev[1].int_val = 0;
		ev[2].key = "retrace.jail.func";
		ev[2].str_val = func;
		ev[2].int_val = 0;
		(void)retrace_otlp_live_emit_event(
			RETRACE_OTLP_SEV_ERROR, "retrace.jail.denied",
			ev, 3);

		/* The supervisor agent's fan-out (TODO.supervisor/03):
		 * same event, the control-bus subscriber. String pairs
		 * (the wire shape is the protocol's attrs object).
		 */
		{
			const char *kv[6] = {
				"retrace.jail.path", arg,
				"retrace.jail.reason", why,
				"retrace.jail.func", func,
			};

			(void)retrace_agent_emit_event(
				"retrace.jail.denied", kv, 3);
		}
	}

	errno = EACCES;
	t_ctx->ret_val = deny_ret(t_ctx);
	return -1;
}

/*
 * Deception: rewrite the path param to decoy_dir/<basename> and
 * let call_real run against the decoy. Owns the new string
 * (free_val) so cleanup releases it.
 */
static int decoy(struct ThreadContext *t_ctx, int arg_idx,
		 const char *path_arg, const char *decoy_dir)
{
	const char *base = path_arg;
	const char *scan;
	char *fake;
	size_t dlen = retrace_real_impls.strlen(decoy_dir);

	for (scan = path_arg; *scan != '\0'; scan++) {
		if (*scan == '/' || *scan == '\\')
			base = scan + 1;
	}
	fake = (char *)retrace_real_impls.malloc(
		dlen + retrace_real_impls.strlen(base) + 2);
	if (fake == NULL)
		return 0; /* fall through to plain denial */
	/* real_snprintf, not strcpy: macOS macro-substitutes plain
	 * strcpy into __builtin___strcpy_chk (the v2.11.2 lesson)
	 */
	retrace_real_impls.real_snprintf(fake,
		dlen + retrace_real_impls.strlen(base) + 2,
		"%s/%s", decoy_dir, base);

	t_ctx->params[arg_idx].val = (intptr_t)fake;
	t_ctx->params[arg_idx].free_val = 1;
	log_warn("sandbox: DECOYED '%s' -> '%s'", path_arg, fake);
	return 1;
}

static int ia_sandbox(struct ThreadContext *t_ctx,
		      const JSON_Object *action_params)
{
	JSON_Array *deny_paths, *allow_paths;
	JSON_Array *deny_classes, *allow_env, *deny_env;
	const char *decoy_dir;
	const char *path_arg = NULL;
	int arg_idx = -1;
	int path_idx = -1;

	if (action_params == NULL) {
		log_err("sandbox: action_params required");
		t_ctx->ret_val = -1;
		return -1;
	}

	deny_paths = json_object_get_array(action_params, "deny_paths");
	allow_paths = json_object_get_array(action_params, "allow_paths");
	deny_classes = json_object_get_array(action_params,
		"deny_classes");
	allow_env = json_object_get_array(action_params, "allow_env");
	deny_env = json_object_get_array(action_params, "deny_env");
	decoy_dir = json_object_get_string(action_params, "decoy_dir");
	if (deny_paths == NULL && allow_paths == NULL &&
	    deny_classes == NULL && allow_env == NULL &&
	    deny_env == NULL) {
		log_err("sandbox: a policy array is required");
		t_ctx->ret_val = -1;
		return -1;
	}

	/*
	 * Find the string argument via prototype metadata: the first
	 * string param among the first two args (open: path; openat:
	 * dirfd, path; getenv: name). Non-pointer params are skipped
	 * -- comparing those as strings would dereference garbage.
	 */
	for (arg_idx = 0; arg_idx < t_ctx->params_cnt && arg_idx < 2; arg_idx++) {
		const struct ParamMeta *pm =
			&t_ctx->params[arg_idx].param_meta;

		if ((pm->modifiers & CDM_POINTER) &&
		    t_ctx->params[arg_idx].val != 0 &&
		    retrace_real_impls.strncmp(pm->ref_type_name,
			"sz", 3) == 0) {
			path_arg = (const char *)t_ctx->params[arg_idx].val;
			path_idx = arg_idx;
			break;
		}
	}

	/* Class policy first: read-only detonation denies ANY write
	 * regardless of path
	 */
	if (deny_classes != NULL &&
	    classes_deny(t_ctx, deny_classes))
		return deny(t_ctx, "write-class", path_arg != NULL ?
			path_arg : t_ctx->prototype->name);

	/* Env policy: police env NAME reads/writes first-class */
	if (allow_env != NULL || deny_env != NULL) {
		if (is_env_func(t_ctx->prototype->name) &&
		    path_arg != NULL) {
			if (deny_env != NULL &&
			    list_contains(path_arg, deny_env))
				return deny(t_ctx, "deny_env", path_arg);
			if (allow_env != NULL &&
			    !list_contains(path_arg, allow_env))
				return deny(t_ctx, "not in allow_env",
					path_arg);
		}
	}

	/*
	 * No path argument (non-file function under a wildcard
	 * script): nothing to police, let the call through.
	 */
	if (path_arg == NULL)
		return 0;

	/*
	 * Denial return value is PROTOTYPE-DRIVEN (TODO 17): a
	 * synthesized -1 for a pointer-returning function hands the
	 * caller (FILE *)-1 -- != NULL, so correct code USES it and
	 * crashes. Pointer returns deny with NULL; int returns with
	 * -1 (POSIX open-family error).
	 */
	if (allow_paths != NULL && !list_contains(path_arg, allow_paths)) {
		/* deception: reads may be redirected to a decoy */
		if (decoy_dir != NULL && !call_is_write(t_ctx) &&
		    decoy(t_ctx, path_idx, path_arg, decoy_dir))
			return 0;
		return deny(t_ctx, "not in allow_paths", path_arg);
	}
	if (deny_paths != NULL && list_contains(path_arg, deny_paths)) {
		if (decoy_dir != NULL && !call_is_write(t_ctx) &&
		    decoy(t_ctx, path_idx, path_arg, decoy_dir))
			return 0;
		return deny(t_ctx, "matches deny_paths", path_arg);
	}

	return 0;
}

retrace_actions_define_package(sandbox) = {
	{
		.name = "sandbox",
		.action = ia_sandbox
	}
};
