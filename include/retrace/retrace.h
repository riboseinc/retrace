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
 * retrace public API — see the modernization plan.md
 *
 * Design constraints (see docs/adr/0008-opaque-public-types-for-abi.md):
 *   - All types are opaque handles. Struct definitions are internal.
 *   - All functions return int (0 on success, negative on error).
 *   - All output is caller-allocated with a size parameter, or returned as
 *     a `const char *` owned by the engine.
 *   - ABI-stable from v2.0.0.
 */

#ifndef RETRACE_RETRACE_H
#define RETRACE_RETRACE_H

#include <stddef.h>
#include <stdint.h>

#include <retrace/version.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Visibility annotation. Public symbols are tagged RETRACE_API; everything
 * else is hidden by default (set via CMAKE_C_VISIBILITY_PRESET=hidden).
 */
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

/* Opaque handles — definitions live in src/core/internal/. */
typedef struct retrace_engine         retrace_engine_t;
typedef struct retrace_script         retrace_script_t;
typedef struct retrace_intercept_rule retrace_intercept_rule_t;
typedef struct retrace_action_params  retrace_action_params_t;

/* Result of an action callback. Drives engine dispatch. */
typedef enum {
	RETRACE_ACTION_OK          =  0,
	RETRACE_ACTION_SKIP_CALL   =  1,
	RETRACE_ACTION_HANDLED     =  2,
	RETRACE_ACTION_ERROR       = -1,
} retrace_action_result_t;

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
 */
RETRACE_API const char *retrace_version(void);
RETRACE_API const char *retrace_version_info(void);

/* ----------------------------------------------------------------- *
 * Engine lifecycle
 *
 * The engine is the top-level owner of: the prototype registry, the action
 * registry, the backend handle, the active script, per-thread invocation
 * state, and the real-impl table (libc function pointers used internally
 * to avoid reentrancy).
 *
 * One engine per process is typical. Multi-engine is supported but each
 * engine has independent registries; prototypes are global (linker-section
 * scanned) so they are shared.
 */
RETRACE_API retrace_status_t retrace_engine_create(retrace_engine_t **out);
RETRACE_API retrace_status_t retrace_engine_destroy(retrace_engine_t *eng);

RETRACE_API retrace_status_t retrace_engine_set_script(
		retrace_engine_t *eng, retrace_script_t *script);
RETRACE_API retrace_status_t retrace_engine_get_script(
		retrace_engine_t *eng, const retrace_script_t **out);

RETRACE_API retrace_status_t retrace_engine_set_option(
		retrace_engine_t *eng,
		const char *key,
		const char *value);

/* ----------------------------------------------------------------- *
 * Backend selection
 *
 * Backends self-register at constructor time. The engine selects one by
 * probing each registered backend; `retrace_engine_select_backend` forces
 * a specific one. See include/retrace/backend.h (the modernization plan/03).
 */
RETRACE_API const char *const *retrace_engine_list_backends(
		retrace_engine_t *eng, size_t *count);
RETRACE_API retrace_status_t retrace_engine_select_backend(
		retrace_engine_t *eng, const char *name);

/* ----------------------------------------------------------------- *
 * Programmatic script builder
 *
 * Build a script in C without parsing a file. Equivalent to the JSON path
 * but with no serialization in between.
 */
RETRACE_API retrace_script_t *retrace_script_new(retrace_engine_t *eng);
RETRACE_API void              retrace_script_free(retrace_script_t *script);
RETRACE_API retrace_status_t  retrace_script_validate(
		retrace_script_t *script, char *err_buf, size_t err_len);

RETRACE_API retrace_status_t retrace_script_add_intercept(
		retrace_script_t *script,
		const char *func_glob,
		retrace_intercept_rule_t **out);

RETRACE_API retrace_action_params_t *retrace_action_params_new(void);
RETRACE_API void retrace_action_params_free(retrace_action_params_t *params);

RETRACE_API retrace_status_t retrace_action_params_set_int(
		retrace_action_params_t *params,
		const char *name, long long value);
RETRACE_API retrace_status_t retrace_action_params_set_double(
		retrace_action_params_t *params,
		const char *name, double value);
RETRACE_API retrace_status_t retrace_action_params_set_string(
		retrace_action_params_t *params,
		const char *name, const char *value);

RETRACE_API retrace_status_t retrace_intercept_rule_add_action(
		retrace_intercept_rule_t *rule,
		const char *action_name,
		retrace_action_params_t *params);

/* ----------------------------------------------------------------- *
 * Config parsing
 *
 * Delegates to the named config source (typically "json" or "text").
 * Sources self-register at constructor time. See the modernization plan/04.
 */
RETRACE_API retrace_status_t retrace_config_parse_file(
		retrace_engine_t *eng,
		const char *source_name,
		const char *path,
		retrace_script_t **out);

RETRACE_API retrace_status_t retrace_config_parse_buffer(
		retrace_engine_t *eng,
		const char *source_name,
		const char *buf, size_t len,
		retrace_script_t **out);

RETRACE_API const char *const *retrace_config_list_sources(
		retrace_engine_t *eng, size_t *count);

/* ----------------------------------------------------------------- *
 * Inspection
 *
 * Used by the CLI (`retrace prototypes list`, etc.) and by tooling that
 * wants to introspect the engine. Output arrays are NULL-terminated and
 * owned by the engine until the next call to the same function.
 */
RETRACE_API const char *const *retrace_engine_list_prototypes(
		retrace_engine_t *eng, size_t *count);
RETRACE_API const char *const *retrace_engine_list_actions(
		retrace_engine_t *eng, size_t *count);

RETRACE_API retrace_status_t retrace_engine_describe_prototype(
		retrace_engine_t *eng,
		const char *func_name,
		char **out_desc);  /* caller frees with retrace_free() */

RETRACE_API retrace_status_t retrace_engine_describe_action(
		retrace_engine_t *eng,
		const char *action_name,
		char **out_desc);

RETRACE_API void retrace_free(void *ptr);

/* ----------------------------------------------------------------- *
 * Error reporting
 *
 * The most recent error for the calling thread is held in thread-local
 * storage. `retrace_last_error` returns the message (owned by the engine
 * until the next call on the same thread).
 */
RETRACE_API const char *retrace_last_error(retrace_engine_t *eng);
RETRACE_API int         retrace_last_error_code(retrace_engine_t *eng);

#ifdef __cplusplus
}
#endif

#endif /* RETRACE_RETRACE_H */
