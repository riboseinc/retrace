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

#include <stdarg.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include "posix_compat.h"

#include "logger.h"
#include "real_impls.h"
#include "log_ring.h"
#include "log_flusher.h"

/* Forward declarations for the lazy-spawn function. */
static volatile int g_flusher_spawned; /* guarded by rc_cas */
static int logger_emit_entry(const struct LogEntry *entry, void *ctx);

/* Set in main.c's constructor when all init is complete. */
extern int retrace_inited;

/*
 * Ring enabled flag. Defaults to 1 (use the lock-free ring + flusher
 * post-init). Set to 0 by RETRACE_LOGGER_RING=0 env var, for platforms
 * where the background thread is unstable (OHOS Docker/QEMU).
 */
static int g_logger_ring_enabled;

/*
 * Lazy flusher spawn: on the first log push, atomically try to
 * spawn the flusher thread. Only one thread wins the race; the
 * others skip. This avoids spawning the thread during the dyld
 * constructor (which crashes on OHOS/musl under QEMU).
 */
static void ensure_flusher_running(void)
{
	if (rc_cas(&g_flusher_spawned, 0, 1)) {
		if (retrace_log_flusher_init(logger_emit_entry, NULL) != 0)
			log_err("logger: log_flusher_init failed");
	}
}

/*
 * MAXLEN_FUNC_NAME from funcs.h is 64. Don't include funcs.h here -- it
 * pulls in section macros that conflict with real_impls.h on Darwin.
 */
#define LOGGER_MAXLEN_FUNC_NAME 64

#define ENVAR_LOGGER_DEF_ENA "RETRACE_LOGGER_DEF_ENA"
#define ENVAR_LOGGER_DEF_DECOR_ENA "RETRACE_LOGGER_DEF_DECOR_ENA"
#define ENVAR_LOGGER_DEF_STDOUT_ENA "RETRACE_LOGGER_DEF_STDOUT_ENA"
#define ENVAR_LOGGER_DEF_FN "RETRACE_LOGGER_DEF_FN"
#define ENVAR_LOGGER_FMT "RETRACE_LOGGER_FMT"

/*
 * Per-function log filter env vars (issue #486):
 *
 *   RETRACE_LOGGER_ALLOWED_FUNCS=malloc,free,read
 *     Comma-separated allowlist. Only matching functions get logged
 *     by log_params. Other functions still go through retrace (real
 *     call runs) but emit no JSON entry.
 *
 *   RETRACE_LOGGER_EXCLUDED_FUNCS=printf
 *     Comma-separated denylist. Matching functions skip logging.
 *
 * Both can be combined: allowlist first, denylist removes from it.
 * If ALLOWED is unset, all functions are allowed by default; EXCLUDED
 * then optionally removes some.
 *
 * Names match exactly (no globbing). 64 functions max per list --
 * enough for any realistic filter.
 */
#define ENVAR_LOGGER_ALLOWED_FUNCS "RETRACE_LOGGER_ALLOWED_FUNCS"
#define ENVAR_LOGGER_EXCLUDED_FUNCS "RETRACE_LOGGER_EXCLUDED_FUNCS"
#define LOGGER_FUNC_FILTER_MAX 64

struct LoggerFuncFilter {
	char names[LOGGER_FUNC_FILTER_MAX][LOGGER_MAXLEN_FUNC_NAME + 1];
	int count;
};

static struct LoggerConfig
{
	int severities_ena[SEVERITY_CNT];
	int modules_ena[MODULES_CNT];
	int ena;
	int decoration_ena;
	int stdout_ena;
	FILE *logfile;

} g_logger_config = {
	.severities_ena = {
		[SEVERITY_DEBUG] = 0,
		[SEVERITY_INFO] = 1,
		[SEVERITY_WARN] = 1,
		[SEVERITY_ERROR] = 1
	},
	.modules_ena = {
		[ACTIONS] = 1,
		[CONF] = 1,
		[DATA_TYPES] = 1,
		[ENGINE] = 1,
		[FUNCS] = 1,
		[MAIN] = 1,
		[ARCH] = 1
	},
	.ena = 1,
	.decoration_ena = 1,
	.stdout_ena = 1,
	.logfile = NULL
};

static char *g_retrace_module_pref[MODULES_CNT] = {"ACT",
	"CONF",
	"TYPES",
	"ENGINE",
	"FUNCS",
	"MAIN",
	"ARCH"
};

static char *g_retrace_severities[SEVERITY_CNT] = {"DEBUG",
	"INFO",
	"WARN",
	"ERROR"
};

static int g_first_json;

/* TODO.windows/07: 1 = one JSON object per line (JSONL),
 * 0 (default) = a single JSON array document.
 */
static int g_logger_fmt_jsonl;

/*
 * MSVC's ring atomics are staged out (log_ring.h note): the
 * synchronous logger is the default there -- correct, just
 * hotter on the writing thread. MinGW/GCC have real C11
 * atomics.
 */
#if defined(_MSC_VER) && !defined(__clang__)
#define RC_RING_DEFAULT_ENABLED 0
#else
#define RC_RING_DEFAULT_ENABLED 1
#endif

/*
 * Thread-local "logging disabled" flag (TODO.complete/19 PR C).
 *
 * Set in the flusher thread (and any future internal thread)
 * so its libc calls don't generate log entries that would
 * feed back into its own ring. Without this, the flusher's
 * first printf via real_impls triggers a retrace intercept
 * (printf IS wrapped), the engine creates a ThreadContext for
 * the flusher, log_params pushes to the flusher's ring, the
 * flusher drains and emits via printf, which gets intercepted
 * again -- infinite recursion.
 *
 * The flag short-circuits log_json before any ring push.
 */
static _Thread_local int g_this_thread_logging_disabled;

/*
 * Tracks whether the ring subsystem has been initialized. The
 * flusher thread is spawned lazily on the first push -- see
 * ensure_flusher_running(). Split from g_logger_config.ena
 * because the ring may be inited even when logging is disabled
 * by env at a later stage.
 */
static int g_logger_ring_ready;

/*
 * Set when logger init has completed and cleared at deinit: libc
 * calls keep arriving during process teardown (musl's exit path
 * calls strcmp on "core..." AFTER retrace's destructor ran), and
 * they must be declined, not logged into torn-down state.
 */
static int g_logger_active;

/*
 * One-time guard for lazy flusher spawn. Set atomically; only
 * one thread wins the race to spawn.
 */
void retrace_logger_disable_for_this_thread(void)
{
	g_this_thread_logging_disabled = 1;
}

/* Per-function log filter, populated from env at logger_init. */
static struct LoggerFuncFilter g_logger_allowed_funcs;
static struct LoggerFuncFilter g_logger_excluded_funcs;

/*
 * Parse a comma-separated env var (e.g. "malloc,free,read") into the
 * filter. Truncates names > LOGGER_MAXLEN_FUNC_NAME. Stops at
 * LOGGER_FUNC_FILTER_MAX entries.
 */
static void logger_parse_func_filter(const char *env_name,
				     struct LoggerFuncFilter *filter)
{
	const char *p;
	const char *start;
	char *env_val;

	env_val = retrace_real_impls.getenv(env_name);
	if (env_val == NULL)
		return;

	filter->count = 0;
	p = env_val;
	start = env_val;

	while (1) {
		if (*p == ',' || *p == '\0') {
			size_t len = (size_t)(p - start);

			if (len > 0 && filter->count < LOGGER_FUNC_FILTER_MAX) {
				if (len > LOGGER_MAXLEN_FUNC_NAME)
					len = LOGGER_MAXLEN_FUNC_NAME;
				retrace_real_impls.strcpy(
					filter->names[filter->count],
					"");
				/* strncpy not in real_impls; manual truncation. */
				{
					size_t k;

					for (k = 0; k < len; k++)
						filter->names[filter->count][k] =
							start[k];
					filter->names[filter->count][len] = '\0';
				}
				filter->count++;
			}
			if (*p == '\0')
				break;
			start = p + 1;
		}
		p++;
	}
}

static int logger_filter_contains(const struct LoggerFuncFilter *filter,
				  const char *name)
{
	int i;

	for (i = 0; i < filter->count; i++) {
		if (!retrace_real_impls.strcmp(filter->names[i], name))
			return 1;
	}

	return 0;
}

int retrace_logger_func_loggable(const char *func_name)
{
	/* If the allowlist is set and func isn't in it, suppress. */
	if (g_logger_allowed_funcs.count > 0 &&
	    !logger_filter_contains(&g_logger_allowed_funcs, func_name))
		return 0;

	/* If the denylist contains func, suppress. */
	if (logger_filter_contains(&g_logger_excluded_funcs, func_name))
		return 0;

	return 1;
}

/*
 * Emit callback for the background flusher. Runs on the flusher
 * thread, so no synchronization needed on g_first_json or the
 * underlying FILE* -- only this thread writes to them.
 *
 * Receives a serialized JSON entry as entry->text and writes it
 * with the leading-comma logic that produces a valid JSON array.
 */
/*
 * One serialized entry, framed per the active format: JSONL
 * always emits a bare line; the array format prefixes later
 * entries with a comma (retrace's leading-comma emission).
 */
static void logger_write_entry_line(FILE *f, const char *text)
{
	if (g_logger_fmt_jsonl) {
		retrace_real_impls.fprintf(f, "%s\n", text);
	} else if (g_first_json) {
		retrace_real_impls.fprintf(f, ",\n%s\n", text);
	} else {
		retrace_real_impls.fprintf(f, "%s\n", text);
	}
}

static int logger_emit_entry(const struct LogEntry *entry, void *ctx)
{
	(void)ctx;

	if (entry == NULL || entry->text == NULL)
		return 0;

	if (g_logger_config.stdout_ena &&
	    retrace_real_impls.fprintf &&
	    retrace_real_impls.fflush) {
		logger_write_entry_line(stdout, entry->text);
		retrace_real_impls.fflush(stdout);
	}

	if (g_logger_config.logfile != NULL &&
	    retrace_real_impls.fprintf &&
	    retrace_real_impls.fflush) {
		logger_write_entry_line(g_logger_config.logfile,
			entry->text);
		retrace_real_impls.fflush(g_logger_config.logfile);
	}

	g_first_json = 1;
	return 0;
}

void retrace_logger_deinit(void)
{
	g_logger_active = 0;
	g_logger_ring_ready = 0;

	/* Stop the flusher first so it drains every pending entry
	 * before we write the closing "]" and tear down the rings.
	 * Only stop if it was actually spawned (lazy init: the
	 * flusher starts on first push, not in logger_init).
	 */
	if (g_logger_ring_ready && g_flusher_spawned)
		retrace_log_flusher_stop();
	if (g_logger_ring_ready)
		retrace_log_ring_deinit();

	/* experimental JSON, close the array of log messages */
	if (!g_logger_fmt_jsonl &&
		g_logger_config.ena &&
		(g_logger_config.stdout_ena || g_logger_config.logfile)) {

		/*
		 * Defensive: on some platforms retrace_real_impls may not be
		 * fully populated when the destructor runs (issue #452 on
		 * macOS Intel). Skip the closing "]\n" rather than crash on
		 * a NULL function pointer; the log is already flushed.
		 */
		if (g_logger_config.stdout_ena &&
			retrace_real_impls.printf &&
			retrace_real_impls.fflush) {
			retrace_real_impls.printf("]\n");
			retrace_real_impls.fflush(stdout);
		}

		if (g_logger_config.logfile != NULL &&
			retrace_real_impls.fprintf &&
			retrace_real_impls.fflush) {
			retrace_real_impls.fprintf(g_logger_config.logfile, "]\n");
			retrace_real_impls.fflush(g_logger_config.logfile);
		}
	}
/* Dont close logfile it breaks JSON format, the log is flushed anyway */

}

int retrace_logger_init(void)
{
	char *env_val;

	/* override def config with env parameters */
	env_val =
		retrace_real_impls.getenv(ENVAR_LOGGER_DEF_ENA);
	if (env_val != NULL)
		g_logger_config.ena = retrace_real_impls.atoi(env_val);

	env_val =
		retrace_real_impls.getenv(ENVAR_LOGGER_DEF_DECOR_ENA);
	if (env_val != NULL)
		g_logger_config.decoration_ena =
			retrace_real_impls.atoi(env_val);

	env_val =
		retrace_real_impls.getenv(ENVAR_LOGGER_DEF_STDOUT_ENA);
	if (env_val != NULL)
		g_logger_config.stdout_ena =
			retrace_real_impls.atoi(env_val);

	/* file will be closed upon process termination */
	env_val =
		retrace_real_impls.getenv(ENVAR_LOGGER_DEF_FN);
	if (env_val != NULL)
		g_logger_config.logfile =
			retrace_real_impls.fopen(env_val, "a");

	/* TODO.windows/07: streaming output -- one object per
	 * line instead of one array document.
	 */
	env_val = retrace_real_impls.getenv(ENVAR_LOGGER_FMT);
	if (env_val != NULL &&
	    retrace_real_impls.strcmp(env_val, "jsonl") == 0)
		g_logger_fmt_jsonl = 1;

	/* parse per-function filter env vars */
	logger_parse_func_filter(ENVAR_LOGGER_ALLOWED_FUNCS,
		&g_logger_allowed_funcs);
	logger_parse_func_filter(ENVAR_LOGGER_EXCLUDED_FUNCS,
		&g_logger_excluded_funcs);

	/* experimental JSON, make array of log messages */
	if (g_logger_config.ena &&
		(g_logger_config.stdout_ena || g_logger_config.logfile)) {

		/* The array format opens the document here; JSONL
		 * has no framing to write.
		 */
		if (!g_logger_fmt_jsonl) {
			if (g_logger_config.stdout_ena) {
				retrace_real_impls.printf("[\n");
				retrace_real_impls.fflush(stdout);
			}

			if (g_logger_config.logfile != NULL) {
				retrace_real_impls.fprintf(
					g_logger_config.logfile, "[\n");
				retrace_real_impls.fflush(
					g_logger_config.logfile);
			}
		}

		/* Init the lock-free ring subsystem. The background
		 * flusher thread is NOT spawned here -- it's spawned
		 * lazily on the first log push (see retrace_logger_log_json).
		 * Thread creation during a dyld __attribute__((constructor))
		 * can crash on some platforms (OHOS/musl under QEMU);
		 * deferring to first use avoids that.
		 *
		 * RETRACE_LOGGER_RING=0 disables the ring entirely;
		 * log_json falls back to synchronous writes for every
		 * call. Default: enabled. Use 0 on platforms where the
		 * background flusher thread is unstable (OHOS Docker/QEMU).
		 */
		{
			char *ring_env = retrace_real_impls.getenv(
				"RETRACE_LOGGER_RING");

			if (ring_env != NULL && ring_env[0] == '0')
				g_logger_ring_enabled = 0;
			else if (ring_env != NULL && ring_env[0] == '1')
				g_logger_ring_enabled = 1;
			else
				g_logger_ring_enabled =
					RC_RING_DEFAULT_ENABLED;
		}
		if (g_logger_ring_enabled) {
			if (retrace_log_ring_init() != 0) {
				log_err("logger: log_ring_init failed; "
					"falling back to sync log path");
				g_logger_ring_enabled = 0;
			} else {
				g_logger_ring_ready = 1;
			}
		}
	}

	g_logger_active = 1;
	return 0;
}

void retrace_loger_update_config(void)
{
	//const JSON_Object *logger_conf;
	//logger_conf = json_object_get_object(retrace_conf, "logger");
}


void retrace_logger_log(int module, int sev, const char *fmt, ...)
{
	JSON_Value *root_value;
	JSON_Object *root_object;
	va_list args;
	va_list args2;
	size_t log_size;
	char *logBuff;

	root_value = json_value_init_object();
	root_object = json_value_get_object(root_value);

	va_start(args, fmt);
	va_copy(args2, args);

	log_size = retrace_real_impls.real_vsnprintf(NULL, 0, fmt, args);
	logBuff = (char *) retrace_real_impls.malloc(log_size + 1);

	retrace_real_impls.real_vsnprintf(logBuff, log_size + 1, fmt, args2);
	json_object_set_string(root_object, "text", logBuff);

	retrace_logger_log_json(module, sev, root_value);

	retrace_real_impls.free(logBuff);
	va_end(args2);
	va_end(args);

	/* retrace_logger_log_json frees msg_value via
	 * json_object_set_value(... msg_value) -- it becomes a child
	 * of root_value which we then free.
	 */
}

/*
 * Process/thread identity for correlation (tebako-style inside vs
 * outside stream joins -- TODO.next-level/01). Neither getpid nor
 * the tid getters are in retrace's intercept inventory, so plain
 * libc calls carry no reentrancy risk.
 */
static long g_logger_pid = -1;

static long logger_pid(void)
{
	if (g_logger_pid < 0)
		g_logger_pid = rc_getpid();
	return g_logger_pid;
}

static long logger_tid(void)
{
	return (long)rc_thread_self_tid();
}

void retrace_logger_log_json(int module, int sev, JSON_Value *msg_value)
{
	time_t rawtime;
	JSON_Value *root_value;
	JSON_Object *root_object;
	char *serialized_string;
	struct LogRing *ring;

	if (g_this_thread_logging_disabled)
		return;

	if (!g_logger_active)
		return;

	if (!(g_logger_config.ena &&
		(g_logger_config.stdout_ena || g_logger_config.logfile)))
		return;

	if (!(g_logger_config.modules_ena[module]
		&& g_logger_config.severities_ena[sev]))
		return;

	root_value = json_value_init_object();
	root_object = json_value_get_object(root_value);

	retrace_real_impls.time(&rawtime);

	json_object_set_number(root_object, "time", rawtime);
	json_object_set_number(root_object, "pid", logger_pid());
	json_object_set_number(root_object, "tid", logger_tid());
	json_object_set_string(root_object, "module",
		g_retrace_module_pref[module]);
	json_object_set_string(root_object, "severity",
			g_retrace_severities[sev]);
	json_object_set_value(root_object, "message", msg_value);

	/*
	 * JSONL is one object per LINE, so it serializes compact;
	 * the array document keeps the pretty form.
	 */
	serialized_string = g_logger_fmt_jsonl ?
		json_serialize_to_string(root_value) :
		json_serialize_to_string_pretty(root_value);

	/*
	 * Lock-free hot path (post-init): push the serialized JSON
	 * to this thread's ring. The background flusher drains the
	 * ring and writes to stdout/file from a single thread,
	 * eliminating the global mutex contention.
	 *
	 * During init (before retrace_inited), write DIRECTLY.
	 * The constructor is single-threaded, so no mutex needed.
	 * This avoids spawning the flusher thread inside the
	 * constructor (which crashes on OHOS/musl under QEMU).
	 */
	if (g_logger_ring_ready && g_logger_ring_enabled &&
	    retrace_inited) {
		ensure_flusher_running();
		ring = retrace_log_ring_get();
		if (ring != NULL) {
			(void)retrace_log_ring_push(ring, (uint8_t)module,
				(uint8_t)sev, (uint32_t)rawtime,
				serialized_string);
		}
	} else if (g_logger_config.stdout_ena ||
		   g_logger_config.logfile != NULL) {
		/* Synchronous write during init -- single-threaded,
		 * no lock needed.
		 */
		if (g_logger_config.stdout_ena) {
			logger_write_entry_line(stdout,
				serialized_string);
			retrace_real_impls.fflush(stdout);
		}
		if (g_logger_config.logfile != NULL) {
			logger_write_entry_line(g_logger_config.logfile,
				serialized_string);
			retrace_real_impls.fflush(
				g_logger_config.logfile);
		}
		g_first_json = 1;
	}

	json_free_serialized_string(serialized_string);
	json_value_free(root_value);
}
