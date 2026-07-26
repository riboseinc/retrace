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
 * retrace backend interface — see the v2 plan.md and
 * ADR-0003-plugin-pattern-for-backends.md.
 *
 * A backend owns one (OS, arch) pair's interposition mechanism. The engine
 * calls retrace_backend_spawn() to install retrace into a target process;
 * the backend's trampoline (per-arch assembly) calls back into
 * retrace_engine_dispatch() on each intercepted function call.
 *
 * Adding a new backend is purely additive: new directory under
 * src/backends/, one retrace_backend_register() call in its constructor.
 * Engine code never changes.
 */

#ifndef RETRACE_BACKEND_H
#define RETRACE_BACKEND_H

#include <stddef.h>
#include <sys/types.h>

#include <retrace/version.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32) && defined(RETRACE_SHARED)
#  define RETRACE_BACKEND_API __declspec(dllexport)
#elif defined(__GNUC__) && (__GNUC__ >= 4)
#  define RETRACE_BACKEND_API __attribute__((visibility("default")))
#else
#  define RETRACE_BACKEND_API
#endif

/* Opaque engine handle — defined in <retrace/retrace.h>. */
struct retrace_engine;

/* Opaque thread context — defined internally per arch. */
struct retrace_thread_context;

/* Backend rank controls selection when multiple backends probe successfully. */
typedef enum {
	RETRACE_BACKEND_RANK_PREFERRED = 0,
	RETRACE_BACKEND_RANK_FALLBACK  = 1,
} retrace_backend_rank_t;

/* Status codes returned by backend methods. */
typedef enum {
	RETRACE_BACKEND_OK            =  0,
	RETRACE_BACKEND_UNSUPPORTED   = -1,
	RETRACE_BACKEND_NOT_FOUND     = -2,
	RETRACE_BACKEND_PERMISSION    = -3,
	RETRACE_BACKEND_INTERNAL      = -99,
} retrace_backend_status_t;

/*
 * The backend interface. Each backend publishes a const instance of this
 * struct (e.g. `const retrace_backend_t preload_elf_backend = {...}`) and
 * registers it via retrace_backend_register() in a constructor.
 */
typedef struct retrace_backend {
	/* Stable identifier: "preload-elf", "preload-macho", "preload-bsd",
	 * "preload-msvc", "ptrace", ... */
	const char *name;

	/* Human-readable single-line description for `retrace backends list`. */
	const char *description;

	/* Selection priority. PREFERRED backends win over FALLBACK at equal
	 * probe() success. */
	retrace_backend_rank_t rank;

	/* Probe whether this backend can trace a given target on this host.
	 * Returns 1 if it can, 0 if not, negative on error. Called by the
	 * registry's select logic — must be cheap (no I/O). */
	int (*probe)(struct retrace_engine *eng, const char *target_path);

	/* Spawn target_path with retrace already installed. Returns child PID
	 * on success, negative on error. argv and envp are NULL-terminated. */
	pid_t (*spawn)(struct retrace_engine *eng,
	               const char *target_path,
	               char *const argv[],
	               char *const envp[]);

	/* Attach to an already-running target (optional — LD_PRELOAD backends
	 * can't, ptrace can). NULL if unsupported. */
	pid_t (*attach)(struct retrace_engine *eng, pid_t target_pid);

	/* Detach / uninstall. NULL if unsupported. */
	int (*detach)(struct retrace_engine *eng);

	/* Per-call hook called by the trampoline. Backend translates its
	 * native frame representation into the engine's thread context.
	 * Required: every backend's trampoline calls this. */
	void (*translate_frame)(struct retrace_thread_context *ctx,
	                         void *native_frame);
} retrace_backend_t;

/* Registration — backends call this from their constructor. Idempotent
 * for the same backend pointer; safe to call multiple times. */
RETRACE_BACKEND_API int retrace_backend_register(const retrace_backend_t *backend);

/* Iterate registered backends. Returns count; caller frees nothing
 * (backends are static const in the binary). */
RETRACE_BACKEND_API size_t retrace_backend_list(const retrace_backend_t ***out);

/* Select a backend for a given target. If requested_name is non-NULL,
 * only that backend is considered. Otherwise probes all registered
 * backends and returns the highest-rank one that returns 1 from
 * probe(). Returns NULL if none match. */
RETRACE_BACKEND_API const retrace_backend_t *retrace_backend_select(
	struct retrace_engine *eng,
	const char *target_path,
	const char *requested_name);

/* Find a backend by name (no probing). Returns NULL if not registered. */
RETRACE_BACKEND_API const retrace_backend_t *retrace_backend_find(const char *name);

#ifdef __cplusplus
}
#endif

#endif /* RETRACE_BACKEND_H */
