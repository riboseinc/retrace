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
 * retrace public API.
 *
 * Every function declared here is implemented and exported by the
 * library. test/unit/test_public_api.c enforces that contract at
 * build time (it dlsyms each symbol and fails the build if any is
 * missing).
 *
 * A larger engine/script-builder/config scaffold was declared here
 * from the v2.0.0 modernization plan but never implemented; the
 * declarations were removed in v2.5.0 because a header that
 * declares unlinked symbols breaks consumers at link time. See
 * docs/adr/0014-public-api-matches-implementation.md for the
 * decision and the re-introduction path.
 *
 * Design constraints (see docs/adr/0008-opaque-public-types-for-abi.md):
 *   - All types are opaque handles. Struct definitions are internal.
 *   - All functions return int (0 on success, negative on error).
 *   - All output is caller-allocated with a size parameter, or returned
 *     as a `const char *` owned by the engine.
 */

#ifndef RETRACE_RETRACE_H
#define RETRACE_RETRACE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Export/visibility. */
#if defined(_WIN32) && defined(RETRACE_SHARED)
#  define RETRACE_API __declspec(dllexport)
#elif defined(_WIN32) && defined(RETRACE_STATIC)
#  define RETRACE_API
#elif defined(__GNUC__) && (__GNUC__ >= 4)
#  define RETRACE_API __attribute__((visibility("default")))
#  define RETRACE_INTERNAL __attribute__((visibility("hidden")))
#else
#  define RETRACE_API
#  define RETRACE_INTERNAL
#endif

/* Status codes for public APIs. */
typedef enum {
	RETRACE_OK                 =  0,
	RETRACE_ERR_NOMEM          = -1,
	RETRACE_ERR_INVAL          = -2,
	RETRACE_ERR_NOENT          = -3,
	RETRACE_ERR_FORMAT         = -4,
	RETRACE_ERR_RANGE          = -5,
	RETRACE_ERR_UNSUPPORTED    = -6,
	RETRACE_ERR_PERMISSION     = -7,
	RETRACE_ERR_INTERNAL       = -99,
} retrace_status_t;

/* ----------------------------------------------------------------- *
 * Version
 *
 * Compile-time: the RETRACE_VERSION_* macros (include/retrace/version.h)
 * and RETRACE_VERSION_ATLEAST(maj, min, pat). Runtime: the two
 * functions below, both returning library-owned static strings.
 */
RETRACE_API const char *retrace_version(void);
RETRACE_API const char *retrace_version_info(void);

/* ----------------------------------------------------------------- *
 * Process attach
 *
 * Attach to an already-running process and trace its syscalls until
 * it exits. This is the ptrace path: unlike the preload backends it
 * needs no control of the process at exec time, so it works for
 * targets LD_PRELOAD cannot reach (any running PID; also the only
 * native option for static binaries after they started).
 *
 * Observation-oriented: the trace loop forwards each syscall to the
 * engine, applying the JSON config like any other trace source.
 *
 * Requires the process-global engine (initialized by the library
 * constructor on dlopen/load); no engine handle is needed.
 *
 * Linux only. Returns:
 *   RETRACE_OK              trace loop ran to completion (target exited)
 *   RETRACE_ERR_NOENT       ptrace backend not registered
 *   RETRACE_ERR_PERMISSION  PTRACE_ATTACH denied (ptrace_scope / creds)
 *   RETRACE_ERR_INVAL       pid <= 0
 *   RETRACE_ERR_UNSUPPORTED non-Linux platform
 */
RETRACE_API retrace_status_t retrace_attach_process(int pid);

/*
 * Enumerate registered backend names (e.g. "preload_elf", "ptrace").
 * The array and its strings are owned by the library and remain valid
 * until the next call; callers must not free them.
 */
RETRACE_API retrace_status_t retrace_list_backends(
		const char *const **out_names, size_t *out_count);

/*
 * Registry introspection: enumerate every interceptable libc
 * function (the prototype registry) and every built-in action.
 * Same ownership contract as retrace_list_backends: the array
 * and strings are library-owned, valid until the next call to
 * the same function.
 */
RETRACE_API retrace_status_t retrace_list_functions(
		const char *const **out_names, size_t *out_count);
RETRACE_API retrace_status_t retrace_list_actions(
		const char *const **out_names, size_t *out_count);

#ifdef __cplusplus
}
#endif

#endif /* RETRACE_RETRACE_H */
