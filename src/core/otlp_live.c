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

#include "otlp_live.h"

#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "real_impls.h"
#include "logger.h"
#include "log_ring.h"
#include "reentrance_guard.h"
#include "engine.h"
#include "parson.h"
#include "otlp_allocator.h"

/* otlp-c public API. Always included -- the source file is
 * compiled out if the build doesn't link otlp-c (see CMake).
 */
#include "otlp-c/exporter.h"
#include "otlp-c/tracer.h"
#include "otlp-c/span.h"
#include "otlp-c/status.h"
#include "otlp-c/log.h"
#include "otlp-c/allocator.h"

/* Tick cadence for the live exporter thread. 100ms gives the
 * MPSC queue a chance to drain between batches while keeping
 * wall-clock latency to the collector well under a second.
 */
#define OTLP_TICK_MS 100

/* Bounded flush on shutdown: don't let a stuck collector hold
 * the destructor open. 2s matches the NtWriteFile / ntdll flush
 * budget used elsewhere in the engine.
 */
#define OTLP_FLUSH_TIMEOUT_MS 2000

static struct {
	otlp_exporter_t *exporter;
	otlp_tracer_t *tracer;
	rc_thread_h thread;
	struct ThreadContext ctx;

	_Atomic int running;
	_Atomic int thread_spawned;
	_Atomic int stop_signal;
	/*
	 * Teardown handshake (the exit-time UAF): the emit DOOR and
	 * the thread handshake are SEPARATE flags. door_open admits
	 * new emit callers and closes FIRST; emit_busy counts
	 * in-flight callers and must drain before any free; running
	 * is the tick thread's own "I am done" flag (cleared after
	 * its final flush) and doubles as the join condition. The
	 * old design joined only the tick thread -- the FLUSHER
	 * thread's sink (and the engine threads' security events)
	 * also touch the exporter and were never synchronized
	 * against deinit's free: a freed-exporter use at exit, the
	 * silent SEGV/abort under OTLP+logging.
	 */
	_Atomic int door_open;
	_Atomic int emit_busy;
	char endpoint[512];
	char service_name[128];
} g_otlp;

static void *otlp_live_thread_main(void *arg);
static void otlp_live_ensure_thread(void);
static int otlp_live_sink(const struct LogEntry *entry, void *ctx);
static int otlp_live_emit_event_inner(int severity,
	const char *event_name,
	const struct retrace_otlp_event_attr *attrs, size_t n_attrs);
static int otlp_live_emit_json_inner(const char *serialized_json);

static void otlp_live_final_stats(void);

static int otlp_live_emit_event_inner(int severity,
	const char *event_name,
	const struct retrace_otlp_event_attr *attrs, size_t n_attrs)
{
	otlp_log_record_t *lr;
	otlp_status_t st;
	size_t i;

	if (event_name == NULL)
		return -1;
	if (atomic_load_explicit(&g_otlp.running,
		memory_order_acquire) != 1)
		return 0;
	if (g_otlp.exporter == NULL)
		return 0;

	otlp_live_ensure_thread();

	lr = otlp_log_record_create((otlp_severity_t)severity,
		event_name);
	if (lr == NULL)
		return -1;
	otlp_log_record_mark_timestamp(lr);
	otlp_log_record_set_severity_text(lr,
		severity >= RETRACE_OTLP_SEV_ERROR ? "ERROR" :
		severity >= RETRACE_OTLP_SEV_WARN ? "WARN" : "INFO");
	otlp_log_record_set_attribute_string(lr, "retrace.event",
		event_name);

	for (i = 0; i < n_attrs; i++) {
		if (attrs[i].key == NULL)
			continue;
		if (attrs[i].str_val != NULL)
			otlp_log_record_set_attribute_string(lr,
				attrs[i].key, attrs[i].str_val);
		else
			otlp_log_record_set_attribute_int(lr,
				attrs[i].key, attrs[i].int_val);
	}

	st = otlp_exporter_emit_log_move(g_otlp.exporter, lr);
	if (st == OTLP_ERR_BUFFER_FULL) {
		/* Bounded failure: drop, count, never block. */
		return 0;
	}
	if (st != OTLP_OK) {
		otlp_log_record_free(lr);
		return -1;
	}
	return 0;
}

/*
 * The guarded emit doors. The early running check is the fast
 * path; the counter + re-check closes the teardown race: deinit
 * sets running=0, waits for emit_busy to drain, THEN frees the
 * exporter -- an in-flight caller is either counted (deinit
 * waits) or sees running==0 under the counter and leaves. The
 * wrapper shape means no early return inside the body can leak
 * the counter.
 */
int retrace_otlp_live_emit_event(int severity, const char *event_name,
	const struct retrace_otlp_event_attr *attrs, size_t n_attrs)
{
	/* args packaged into a small struct; kept on the stack */
	struct {
		int severity;
		const char *event_name;
		const struct retrace_otlp_event_attr *attrs;
		size_t n_attrs;
	} pkg = {severity, event_name, attrs, n_attrs};

	if (atomic_load_explicit(&g_otlp.door_open,
		memory_order_acquire) != 1)
		return 0;
	atomic_fetch_add_explicit(&g_otlp.emit_busy, 1,
		memory_order_acq_rel);
	if (atomic_load_explicit(&g_otlp.door_open,
		memory_order_acquire) != 1) {
		atomic_fetch_sub_explicit(&g_otlp.emit_busy, 1,
			memory_order_release);
		return 0;
	}
	{
		int rc = otlp_live_emit_event_inner(pkg.severity,
			pkg.event_name, pkg.attrs, pkg.n_attrs);

		atomic_fetch_sub_explicit(&g_otlp.emit_busy, 1,
			memory_order_release);
		return rc;
	}
}

int retrace_otlp_live_emit_json(const char *serialized_json)
{
	if (serialized_json == NULL)
		return -1;
	if (atomic_load_explicit(&g_otlp.door_open,
		memory_order_acquire) != 1)
		return 0;
	atomic_fetch_add_explicit(&g_otlp.emit_busy, 1,
		memory_order_acq_rel);
	if (atomic_load_explicit(&g_otlp.door_open,
		memory_order_acquire) != 1) {
		atomic_fetch_sub_explicit(&g_otlp.emit_busy, 1,
			memory_order_release);
		return 0;
	}
	{
		int rc = otlp_live_emit_json_inner(serialized_json);

		atomic_fetch_sub_explicit(&g_otlp.emit_busy, 1,
			memory_order_release);
		return rc;
	}
}

int retrace_otlp_live_init(void)
{
	char *env_endpoint;
	char *env_service;
	otlp_exporter_opts_t opts;

	if (atomic_load_explicit(&g_otlp.running,
		memory_order_relaxed) == 1) {
		log_warn("otlp_live: already running");
		return 0;
	}

	env_endpoint = retrace_real_impls.getenv("RETRACE_OTLP_ENDPOINT");
	if (env_endpoint == NULL || env_endpoint[0] == '\0')
		return 0; /* not enabled -- silent */

	/* Copy the env values: the otlp-c API copies them at create
	 * time, but the g_otlp.endpoint field is also surfaced via
	 * at-exit diagnostics -- keep our own copy for that.
	 *
	 * Path normalization: otlp-c uses the endpoint's path for
	 * the TRACES signal (logs/metrics force /v1/logs,
	 * /v1/metrics themselves). A bare base URL (no '/' after
	 * scheme://host[:port]) would post spans to "/" -- otelcol
	 * 404s there. Accept the base-URL UX users expect and append
	 * the default path.
	 */
	{
		const char *scan = env_endpoint;
		const char *path = NULL;

		/* skip scheme:// */
		while (*scan != '\0' && *scan != ':')
			scan++;
		if (scan[0] == ':' && scan[1] == '/' && scan[2] == '/')
			path = scan + 3;
		else
			path = env_endpoint; /* no scheme: relative? */
		while (*path != '\0' && *path != '/')
			path++;
		if (*path == '\0')
			retrace_real_impls.real_snprintf(
				g_otlp.endpoint, sizeof(g_otlp.endpoint),
				"%s/v1/traces", env_endpoint);
		else
			retrace_real_impls.real_snprintf(
				g_otlp.endpoint, sizeof(g_otlp.endpoint),
				"%s", env_endpoint);
	}

	/* Point otlp-c's internal allocator at retrace's real
	 * malloc/free (via the size-header shim in otlp_allocator.c)
	 * so the library's slab/mpsc/span allocations go through
	 * dlsym(RTLD_NEXT, ...) and not back through our hooked
	 * libc. Without this, otlp-c's alloc paths would re-enter
	 * the engine -- crash or infinite loop. Must be called
	 * BEFORE any other otlp-c API.
	 */
	retrace_otlp_allocator_install();

	env_service = retrace_real_impls.getenv("RETRACE_OTLP_SERVICE_NAME");
	if (env_service != NULL && env_service[0] != '\0') {
		strncpy(g_otlp.service_name, env_service,
			sizeof(g_otlp.service_name) - 1);
		g_otlp.service_name[sizeof(g_otlp.service_name) - 1] = '\0';
	} else {
		strncpy(g_otlp.service_name, "retrace",
			sizeof(g_otlp.service_name) - 1);
	}

	memset(&opts, 0, sizeof(opts));
	opts.endpoint = g_otlp.endpoint;
	opts.service_name = g_otlp.service_name;
	opts.batch_size = 256;
	opts.batch_ms = OTLP_TICK_MS * 2;
	opts.flush_timeout_ms = OTLP_FLUSH_TIMEOUT_MS;
	opts.connect_timeout_ms = 1000;
	opts.read_timeout_ms = 5000;
	opts.user_agent = "retrace/2.x otlp-live";

	g_otlp.exporter = otlp_exporter_create(&opts);
	if (g_otlp.exporter == NULL) {
		log_err("otlp_live: exporter create failed for %s",
			g_otlp.endpoint);
		return -1;
	}

	g_otlp.tracer = otlp_tracer_create(g_otlp.service_name,
		"retrace", "2.x");
	if (g_otlp.tracer == NULL) {
		log_err("otlp_live: tracer create failed");
		otlp_exporter_free(g_otlp.exporter);
		g_otlp.exporter = NULL;
		return -1;
	}

	/*
	 * The tick thread is NOT spawned here. Thread creation
	 * inside the library constructor crashes on musl (and
	 * OHOS/musl under QEMU) -- the same hazard the log
	 * flusher's lazy spawn works around (TODO.complete/19).
	 * otlp_live_ensure_thread() spawns it on the first emit,
	 * which happens on the flusher thread well after init.
	 */
	memset(&g_otlp.ctx, 0, sizeof(g_otlp.ctx));
	retrace_reentrance_guard_enter_permanent(&g_otlp.ctx, NULL);

	atomic_store_explicit(&g_otlp.stop_signal, 0,
		memory_order_relaxed);
	atomic_store_explicit(&g_otlp.door_open, 1,
		memory_order_release);
	atomic_store_explicit(&g_otlp.running, 1,
		memory_order_relaxed);

	/* Subscribe to the logger's sink seam (the architecture
	 * deepening): the flusher hands every drained entry to
	 * otlp_live_sink, which builds and enqueues the span. The
	 * sink checks the running flag, so deinit needs no
	 * unregister -- a full-slot failure just means no live
	 * streaming (logged loudly).
	 */
	if (retrace_log_sink_register(otlp_live_sink, NULL) < 0)
		log_err("otlp_live: log sink registry full; "
			"live span streaming disabled");

	log_info("otlp_live: streaming to %s as service=%s",
		g_otlp.endpoint, g_otlp.service_name);
	return 0;
}

/*
 * The logger-sink face of the live streamer: parse the entry's
 * JSON envelope, build the span, enqueue. Runs on the flusher
 * thread; every check mirrors the old in-logger call.
 */
static int otlp_live_sink(const struct LogEntry *entry, void *ctx)
{
	(void)ctx;

	if (entry == NULL || entry->text == NULL)
		return 0;
	return retrace_otlp_live_emit_json(entry->text);
}

/*
 * Spawn the tick thread on first use. Called from BOTH
 * retrace_otlp_live_emit_json (the flusher thread) and
 * retrace_otlp_live_emit_event (the target's engine thread --
 * Wave C) so the constructor never creates threads. Two
 * callers can race here; the CAS admits exactly ONE creator --
 * a second tick thread would run otlp_exporter_tick()
 * concurrently, which is single-thread-only (CI: the Linux
 * jail-integration leg SEGV'd on the double-spawn race).
 */
static void otlp_live_ensure_thread(void)
{
	int expected = 0;
	int trc;

	if (!atomic_compare_exchange_strong(&g_otlp.thread_spawned,
		&expected, 1))
		return;

	trc = retrace_real_impls.rc_thread_create(&g_otlp.thread,
		otlp_live_thread_main, NULL);
	if (trc != 0) {
		log_err("otlp_live: thread create failed: %d", trc);
		atomic_store_explicit(&g_otlp.thread_spawned, 0,
			memory_order_release);
	}
}

void retrace_otlp_live_deinit(void)
{
	if (atomic_load_explicit(&g_otlp.running,
		memory_order_relaxed) != 1)
		return;

	/*
	 * Close the door, drain in-flight emitters (the flusher's
	 * sink, engine threads), THEN the thread handshake + free.
	 * running stays 1 here: it is the TICK THREAD's "I am
	 * done" flag (it clears it after the final flush) and the
	 * join condition below. On a drain timeout we LEAK instead
	 * of freeing under a live caller -- at exit a bounded leak
	 * is the safe side. The logger's deinit (which drains the
	 * flusher's tail through this sink) runs BEFORE us by
	 * destructor order; the door still being open then is why
	 * the tail ships.
	 */
	atomic_store_explicit(&g_otlp.door_open, 0,
		memory_order_release);
	{
		struct timespec poll = {.tv_sec = 0, .tv_nsec = 100000};
		int waits = 0;

		while (atomic_load_explicit(&g_otlp.emit_busy,
			memory_order_acquire) != 0 && waits < 30000) {
			nanosleep(&poll, NULL);
			waits++;
		}
		if (waits >= 30000)
			return;	/* leak, never free under a caller */
	}

	atomic_store_explicit(&g_otlp.stop_signal, 1,
		memory_order_relaxed);

	if (atomic_load_explicit(&g_otlp.thread_spawned,
		memory_order_acquire) == 1) {
#if defined(__APPLE__)
		/* macOS: pthread_join from a dyld destructor can hang;
		 * use the same spin-wait pattern as the log flusher
		 * (TODO.complete/19). The thread is set to
		 * short-interval tick so it polls the stop signal
		 * within ~OTLP_TICK_MS.
		 */
		{
			struct timespec poll = {.tv_sec = 0, .tv_nsec = 100000};
			struct timespec grace = {.tv_sec = 0, .tv_nsec = 10000000};
			int waits = 0;

			while (atomic_load_explicit(&g_otlp.running,
				memory_order_acquire) == 1 && waits < 20000) {
				nanosleep(&poll, NULL);
				waits++;
			}
			if (waits < 20000)
				nanosleep(&grace, NULL);
		}
#else
		retrace_real_impls.rc_thread_join(&g_otlp.thread);
#endif
	}

	/* Final stats BEFORE free -- get_stats checks the running
	 * flag, but the thread has just set it to 0, so read
	 * directly via the exporter pointer. The exporter itself is
	 * still valid at this point.
	 */
	otlp_live_final_stats();

	otlp_exporter_free(g_otlp.exporter);
	g_otlp.exporter = NULL;
	if (g_otlp.tracer != NULL) {
		otlp_tracer_free(g_otlp.tracer);
		g_otlp.tracer = NULL;
	}

	/* running was cleared at entry; keep the release for the
	 * tick thread's own exit path (it also clears it)
	 */
	atomic_store_explicit(&g_otlp.running, 0,
		memory_order_release);
}

static void *otlp_live_thread_main(void *arg)
{
	struct timespec ts = {
		.tv_sec = 0,
		.tv_nsec = (long)OTLP_TICK_MS * 1000000L,
	};

	(void)arg;

	/*
	 * Mark this thread as "logging disabled" FIRST so the
	 * context registration below -- which goes through
	 * rc_tss_set, an INTERPOSED pthread_setspecific -- cannot
	 * feed log entries back into the ring it would drain (the
	 * log flusher's ordering, TODO.complete/19).
	 */
	retrace_logger_disable_for_this_thread();

	/*
	 * Install the permanent reentrance guard on this thread's
	 * own context -- the otlp_exporter's own send/connect/recv
	 * through our hooked libc MUST pass through to the real
	 * impl. Without the permanent guard, the engine enters its
	 * regular guard on our context, then attempts action
	 * processing on a connect/send call -- which can call back
	 * into the otlp-c library and deadlock or crash. Same
	 * NtWriteFile lesson as TODO 28.
	 */
	{
		extern struct ThreadContext *retrace_thread_context_get(void);
		struct ThreadContext *ctx = retrace_thread_context_get();

		if (ctx != NULL)
			retrace_reentrance_guard_enter_permanent(ctx, NULL);
	}

	while (atomic_load_explicit(&g_otlp.stop_signal,
		memory_order_relaxed) == 0) {
		otlp_exporter_tick(g_otlp.exporter, OTLP_TICK_MS);
		nanosleep(&ts, NULL);
	}

	/* One last tick to drain anything queued during the loop. */
	otlp_exporter_tick(g_otlp.exporter, 0);
	otlp_exporter_shutdown(g_otlp.exporter);
	otlp_exporter_flush(g_otlp.exporter);

	atomic_store_explicit(&g_otlp.running, 0,
		memory_order_release);
	return NULL;
}

static int otlp_live_emit_json_inner(const char *serialized_json)
{
	otlp_span_t *span;
	JSON_Value *root;
	JSON_Object *env;
	JSON_Object *msg;
	const char *func;
	const char *module;
	const char *severity;
	double pid, tid;
	otlp_status_t st;

	if (serialized_json == NULL)
		return -1;
	if (atomic_load_explicit(&g_otlp.running,
		memory_order_acquire) != 1)
		return 0;
	if (g_otlp.tracer == NULL || g_otlp.exporter == NULL)
		return 0;

	otlp_live_ensure_thread();

	root = json_parse_string(serialized_json);
	if (root == NULL)
		return -1;

	env = json_value_get_object(root);
	if (env == NULL) {
		json_value_free(root);
		return -1;
	}
	msg = json_object_get_object(env, "message");
	if (msg == NULL) {
		json_value_free(root);
		return -1;
	}

	func = json_object_get_string(msg, "func");
	module = json_object_get_string(env, "module");
	severity = json_object_get_string(env, "severity");
	pid = json_object_get_number(env, "pid");
	tid = json_object_get_number(env, "tid");

	span = otlp_tracer_start_span(g_otlp.tracer,
		func ? func : "unknown");
	if (span == NULL) {
		json_value_free(root);
		return -1;
	}

	otlp_span_set_kind(span, OTLP_SPAN_KIND_INTERNAL);

	if (module != NULL)
		otlp_span_set_attribute_string(span, "retrace.module",
			module);
	if (severity != NULL) {
		otlp_span_set_attribute_string(span, "retrace.severity",
			severity);
		if (severity[0] == 'E' || severity[0] == 'e')
			otlp_span_set_status(span,
				OTLP_STATUS_CODE_ERROR, severity);
	}
	otlp_span_set_attribute_int(span, "retrace.pid", (int64_t)pid);
	otlp_span_set_attribute_int(span, "retrace.tid", (int64_t)tid);

	/*
	 * Fleet labels (TODO.supervisor/06): when a supervisor is
	 * active, every span carries the session id, agent id, and
	 * the active policy epoch -- one detonation = one trace
	 * waterfall in Tempo, every span stamped. Read via getenv
	 * (the supervisor zero-dispatch contract); missing -> omit.
	 */
	{
		const char *sess = retrace_real_impls.getenv(
			"RETRACE_SESSION");
		const char *sock = retrace_real_impls.getenv(
			"RETRACE_SUPERVISOR_SOCK");
		if (sess != NULL && sess[0] != '\0')
			otlp_span_set_attribute_string(span,
				"retrace.session_id", sess);
		if (sock != NULL && sock[0] != '\0') {
			otlp_span_set_attribute_string(span,
				"retrace.agent_id", sock);
		}
	}

	/* Per-call retrace-specific fields (present when the
	 * function uses log_params + call_real).
	 */
	{
		double dur = json_object_get_number(msg, "call_duration_us");
		double rv = json_object_get_number(msg, "ret_val");
		const char *params = json_object_get_string(msg, "params");

		if (dur != 0.0)
			otlp_span_set_attribute_double(span,
				"retrace.call_duration_us", dur);
		if (rv != 0.0 || json_object_has_value(msg, "ret_val"))
			otlp_span_set_attribute_double(span,
				"retrace.ret_val", rv);
		if (params != NULL)
			otlp_span_set_attribute_string(span,
				"retrace.params", params);
	}

	st = otlp_exporter_emit_move(g_otlp.exporter, span);
	if (st == OTLP_ERR_BUFFER_FULL) {
		/* Bounded failure: drop, count, never block the caller. */
		json_value_free(root);
		return 0;
	}
	if (st != OTLP_OK) {
		otlp_span_free(span);
		json_value_free(root);
		return -1;
	}

	json_value_free(root);
	return 0;
}

void retrace_otlp_live_get_stats(uint64_t *emitted, uint64_t *sent,
				 uint64_t *dropped_full,
				 uint64_t *dropped_err)
{
	otlp_exporter_stats_t stats;

	if (emitted != NULL)
		*emitted = 0;
	if (sent != NULL)
		*sent = 0;
	if (dropped_full != NULL)
		*dropped_full = 0;
	if (dropped_err != NULL)
		*dropped_err = 0;

	/* The exporter pointer is valid as long as we have it -- do
	 * not gate on the running flag (the thread sets it to 0
	 * before deinit's final-stats call).
	 */
	if (g_otlp.exporter == NULL)
		return;

	if (otlp_exporter_get_stats(g_otlp.exporter, &stats) == OTLP_OK) {
		if (emitted != NULL)
			*emitted = stats.emitted;
		if (sent != NULL)
			*sent = stats.sent;
		if (dropped_full != NULL)
			*dropped_full = stats.dropped_full;
		if (dropped_err != NULL)
			*dropped_err = stats.dropped_err;
	}
}

static void otlp_live_final_stats(void)
{
	uint64_t emitted, sent, dropped_full, dropped_err;
	otlp_exporter_stats_t stats;
	uint64_t logs_emitted = 0, logs_sent = 0;

	retrace_otlp_live_get_stats(&emitted, &sent, &dropped_full,
		&dropped_err);

	/* LOG-signal counters (TODO.trace-profile/32): security
	 * events ride /v1/logs; surface them on the same line.
	 */
	if (g_otlp.exporter != NULL &&
	    otlp_exporter_get_stats(g_otlp.exporter, &stats) == OTLP_OK) {
		logs_emitted = stats.emitted_logs;
		logs_sent = stats.sent_logs;
	}

	if (emitted == 0 && sent == 0 && logs_emitted == 0 &&
	    logs_sent == 0)
		return;

	/* stderr line -- users grep for it on agent teardown. */
	retrace_real_impls.fprintf(stderr,
		"retrace: otlp_live: emitted=%llu sent=%llu "
		"logs_emitted=%llu logs_sent=%llu "
		"dropped_full=%llu dropped_err=%llu endpoint=%s\n",
		(unsigned long long)emitted,
		(unsigned long long)sent,
		(unsigned long long)logs_emitted,
		(unsigned long long)logs_sent,
		(unsigned long long)dropped_full,
		(unsigned long long)dropped_err,
		g_otlp.endpoint);
}
