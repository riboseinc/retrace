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

/*
 * printf_compat.h carries the portable fallbacks (musl path);
 * MSVC/clang-cl compile without GNU extensions.
 */
#if !defined(__GNUC__) && !defined(_MSC_VER) && \
	!defined(__clang__)
#error GNU extensions are required!
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <string.h>

#include "win_diag.h"

#include <stdint.h>

#if !defined(__STDC_NO_ATOMICS__)
#include <stdatomic.h>
#endif

#include "engine.h"
#include "funcs.h"
#include "agent.h"
#include "real_impls.h"
#include "arch_spec.h"
#include "actions.h"
#include "conf.h"
#include "logger.h"
#include "thread_context.h"
#include "reentrance_guard.h"
#include "script_resolver.h"
#include "call_hash.h"
#include "action_runner.h"

/*
 * The central dispatch entry point invoked by every per-arch asm
 * trampoline. Responsibilities, in strict order:
 *
 *   1. Look up the real libc implementation (no allocation here — this
 *      runs before init and during reentry).
 *   2. If init never completed, schedule the real call and return.
 *   3. Acquire the per-thread interception context.
 *   4. If real_impl is NULL, abort the call with ret_val = -1.
 *   5. Default to calling the real implementation.
 *   6. Reentrance guard: if thread_ctx already holds a real_impl,
 *      we're inside a nested call — bail.
 *   7. Mark this thread as actively intercepting.
 *   8. Look up the prototype; if missing, just call real.
 *   9. Apply the per-function log filter (issue #486).
 *  10. Parse variadic args out of the frame (or bail for FP varargs).
 *  11. Find the intercept_script; if none, call real.
 *  12. Hand off to action_runner to actually run the script.
 *  13. Write thread_ctx->ret_val back into the frame.
 *  14. Clear the per-thread context.
 *
 * Steps 3, 11, 12 live in their own modules (ADR-0013); this function
 * is the orchestrator that calls them in order.
 */
/*
 * macOS SDKs with _DARWIN_C_SOURCE remap some libc names (fopen)
 * to $DARWIN_EXTSN variants in optimized builds. The Mach-O
 * backends interpose the variant symbols too; stripping the
 * suffix here makes every downstream lookup -- prototype,
 * config script, real-impl resolution, log output -- see the
 * clean name. Single normalization point (SSOT).
 *
 * Deliberately libc-free (manual loops): a plain strlen/strcmp
 * here is an interposed call -- the wrapper would re-enter the
 * engine and recurse (every preload test died to this on Linux).
 */
static char *strip_darwin_extsn(char *name, char *buf, size_t bufsz)
{
	static const char suffix[] = "$DARWIN_EXTSN";
	const size_t slen = sizeof(suffix) - 1;
	size_t n = 0;
	size_t i;

	while (name[n] != '\0')
		n++;
	if (n <= slen)
		return name;
	for (i = 0; i < slen; i++) {
		if (name[n - slen + i] != suffix[i])
			return name;
	}
	if (n - slen >= bufsz)
		return name;
	for (i = 0; i < n - slen; i++)
		buf[i] = name[i];
	buf[n - slen] = '\0';
	return buf;
}

/*
 * real-impl cache: name-hash keyed, open addressing. The
 * historical entry dlsym'd EVERY dispatch -- the loader lock
 * plus an ELF lookup per interposed call, dominating the
 * per-call budget. This table resolves each function exactly
 * once for the process lifetime.
 *
 * THE LAW (the engine-entry order, restated): everything
 * between entry and the reentrance-guard check must be
 * dispatch-free -- only member calls (retrace_real_impls.*)
 * and pure code. This cache obeys it: hashing is arithmetic,
 * comparison is the member strcmp. The miss path resolves via
 * the allocator law below (free/malloc take the init-resolved
 * members; everything else dlsyms once). func_get and the rest
 * of the historical entry stay BELOW the guard exactly as
 * they were.
 */
#define REAL_CACHE_SLOTS 2048	/* pow2 */

/*
 * MSVC without C11 atomics: an aligned uint64_t store/load is
 * atomic on every supported arch (the config_cache.c precedent)
 */
#if defined(__STDC_NO_ATOMICS__)
struct real_cache_el {
	volatile uint64_t hash;
	void *real;
	char name[MAXLEN_FUNC_NAME];
	const struct FuncPrototype *proto;
};

#define real_hash_load(p)	(*(p))
#define real_hash_store(p, v)	(*(p) = (v))
#else
struct real_cache_el {
	_Atomic(uint64_t) hash;
	void *real;
	char name[MAXLEN_FUNC_NAME];
	const struct FuncPrototype *proto;
};

#define real_hash_load(p)	atomic_load_explicit((p), \
	memory_order_acquire)
#define real_hash_store(p, v)	atomic_store_explicit((p), (v), \
	memory_order_release)
#endif

struct FuncPrototype;

static struct real_cache_el real_cache[REAL_CACHE_SLOTS];

static uint64_t real_name_hash(const char *s)
{
	uint64_t h = 14695981039346656037ULL;

	for (; *s != '\0'; s++) {
		h ^= (unsigned char)*s;
		h *= 1099511628211ULL;
	}
	return h;
}

/*
 * Miss-path resolution. The ALLOCATOR law (the rounds-12/13
 * recursion): glibc's dlsym frees its per-thread dlerror
 * result through the PLT, that free re-enters the engine, and
 * the dlsym would recurse unbounded -- free/malloc take the
 * init-resolved members instead.
 */
void *retrace_real_impl_resolve(const char *func_name)
{
	if (retrace_real_impls.strcmp != NULL &&
	    retrace_real_impls.free != NULL &&
	    retrace_real_impls.strcmp(func_name, "free") == 0)
		return retrace_real_impls.free;
	if (retrace_real_impls.strcmp != NULL &&
	    retrace_real_impls.malloc != NULL &&
	    retrace_real_impls.strcmp(func_name, "malloc") == 0)
		return retrace_real_impls.malloc;
	return retrace_as_get_real_safe(func_name);
}

void *retrace_real_impl_cached(const char *func_name)
{
	/*
	 * Pre-init dispatches take the HISTORICAL path: they are
	 * construction-internal, single-threaded, and carry no
	 * dlerror state -- dlsym is safe exactly there and only
	 * there. The cache (and its member free/malloc miss law)
	 * operates post-init, when real_impls is fully resolved.
	 */
	uint64_t h;
	size_t i;
	struct real_cache_el *el;
	uint64_t k;

	if (!retrace_inited)
		return retrace_as_get_real_safe(func_name);
	h = real_name_hash(func_name);
	i = (size_t)h & (REAL_CACHE_SLOTS - 1);

	for (;;) {
		el = &real_cache[i & (REAL_CACHE_SLOTS - 1)];
		k = real_hash_load(&el->hash);
		if (k == h && el->name != NULL &&
		    retrace_real_impls.strcmp(el->name, func_name)
			    == 0)
			return el->real;
		if (k == 0) {
			void *real = retrace_real_impl_resolve(func_name);
			size_t n = 0;

			/* COPY the name -- never store the caller's
			 * pointer: on macOS strip_darwin_extsn hands
			 * back a buffer on the engine frame, dead
			 * the moment this returns. Manual loop: a
			 * libc copy here would itself dispatch.
			 */
			while (func_name[n] != '\0' &&
			       n + 1 < sizeof(el->name)) {
				el->name[n] = func_name[n];
				n++;
			}
			el->name[n] = '\0';
			el->real = real;
			/* one index, both answers -- with one trap:
			 * retrace_inited flips BEFORE funcs_init
			 * registers the prototype table, so a
			 * construction-window dispatch of a name
			 * (parson opens the config) would freeze
			 * proto=NULL forever. NULL therefore means
			 * not-yet-resolved: proto_cached re-walks
			 * and self-heals the entry.
			 */
			el->proto = NULL;
			real_hash_store(&el->hash, h);
			return real;
		}
		i++;
	}
}

/*
 * The dispatch tail's prototype lookup, served from the same
 * name->slot index the real-impl pointer comes from: one probe
 * per dispatch answers both. Only runs post-init (after the
 * guard), where the entry was just inserted by the real-impl
 * lookup earlier in this same dispatch -- the fallback covers
 * ordering drift, never inserts.
 */
const struct FuncPrototype *retrace_proto_cached(
	const char *func_name)
{
	uint64_t h;
	size_t i;
	struct real_cache_el *el;
	uint64_t k;

	if (!retrace_inited)
		return retrace_func_get(func_name);
	h = real_name_hash(func_name);
	i = (size_t)h & (REAL_CACHE_SLOTS - 1);

	for (;;) {
		el = &real_cache[i & (REAL_CACHE_SLOTS - 1)];
		k = real_hash_load(&el->hash);
		if (k == h && el->name != NULL &&
		    retrace_real_impls.strcmp(el->name, func_name)
			    == 0) {
			if (el->proto != NULL)
				return el->proto;
			/* filled post-funcs_init on the first
			 * query that needs it; benign racing
			 * (same value stored twice)
			 */
			el->proto = retrace_func_get(func_name);
			return el->proto;
		}
		if (k == 0)
			return retrace_func_get(func_name);
		i++;
	}
}

void retrace_engine_wrapper(char *func_name,
	void *arch_spec_ctx)
{
	void *real_impl;
	struct ThreadContext *thread_ctx;
	const JSON_Object *i_script;
	const JSON_Array *i_scripts;
	char clean_name[64]; /* MAXLEN_FUNC_NAME; funcs.h not included here */

	func_name = strip_darwin_extsn(func_name, clean_name,
		sizeof(clean_name));

	real_impl = retrace_real_impl_cached(func_name);
	if (!retrace_inited) {
		retrace_as_sched_real(arch_spec_ctx, real_impl);
		return;
	}
	retrace_win_diag("enter", func_name, 0);

	thread_ctx = retrace_thread_context_get();
	if (thread_ctx == NULL) {
		log_err(
			"%s() intercept failed - could not get context",
			func_name);
		return;
	}
	retrace_win_diag("ctx", func_name, 0);

	if (real_impl == NULL) {
		/* cannot process
		 * abort call with return val -1,
		 * -1 is chosen for the best chance of
		 * signaling an error for CRT funcs
		 * The caller will probably crash anyway...
		 */
		retrace_as_set_ret_val(arch_spec_ctx, -1);
		return;
	}

	/* set default to call real impl */
	retrace_as_sched_real(arch_spec_ctx, real_impl);

	/* Reentrance guard: nested libc calls from inside an action
	 * would recurse unbounded. Bail if the current thread is
	 * already inside an intercept (reentrance_guard.c).
	 */
	if (retrace_reentrance_guard_active(thread_ctx)) {
		retrace_win_diag("guard-bail", func_name, 0);
		return;
	}

	retrace_reentrance_guard_enter(thread_ctx, real_impl,
		arch_spec_ctx);

	/* first dispatch = the constructor provably finished. The
	 * kick ALSO sits AFTER guard-enter: its dlsym resolutions
	 * nest dispatches (glibc's dlsym calls free through the
	 * PLT), and only the active guard bounds that nesting --
	 * before guard-enter the cycle ran unbroken and overflowed
	 * the stack (the macOS lldb and Linux gdb signatures).
	 */
	retrace_agent_kick();

	thread_ctx->prototype = retrace_proto_cached(func_name);
	retrace_win_diag("proto", func_name,
		thread_ctx->prototype != NULL);

	/* Coverage feedback for libFuzzer/AFL integration (TODO.complete/24).
	 * Cheap (FNV-1a over the func name + arg count) and gated by env,
	 * so zero overhead when disabled. Updates the per-thread rolling
	 * hash; the destructor surfaces the final hash to stderr.
	 */
	if (retrace_call_hash_enabled())
		retrace_call_hash_observe(func_name,
			thread_ctx->prototype
				? thread_ctx->prototype->params_cnt
				: 0);

	/* if func is not prototyped - do not intervene */
	if (thread_ctx->prototype == NULL)
		goto clean_up;

	/*
	 * Per-function log filter (issue #486). If the user excluded
	 * this function (or didn't include it in the allowlist), skip
	 * all action processing. The real call still runs via
	 * call_real_flag (already set by retrace_as_sched_real above).
	 */
	if (!retrace_logger_func_loggable(func_name)) {
		log_dbg("func '%s' filtered -- skip actions", func_name);
		goto clean_up;
	}

	/* setup params, do not proceed if failed since
	 * it can be dangerous to call orig with partial params
	 */
	thread_ctx->params_cnt = ENGINE_MAXCOUNT_PARAMS;
	if (!retrace_as_setup_params(thread_ctx->arch_spec_ctx,
		thread_ctx->prototype,
		thread_ctx->params,
		&thread_ctx->params_cnt)) {
		log_err(
			"failed to setup params for %s(), will not proceed",
			func_name);
		goto clean_up;
	}
	retrace_win_diag("params", func_name, thread_ctx->params_cnt);

	/* find intercept script for the func and return addr */
	i_scripts = json_object_get_array(retrace_conf, "intercept_scripts");
	if (!i_scripts) {
		log_warn(
			"%s() config does not contain intercept_scripts",
			func_name);
		goto clean_up;
	}

	i_script = retrace_script_find(i_scripts, func_name,
		thread_ctx->ret_addr);

	if (!i_script) {
		/* no script defined for this func, call real */
		retrace_win_diag("no-script", func_name, 0);
		goto clean_up;
	}
	retrace_win_diag("script", func_name, 0);

	/* we have script, do not call real impl by default */
	retrace_as_cancel_sched_real(arch_spec_ctx);

	retrace_win_diag("actions", func_name, 0);
	retrace_action_runner_run(thread_ctx, func_name, i_script);
	retrace_win_diag("actions-done", func_name, thread_ctx->ret_val);

	/* write back to arch spec. portion */
	retrace_as_set_ret_val(arch_spec_ctx, thread_ctx->ret_val);

clean_up:
	/* mark hi-level intercept done */
	retrace_win_diag("clean", func_name, 0);
	retrace_thread_context_clear(thread_ctx);
}

int retrace_engine_init(void)
{
	return retrace_thread_context_init();
}
