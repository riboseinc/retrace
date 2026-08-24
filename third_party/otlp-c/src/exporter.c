/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Batching OTLP/HTTP exporter — caller-tick model.
 *
 * emit() (any thread): clones the span and pushes the pointer into
 * a lock-free MPSC queue. Returns immediately.
 *
 * tick() (single thread): drains the queue, accumulates spans into
 * a pending batch, and either starts a new HTTP POST (encode once)
 * or steps the in-flight request. On terminal HTTP/network status,
 * updates stats and either retries (with backoff) or drops.
 *
 * flush() (single thread): loops tick() until the queue is empty
 * and no request is in flight, or until max_retries is exhausted.
 *
 * shutdown() (any thread): sets an atomic flag; subsequent emit()
 * calls return OTLP_ERR_SHUTDOWN.
 *
 * The library never spawns a thread, never takes a mutex. All
 * cross-thread data flow uses atomics + the MPSC queue.
 */
#include <otlp-c/exporter.h>
#include <otlp-c/span.h>
#include <otlp-c/version.h>

#include "exporter_internal.h"
#include "exporter_otel.h"
#include "http_client.h"
#include "internal_util.h"
#include "log_internal.h"
#include "metric_internal.h"
#include "mpsc_queue.h"
#include "otlp_messages.h"
#include "platform.h"
#include "retry_policy.h"
#include "span_internal.h"

#include "atomic_compat.h"
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define OTLP_DEFAULT_ENDPOINT "http://localhost:4318/v1/traces"
#define OTLP_DEFAULT_BATCH_SIZE 512
/* Upper clamp for caller-supplied batch_size. Prevents
 * batch_size * 2 * sizeof(ptr) from overflowing size_t in the
 * pending-array allocation. 1M items per batch is already
 * enormous (~200 MB of encoded wire data); callers wanting
 * more should shard across multiple exporters. */
#define OTLP_MAX_BATCH_SIZE (1024u * 1024u)
#define OTLP_DEFAULT_BATCH_MS 100
#define OTLP_DEFAULT_MAX_RETRIES 5
#define OTLP_RESOURCE_MAX_ATTRS 128
#define OTLP_DEFAULT_BACKOFF_INIT_MS 1000
#define OTLP_DEFAULT_BACKOFF_MAX_MS 30000
#define OTLP_DEFAULT_CONNECT_TIMEOUT 5000
#define OTLP_DEFAULT_READ_TIMEOUT 10000
#define OTLP_DEFAULT_FLUSH_TIMEOUT_MS 30000
#define OTLP_DEFAULT_QUEUE_CAP 4096

/* Per-signal runtime state (one instance per signal, sig[3]).
 * The static-constant half of the descriptor (name, free/clone/
 * build adapters) lives in SIGNAL_SPECS below. */
struct signal_state
{
	struct mpsc_queue queue;
	void **pending;
	size_t pending_cap;
	size_t pending_count;
	bool first_set;
	uint64_t first_mono;
	otlp_atomic_u64 emitted;
	otlp_atomic_u64 dropped_full;
	otlp_atomic_u64 dropped_err;
	otlp_atomic_u64 sent;
	otlp_atomic_u64 rejected;
};

struct otlp_exporter
{
	/* Immutable after create. */
	struct otlp_http_url url;
	char *user_agent;
	char *service_name;
	struct otlp_attr_vec resource_attrs;
	size_t batch_size;
	uint32_t batch_ms;
	uint32_t max_retries;
	uint32_t backoff_initial_ms;
	uint32_t backoff_max_ms;
	uint32_t flush_timeout_ms;
	uint32_t connect_timeout_ms;
	uint32_t read_timeout_ms;

	/* Per-signal state, indexed by signal kind (SIGNAL_SPAN=0,
	 * SIGNAL_METRIC=1, SIGNAL_LOG=2): queue, pending batch, batch
	 * timer, and the per-signal stats counters, ONE struct instead
	 * of fifteen hand-named fields per signal. Every signal-generic
	 * driver (emit, tick, record_outcome, start-post, drain, stats)
	 * indexes this array. */
	struct signal_state sig[3];

	/* HTTP-level counters (not per-signal). */
	otlp_atomic_u64 http_2xx;
	otlp_atomic_u64 http_4xx;
	otlp_atomic_u64 http_5xx;
	otlp_atomic_u64 network_err;
	otlp_atomic_int shutdown_requested;

	/* Optional diagnostic callbacks (NULL = no-op). The event
	 * callback receives the structured model; the string logger
	 * receives the message derived from it by format_event(). */
	otlp_log_fn log_fn;
	void *log_ctx;
	otlp_event_fn event_fn;
	void *event_ctx;

	/* In-flight request state. in_flight_signal identifies which
	 * signal's batch is being POSTed (0=span, 1=metric, 2=log). */
	otlp_http_request_t *in_flight;
	int in_flight_signal;
	size_t in_flight_count;
	uint32_t attempt;
	uint64_t backoff_deadline_mono;
	bool backoff_armed;
	/* Cached TCP connection for HTTP keep-alive. Owned by the exporter,
	 * donated to the next in_flight request, re-acquired on success. */
	otlp_socket_t *keepalive_sock;
	bool null_transport;
	otlp_null_transport_status_fn null_transport_status_fn;
	void *null_transport_status_ctx;
	/* Backoff-jitter PRNG (xorshift64s). Tick-thread-only: read
	 * and written exclusively from the tick caller, so plain
	 * non-atomic state is correct. */
	uint64_t jitter_prng;
};

/* ── The signal table ────────────────────────────────────────────
 *
 * ONE row per signal: the public id, the diagnostics name, and the
 * three type-erased adapters (free / clone / build-request). This
 * is the single place a signal's type-specific knowledge lives;
 * every signal-generic driver (emit, tick, record_outcome,
 * start-post, drain) dispatches through it. Adding a signal (OTLP
 * profiles someday) = one row + the typed functions it names.
 *
 * The RUNTIME half (queues, pending batches, counters) lives in
 * struct otlp_exporter's sig[3], indexed by the same kind. */

/* Signal kind constants — also the index into sig[] and
 * SIGNAL_SPECS[]. Matches otlp_signal_id_t's values. */
enum
{
	SIGNAL_SPAN = 0,
	SIGNAL_METRIC = 1,
	SIGNAL_LOG = 2,
	N_SIGNALS
};

static void
span_free_void(void *p)
{
	otlp_span_free(p);
}

static void
metric_free_void(void *p)
{
	otlp_metric_free(p);
}

static void
log_free_void(void *p)
{
	otlp_log_record_free(p);
}

static void *
span_clone_void(const void *p)
{
	return otlp_span_clone((const otlp_span_t *) p);
}

static void *
metric_clone_void(const void *p)
{
	return otlp_metric_clone((const otlp_metric_t *) p);
}

static void *
log_clone_void(const void *p)
{
	return otlp_log_record_clone((const otlp_log_record_t *) p);
}

/* Type-erased wrappers around the typed otlp_exporter_otel_build_*
 * functions. Each takes const void *const * items so the spec table
 * can hold a single function-pointer type. The cast is localized
 * here; the typed build helpers retain full type safety. */
static otlp_status_t
build_span_request_void(const struct otlp_http_url *url,
	const char *user_agent,
	const char *service_name,
	const struct otlp_attribute *res_attrs,
	size_t n_res_attrs,
	const void *const *items,
	size_t n_items,
	uint32_t connect_to,
	uint32_t read_to,
	otlp_socket_t *reuse,
	otlp_http_request_t **out)
{
	return otlp_exporter_otel_build_span_request(url,
		user_agent,
		service_name,
		res_attrs,
		n_res_attrs,
		(const otlp_span_t *const *) items,
		n_items,
		connect_to,
		read_to,
		reuse,
		out);
}

static otlp_status_t
build_metric_request_void(const struct otlp_http_url *url,
	const char *user_agent,
	const char *service_name,
	const struct otlp_attribute *res_attrs,
	size_t n_res_attrs,
	const void *const *items,
	size_t n_items,
	uint32_t connect_to,
	uint32_t read_to,
	otlp_socket_t *reuse,
	otlp_http_request_t **out)
{
	return otlp_exporter_otel_build_metric_request(url,
		user_agent,
		service_name,
		res_attrs,
		n_res_attrs,
		(const otlp_metric_t *const *) items,
		n_items,
		connect_to,
		read_to,
		reuse,
		out);
}

static otlp_status_t
build_log_request_void(const struct otlp_http_url *url,
	const char *user_agent,
	const char *service_name,
	const struct otlp_attribute *res_attrs,
	size_t n_res_attrs,
	const void *const *items,
	size_t n_items,
	uint32_t connect_to,
	uint32_t read_to,
	otlp_socket_t *reuse,
	otlp_http_request_t **out)
{
	return otlp_exporter_otel_build_log_request(url,
		user_agent,
		service_name,
		res_attrs,
		n_res_attrs,
		(const otlp_log_record_t *const *) items,
		n_items,
		connect_to,
		read_to,
		reuse,
		out);
}

typedef otlp_status_t (*otlp_build_request_fn)(const struct otlp_http_url *,
	const char *,
	const char *,
	const struct otlp_attribute *,
	size_t,
	const void *const *,
	size_t,
	uint32_t,
	uint32_t,
	otlp_socket_t *,
	otlp_http_request_t **);

/* The static-constant half of the per-signal descriptor. */
struct signal_spec
{
	otlp_signal_id_t id;
	const char *name;
	void (*free_item)(void *);
	void *(*clone_item)(const void *);
	otlp_build_request_fn build_request;
};

static const struct signal_spec SIGNAL_SPECS[N_SIGNALS] = {
	[SIGNAL_SPAN] = { OTLP_SIGNAL_TRACES,
		"spans",
		span_free_void,
		span_clone_void,
		build_span_request_void },
	[SIGNAL_METRIC] = { OTLP_SIGNAL_METRICS,
		"metrics",
		metric_free_void,
		metric_clone_void,
		build_metric_request_void },
	[SIGNAL_LOG] = { OTLP_SIGNAL_LOGS,
		"logs",
		log_free_void,
		log_clone_void,
		build_log_request_void },
};

/* Drain one signal: pop everything from the queue and free it,
 * then free the pending batch. */
static void
drain_signal(struct signal_state *sg, void (*free_item)(void *))
{
	void *item;
	size_t i;

	while ((item = mpsc_queue_pop(&sg->queue)) != NULL)
		free_item(item);
	for (i = 0; i < sg->pending_count; i++)
		free_item(sg->pending[i]);
}

/* ── Helpers ──────────────────────────────────────────────────── */

static const char *
signal_name(otlp_signal_id_t s)
{
	int k;

	for (k = 0; k < N_SIGNALS; k++)
		if (SIGNAL_SPECS[k].id == s)
			return SIGNAL_SPECS[k].name;
	return "spans";
}

/* Render the event's human-readable line. THE single presentation
 * point: every diagnostic string is derived from the otlp_event_t
 * model here, so the structured and string views cannot diverge. */
static void
format_event(char *buf, size_t cap, const otlp_event_t *ev)
{
	switch (ev->code)
	{
		case OTLP_EVT_QUEUE_FULL:
			snprintf(buf,
				cap,
				"%s dropped: queue full",
				signal_name(ev->signal));
			break;
		case OTLP_EVT_BATCH_SENT:
			snprintf(buf,
				cap,
				"batch sent: %llu %s",
				(unsigned long long) ev->count,
				signal_name(ev->signal));
			break;
		case OTLP_EVT_RETRY_ARMED:
			if (ev->http_status == 0)
				snprintf(buf,
					cap,
					"network error; %s retry %u/%u in %ums",
					signal_name(ev->signal),
					ev->attempt,
					ev->max_retries,
					ev->delay_ms);
			else
				snprintf(buf,
					cap,
					"%s HTTP %d; retry %u/%u in %ums%s",
					signal_name(ev->signal),
					ev->http_status,
					ev->attempt,
					ev->max_retries,
					ev->delay_ms,
					ev->server_driven
						? " (server Retry-After)"
						: "");
			break;
		case OTLP_EVT_ITEMS_DROPPED:
			if (ev->drop_reason == OTLP_DROP_HTTP_STATUS)
				snprintf(buf,
					cap,
					"HTTP %d: %llu %s dropped (permanent)",
					ev->http_status,
					(unsigned long long) ev->count,
					signal_name(ev->signal));
			else if (ev->http_status == 0)
				snprintf(buf,
					cap,
					"network error: %llu %s dropped "
					"(max retries %u)",
					(unsigned long long) ev->count,
					signal_name(ev->signal),
					ev->max_retries);
			else
				snprintf(buf,
					cap,
					"HTTP %d: %llu %s dropped "
					"(max retries %u)",
					ev->http_status,
					(unsigned long long) ev->count,
					signal_name(ev->signal),
					ev->max_retries);
			break;
		case OTLP_EVT_PARTIAL_SUCCESS:
			if (ev->count > 0)
				snprintf(buf,
					cap,
					"collector partial success: %llu of "
					"%llu "
					"%s rejected",
					(unsigned long long) ev->rejected,
					(unsigned long long) ev->count,
					signal_name(ev->signal));
			else
				snprintf(buf,
					cap,
					"collector partial success: %llu %s "
					"rejected",
					(unsigned long long) ev->rejected,
					signal_name(ev->signal));
			if (ev->detail_len > 0 && ev->detail)
				snprintf(buf + strlen(buf),
					cap - strlen(buf),
					": %.*s",
					(int) (ev->detail_len > 128
							? 128
							: ev->detail_len),
					ev->detail);
			break;
		case OTLP_EVT_SYNC_FLUSH_FAILED:
			if (ev->status == OTLP_ERR_TIMEOUT)
				snprintf(buf,
					cap,
					"sync flush %s: timeout after %ums",
					signal_name(ev->signal),
					ev->timeout_ms);
			else if (ev->http_status > 0)
				snprintf(buf,
					cap,
					"sync flush %s: HTTP %d",
					signal_name(ev->signal),
					ev->http_status);
			else
				snprintf(buf,
					cap,
					"sync flush %s: failed (st=%d)",
					signal_name(ev->signal),
					(int) ev->status);
			break;
		default:
			snprintf(buf, cap, "event %d", (int) ev->code);
			break;
	}
}

/* Dispatch one diagnostic: structured callback first, then the
 * string logger (if installed) with the message derived from the
 * same event. No-ops when neither callback is set — zero overhead
 * beyond building the struct. */
static void
event_log(const struct otlp_exporter *e, const otlp_event_t *ev)
{
	if (!e)
		return;
	if (e->event_fn)
		e->event_fn(e->event_ctx, ev);
	if (e->log_fn)
	{
		char buf[256];

		format_event(buf, sizeof(buf), ev);
		e->log_fn(e->log_ctx, ev->level, buf);
	}
}

static size_t
round_up_pow2(size_t v)
{
	size_t r = 1;

	while (r < v)
	{
		if (r > (SIZE_MAX / 2))
			return r;
		r *= 2;
	}
	return r;
}

static void
normalize_opts(otlp_exporter_opts_t *o)
{
	if (!o->endpoint)
		o->endpoint = OTLP_DEFAULT_ENDPOINT;
	if (!o->service_name)
		o->service_name = "";
	if (o->batch_size == 0)
		o->batch_size = OTLP_DEFAULT_BATCH_SIZE;
	if (o->batch_size > OTLP_MAX_BATCH_SIZE)
		o->batch_size = OTLP_MAX_BATCH_SIZE;
	if (o->batch_ms == 0)
		o->batch_ms = OTLP_DEFAULT_BATCH_MS;
	if (o->max_retries == 0)
		o->max_retries = OTLP_DEFAULT_MAX_RETRIES;
	if (o->backoff_initial_ms == 0)
		o->backoff_initial_ms = OTLP_DEFAULT_BACKOFF_INIT_MS;
	if (o->backoff_max_ms == 0)
		o->backoff_max_ms = OTLP_DEFAULT_BACKOFF_MAX_MS;
	if (o->connect_timeout_ms == 0)
		o->connect_timeout_ms = OTLP_DEFAULT_CONNECT_TIMEOUT;
	if (o->read_timeout_ms == 0)
		o->read_timeout_ms = OTLP_DEFAULT_READ_TIMEOUT;
	if (o->flush_timeout_ms == 0)
		o->flush_timeout_ms = OTLP_DEFAULT_FLUSH_TIMEOUT_MS;
	if (!o->user_agent)
		o->user_agent = "otlp-c/" OTLP_C_VERSION_STRING;
	if (o->queue_capacity == 0)
		o->queue_capacity = OTLP_DEFAULT_QUEUE_CAP;
	o->queue_capacity = round_up_pow2(o->queue_capacity);
}

/* ── Lifecycle ────────────────────────────────────────────────── */

const struct otlp_attribute *
otlp_exporter_get_resource_attrs(const otlp_exporter_t *e, size_t *n)
{
	if (n)
		*n = e ? e->resource_attrs.n : 0;
	return e ? e->resource_attrs.items : NULL;
}

otlp_exporter_t *
otlp_exporter_create(const otlp_exporter_opts_t *opts_in)
{
	struct otlp_exporter *e;
	otlp_exporter_opts_t o;
	otlp_status_t st;

	if (!opts_in)
		return NULL;

	o = *opts_in;
	normalize_opts(&o);

	e = otlp_calloc(1, sizeof(*e));
	if (!e)
		return NULL;

	st = otlp_http_parse_url(o.endpoint, &e->url);
	if (st != OTLP_OK)
		goto fail;

	if (o.service_name && !otlp_str_is_utf8(o.service_name))
		goto fail;
	e->user_agent = otlp_dup_str(o.user_agent);
	e->service_name = otlp_dup_str(o.service_name);
	if (!e->user_agent || !e->service_name)
		goto fail;
	if (o.n_resource_attributes > 0 && o.resource_attributes)
	{
		size_t i;
		bool svc_set = o.service_name && o.service_name[0];

		/* Resource attributes on the ONE model (v0.5.92): the
		 * set-attribute engine gives map semantics (duplicate
		 * keys collapse last-write-wins), deep copy, and all
		 * seven AnyValue types. "service.name" yields to the
		 * dedicated opt (v0.5.78). Empty keys and empty string
		 * values are skipped (pre-v0.5.92 behavior preserved). */
		for (i = 0; i < o.n_resource_attributes; i++)
		{
			const otlp_resource_attr_t *src =
				&o.resource_attributes[i];

			if (!src->key || !src->key[0])
				continue;
			if (svc_set && strcmp(src->key, "service.name") == 0)
				continue;
			if (src->value.type == OTLP_VALUE_STRING &&
				(!src->value.v.string_val ||
					!src->value.v.string_val[0]))
				continue;
			{
				otlp_status_t rst =
					otlp_attr_vec_set(&e->resource_attrs,
						OTLP_RESOURCE_MAX_ATTRS,
						src->key,
						&src->value);

				if (rst != OTLP_OK)
					goto fail;
			}
		}
	}

	e->batch_size = o.batch_size;
	e->batch_ms = o.batch_ms;
	e->max_retries = o.max_retries;
	e->backoff_initial_ms = o.backoff_initial_ms;
	e->backoff_max_ms = o.backoff_max_ms;
	e->flush_timeout_ms = o.flush_timeout_ms;
	e->connect_timeout_ms = o.connect_timeout_ms;
	e->read_timeout_ms = o.read_timeout_ms;

	{
		int s;

		for (s = 0; s < N_SIGNALS; s++)
		{
			e->sig[s].pending_cap = e->batch_size * 2;
			e->sig[s].pending = otlp_malloc(e->sig[s].pending_cap *
				sizeof(*e->sig[s].pending));
			if (!e->sig[s].pending)
				goto fail;
			st = mpsc_queue_init(
				&e->sig[s].queue, o.queue_capacity);
			if (st != OTLP_OK)
				goto fail;
		}
	}

	e->in_flight_signal = 0; /* SPAN */
	{
		uint64_t seed = 0;

		(void) otlp_platform_now_mono_nano(&seed);
		seed ^= (uint64_t)(uintptr_t) e;
		if (seed == 0)
			seed = 0x9E3779B97F4A7C15ULL;
		e->jitter_prng = seed;
	}
	otlp_atomic_store_int(
		&e->shutdown_requested, 0, OTLP_MEMORY_ORDER_RELEASE);
	return e;

fail:
	otlp_free(e->user_agent);
	otlp_free(e->service_name);
	otlp_attr_vec_free(&e->resource_attrs);
	{
		int s;

		/* mpsc_queue_free is safe on uninitialized queues
		 * (slots=NULL → otlp_free(NULL) is a no-op). If any
		 * queue init succeeded before the failure, its slots
		 * must be freed; otherwise they'd leak. */
		for (s = 0; s < N_SIGNALS; s++)
		{
			otlp_free(e->sig[s].pending);
			mpsc_queue_free(&e->sig[s].queue);
		}
	}
	otlp_free(e);
	return NULL;
}

void
otlp_exporter_free(otlp_exporter_t *e)
{
	int s;

	if (!e)
		return;
	/* Drain queues + free pending batches, all signals — one loop
	 * over the signal table. */
	for (s = 0; s < N_SIGNALS; s++)
		drain_signal(&e->sig[s], SIGNAL_SPECS[s].free_item);
	/* Free in-flight request. */
	if (e->in_flight)
		otlp_http_request_free(e->in_flight);
	/* Close any cached keep-alive socket. */
	if (e->keepalive_sock)
		otlp_socket_close(e->keepalive_sock);
	for (s = 0; s < N_SIGNALS; s++)
	{
		mpsc_queue_free(&e->sig[s].queue);
		otlp_free(e->sig[s].pending);
	}
	otlp_free(e->user_agent);
	otlp_free(e->service_name);
	otlp_attr_vec_free(&e->resource_attrs);
	otlp_free(e);
	return;
}

/* ── emit (any thread) ────────────────────────────────────────── */

/* Core of every emit_move variant. NULL + shutdown + push + stats
 * are signal-agnostic; only the signal kind varies. */
static otlp_status_t
emit_move_common(struct otlp_exporter *e, int kind, void *item)
{
	const struct signal_spec *sp = &SIGNAL_SPECS[kind];
	struct signal_state *sg = &e->sig[kind];
	otlp_status_t st;

	if (!e || !item)
		return OTLP_ERR_NULL;
	if (otlp_atomic_load_int(
		    &e->shutdown_requested, OTLP_MEMORY_ORDER_ACQUIRE))
	{
		/* Honor the move contract: we own the item from call entry.
		 * The docstring promises the library frees on drop. */
		sp->free_item(item);
		return OTLP_ERR_SHUTDOWN;
	}

	st = mpsc_queue_push(&sg->queue, item);
	if (st != OTLP_OK)
	{
		otlp_event_t ev = {
			.code = OTLP_EVT_QUEUE_FULL,
			.level = OTLP_LOG_WARN,
			.signal = sp->id,
			.count = 1,
			.drop_reason = OTLP_DROP_QUEUE_FULL,
		};

		sp->free_item(item);
		otlp_atomic_fetch_add_u64(
			&sg->dropped_full, 1, OTLP_MEMORY_ORDER_RELAXED);
		event_log(e, &ev);
		return st;
	}
	otlp_atomic_fetch_add_u64(&sg->emitted, 1, OTLP_MEMORY_ORDER_RELAXED);
	return OTLP_OK;
}

/* Core of every clone-variant emit. NULL + shutdown checks happen
 * BEFORE the clone (v0.5.42 symmetry), so we never allocate under
 * shutdown contention. The move variant's re-check catches the
 * race between clone and shutdown. */
static otlp_status_t
emit_clone_common(struct otlp_exporter *e, int kind, const void *item)
{
	void *clone;

	if (!e || !item)
		return OTLP_ERR_NULL;
	if (otlp_atomic_load_int(
		    &e->shutdown_requested, OTLP_MEMORY_ORDER_ACQUIRE))
		return OTLP_ERR_SHUTDOWN;
	clone = SIGNAL_SPECS[kind].clone_item(item);
	if (!clone)
		return OTLP_ERR_NOMEM;
	return emit_move_common(e, kind, clone);
}

otlp_status_t
otlp_exporter_emit_move(otlp_exporter_t *e, otlp_span_t *span)
{
	return emit_move_common(e, SIGNAL_SPAN, span);
}

otlp_status_t
otlp_exporter_emit_metric_move(otlp_exporter_t *e, otlp_metric_t *metric)
{
	return emit_move_common(e, SIGNAL_METRIC, metric);
}

otlp_status_t
otlp_exporter_emit_log_move(otlp_exporter_t *e, otlp_log_record_t *log)
{
	return emit_move_common(e, SIGNAL_LOG, log);
}

otlp_status_t
otlp_exporter_emit(otlp_exporter_t *e, const otlp_span_t *span)
{
	return emit_clone_common(e, SIGNAL_SPAN, span);
}

otlp_status_t
otlp_exporter_emit_metric(otlp_exporter_t *e, const otlp_metric_t *metric)
{
	return emit_clone_common(e, SIGNAL_METRIC, metric);
}

otlp_status_t
otlp_exporter_emit_log(otlp_exporter_t *e, const otlp_log_record_t *log)
{
	return emit_clone_common(e, SIGNAL_LOG, log);
}

/* ── tick (single thread) ─────────────────────────────────────── */

/* Clear the pending batch for whichever signal is in-flight. */
static void
clear_in_flight_batch(struct otlp_exporter *e,
	struct signal_state *sg,
	const struct signal_spec *sp)
{
	size_t i;

	for (i = 0; i < sg->pending_count; i++)
		sp->free_item(sg->pending[i]);
	sg->pending_count = 0;
	sg->first_set = false;
	/* Reset retry state for the next batch. */
	e->attempt = 0;
	e->backoff_armed = false;
}

/* Encode + start the HTTP POST for signal kind `s`. On success:
 * in_flight is set, keepalive_sock is consumed (or cleared),
 * in_flight_signal/count are populated, first_set cleared.
 * On failure: keepalive_sock is cleared (the build path closed it
 * or did not take it). */
static otlp_status_t
try_start_post(struct otlp_exporter *e, int s)
{
	struct signal_state *sg = &e->sig[s];
	otlp_status_t st;

	if (e->in_flight || sg->pending_count == 0)
		return OTLP_OK;
	st = SIGNAL_SPECS[s].build_request(&e->url,
		e->user_agent,
		e->service_name,
		e->resource_attrs.items,
		e->resource_attrs.n,
		(const void *const *) sg->pending,
		sg->pending_count,
		e->connect_timeout_ms,
		e->read_timeout_ms,
		e->keepalive_sock,
		&e->in_flight);
	if (st != OTLP_OK)
	{
		/* Build failed. The donated socket (if any) was closed by
		 * the build path. Drop our reference so we reconnect next
		 * time. */
		e->keepalive_sock = NULL;
		return st;
	}
	/* The request now owns the donated socket (if any). */
	e->keepalive_sock = NULL;
	e->in_flight_signal = s;
	e->in_flight_count = sg->pending_count;
	/* IMPORTANT: do NOT free the pending batch here. It must stay
	 * alive until the in-flight request completes successfully or
	 * is permanently dropped, so retry can re-encode it. The batch
	 * is freed in record_outcome on success / permanent-failure
	 * paths. */
	sg->first_set = false;
	return OTLP_OK;
}

/* What the in-flight HTTP exchange produced, handed to
 * record_outcome as one bundle. body/body_len are the response body
 * (NULL/0 when there is none — network failure, null transport). */
struct http_outcome
{
	int status; /* HTTP status; 0 = network-level failure */
	uint32_t retry_after_ms; /* Retry-After header value (v0.5.95) */
	const uint8_t *body;
	size_t body_len;
};

/* Surface a collector PartialSuccess report — the OTLP mechanism
 * for server-side data loss on an otherwise-successful export (the
 * collector accepted the request but rejected some items: queue
 * full, size limits, ...). Counts into the per-signal rejected_*
 * stat (when a counter is given — the sync one-shot path has none)
 * and logs WARN. The batch is NOT retried: a 200 is final, and the
 * rejected items are gone server-side. */
static void
report_partial_success(struct otlp_exporter *e,
	otlp_signal_id_t signal,
	otlp_atomic_u64 *rejected_counter,
	uint64_t count,
	const uint8_t *body,
	size_t body_len)
{
	otlp_event_t ev = {
		.code = OTLP_EVT_PARTIAL_SUCCESS,
		.level = OTLP_LOG_WARN,
		.signal = signal,
		.count = count,
	};
	int64_t rejected = 0;
	const char *msg = NULL;
	size_t msg_len = 0;

	if (!otlp_exporter_otel_decode_partial_success(
		    body, body_len, &rejected, &msg, &msg_len))
		return;
	if (rejected <= 0 && msg_len == 0)
		return;
	if (rejected > 0)
	{
		ev.rejected = (uint64_t) rejected;
		if (rejected_counter)
			otlp_atomic_fetch_add_u64(rejected_counter,
				(uint64_t) rejected,
				OTLP_MEMORY_ORDER_RELAXED);
	}
	if (msg_len > 0 && msg)
	{
		ev.detail = msg;
		ev.detail_len = msg_len;
	}
	event_log(e, &ev);
}

static void
record_outcome(struct otlp_exporter *e, const struct http_outcome *o)
{
	struct signal_state *sg = &e->sig[e->in_flight_signal];
	const struct signal_spec *sp = &SIGNAL_SPECS[e->in_flight_signal];
	uint64_t count = e->in_flight_count;
	int http_status = o->status;

	if (http_status == 0)
	{
		/* Network-level failure (no HTTP response received).
		 * Treat as transient — same retry path as 5xx. (No
		 * Retry-After: there was no response to carry one.) */
		otlp_atomic_fetch_add_u64(
			&e->network_err, 1, OTLP_MEMORY_ORDER_RELAXED);
		e->attempt++;
		if (e->attempt > e->max_retries)
		{
			otlp_event_t ev = {
				.code = OTLP_EVT_ITEMS_DROPPED,
				.level = OTLP_LOG_ERROR,
				.signal = sp->id,
				.count = count,
				.max_retries = e->max_retries,
				.drop_reason = OTLP_DROP_MAX_RETRIES,
			};

			otlp_atomic_fetch_add_u64(&sg->dropped_err,
				count,
				OTLP_MEMORY_ORDER_RELAXED);
			event_log(e, &ev);
			clear_in_flight_batch(e, sg, sp);
			return;
		}
		{
			struct otlp_retry_cfg cfg = {
				e->backoff_initial_ms,
				e->backoff_max_ms,
			};
			uint32_t delay = otlp_retry_delay_ms(
				&e->jitter_prng, e->attempt, 0, &cfg, NULL);
			otlp_event_t ev = {
				.code = OTLP_EVT_RETRY_ARMED,
				.level = OTLP_LOG_WARN,
				.signal = sp->id,
				.attempt = e->attempt,
				.max_retries = e->max_retries,
				.delay_ms = delay,
			};

			e->backoff_deadline_mono =
				otlp_platform_now_mono_ms() + delay;
			e->backoff_armed = true;
			event_log(e, &ev);
		}
		return;
	}
	if (http_status >= 200 && http_status < 300)
	{
		otlp_atomic_fetch_add_u64(
			&e->http_2xx, 1, OTLP_MEMORY_ORDER_RELAXED);
		otlp_atomic_fetch_add_u64(
			&sg->sent, count, OTLP_MEMORY_ORDER_RELAXED);
		{
			otlp_event_t ev = {
				.code = OTLP_EVT_BATCH_SENT,
				.level = OTLP_LOG_DEBUG,
				.signal = sp->id,
				.count = count,
				.http_status = http_status,
			};

			event_log(e, &ev);
		}
		/* A 200 can still report server-side data loss via
		 * PartialSuccess in the response body (v0.5.96). */
		if (o->body && o->body_len > 0)
			report_partial_success(e,
				sp->id,
				&sg->rejected,
				count,
				o->body,
				o->body_len);
		/* Success — free the pending batch (kept across retries). */
		clear_in_flight_batch(e, sg, sp);
		return;
	}
	if (http_status == 429 || (http_status >= 500 && http_status < 600))
	{
		/* 429 is retryable but still a 4xx — count it in its own
		 * status-class bucket (http_4xx), not http_5xx. */
		otlp_atomic_fetch_add_u64(
			http_status == 429 ? &e->http_4xx : &e->http_5xx,
			1,
			OTLP_MEMORY_ORDER_RELAXED);
		e->attempt++;
		if (e->attempt > e->max_retries)
		{
			otlp_event_t ev = {
				.code = OTLP_EVT_ITEMS_DROPPED,
				.level = OTLP_LOG_ERROR,
				.signal = sp->id,
				.count = count,
				.http_status = http_status,
				.max_retries = e->max_retries,
				.drop_reason = OTLP_DROP_MAX_RETRIES,
			};

			otlp_atomic_fetch_add_u64(&sg->dropped_err,
				count,
				OTLP_MEMORY_ORDER_RELAXED);
			event_log(e, &ev);
			/* Permanent failure — free the pending batch. */
			clear_in_flight_batch(e, sg, sp);
		}
		else
		{
			/* Server-requested floor (RFC 7231 §7.1.3): never
			 * retry SOONER than Retry-After says, but never let
			 * a hostile/buggy server stall exports beyond our
			 * own cap — delay = max(jitter, Retry-After),
			 * clamped to backoff_max_ms. */
			struct otlp_retry_cfg cfg = {
				e->backoff_initial_ms,
				e->backoff_max_ms,
			};
			bool server_driven = false;
			uint32_t delay = otlp_retry_delay_ms(&e->jitter_prng,
				e->attempt,
				o->retry_after_ms,
				&cfg,
				&server_driven);
			{
				otlp_event_t ev = {
					.code = OTLP_EVT_RETRY_ARMED,
					.level = OTLP_LOG_WARN,
					.signal = sp->id,
					.http_status = http_status,
					.attempt = e->attempt,
					.max_retries = e->max_retries,
					.delay_ms = delay,
					.server_driven = server_driven,
				};

				e->backoff_deadline_mono =
					otlp_platform_now_mono_ms() + delay;
				e->backoff_armed = true;
				event_log(e, &ev);
			}
		}
		return;
	}
	/* Permanent 4xx (non-429). */
	otlp_atomic_fetch_add_u64(&e->http_4xx, 1, OTLP_MEMORY_ORDER_RELAXED);
	otlp_atomic_fetch_add_u64(
		&sg->dropped_err, count, OTLP_MEMORY_ORDER_RELAXED);
	{
		otlp_event_t ev = {
			.code = OTLP_EVT_ITEMS_DROPPED,
			.level = OTLP_LOG_ERROR,
			.signal = sp->id,
			.count = count,
			.http_status = http_status,
			.drop_reason = OTLP_DROP_HTTP_STATUS,
		};

		event_log(e, &ev);
	}
	/* Permanent failure — free the pending batch. */
	clear_in_flight_batch(e, sg, sp);
}

otlp_status_t
otlp_exporter_tick(struct otlp_exporter *e, uint32_t max_wait_ms)
{
	uint64_t deadline;
	bool work_done;
	int s;

	deadline = otlp_platform_now_mono_ms() + max_wait_ms;

	do
	{
		work_done = false;

		/* 1. Drain all three queues into their pending arrays. */
		for (s = 0; s < 3; s++)
		{
			while (e->sig[s].pending_count < e->sig[s].pending_cap)
			{
				void *item = mpsc_queue_pop(&e->sig[s].queue);

				if (!item)
					break;
				e->sig[s].pending[e->sig[s].pending_count++] =
					item;
				if (!e->sig[s].first_set)
				{
					e->sig[s].first_mono =
						otlp_platform_now_mono_ms();
					e->sig[s].first_set = true;
				}
				work_done = true;
			}
		}

		/* 2. Null-transport: try signals by priority. */
		if (e->null_transport && !e->backoff_armed)
		{
			for (s = 0; s < 3; s++)
			{
				if (e->sig[s].pending_count > 0)
				{
					int http_status = 200;
					struct http_outcome o = { 0 };

					e->in_flight_signal = s;
					e->in_flight_count =
						e->sig[s].pending_count;
					if (e->null_transport_status_fn)
						http_status = e->null_transport_status_fn(
							e->null_transport_status_ctx);
					o.status = http_status;
					record_outcome(e, &o);
					work_done = true;
					goto tick_continue;
				}
			}
		}

		/* 3. Start POST if batch ready (by priority). */
		if (!e->in_flight && !e->backoff_armed)
		{
			bool shutdown =
				otlp_atomic_load_int(&e->shutdown_requested,
					OTLP_MEMORY_ORDER_RELAXED);
			uint64_t now_ms = otlp_platform_now_mono_ms();

			for (s = 0; s < 3; s++)
			{
				if (e->in_flight || e->backoff_armed)
					break;
				if (e->sig[s].pending_count >= e->batch_size ||
					(e->sig[s].first_set &&
						now_ms - e->sig[s].first_mono >=
							e->batch_ms) ||
					(shutdown &&
						e->sig[s].pending_count > 0))
				{
					if (try_start_post(e, s) == OTLP_OK)
						work_done = true;
				}
			}
		}

		/* 4. Step in-flight request. */
		if (e->in_flight)
		{
			otlp_status_t st = otlp_http_request_step(e->in_flight);
			otlp_http_req_state_t s2 =
				otlp_http_request_state(e->in_flight);

			if (s2 == OTLP_HTTP_REQ_DONE ||
				s2 == OTLP_HTTP_REQ_FAILED)
			{
				/* Read everything out BEFORE the free — the
				 * values live in the request's parsed state. */
				struct http_outcome o = { 0 };

				if (s2 == OTLP_HTTP_REQ_DONE)
				{
					o.status =
						otlp_http_request_http_status(
							e->in_flight);
					o.retry_after_ms =
						otlp_http_request_retry_after_ms(
							e->in_flight);
					o.body = otlp_http_request_body(
						e->in_flight, &o.body_len);
					e->keepalive_sock =
						otlp_http_request_detach_socket(
							e->in_flight);
				}
				record_outcome(e, &o);
				otlp_http_request_free(e->in_flight);
				e->in_flight = NULL;
				e->in_flight_count = 0;
				if (o.status != 0 &&
					(o.status < 200 || o.status >= 300) &&
					e->keepalive_sock)
				{
					otlp_socket_close(e->keepalive_sock);
					e->keepalive_sock = NULL;
				}
			}
			work_done = true;
			(void) st;
		}

		/* 5. Backoff retry. */
		if (e->backoff_armed && !e->in_flight &&
			otlp_platform_now_mono_ms() >= e->backoff_deadline_mono)
		{
			e->backoff_armed = false;
			/* When null_transport is enabled, skip the HTTP
			 * retry — the next tick iteration's null-transport
			 * path (step 2) handles the retry cleanly. Without
			 * this check, the HTTP retry + null-transport
			 * double-processes the batch (double-counting in
			 * stats). */
			if (!e->null_transport)
				try_start_post(e, e->in_flight_signal);
			if (e->in_flight)
				work_done = true;
		}

		/* 6. Sleep if waiting on backoff. */
		if (!work_done && e->backoff_armed && !e->in_flight)
		{
#if defined(_WIN32)
			Sleep(1);
#else
			struct timespec ts = { 0, 1 * 1000 * 1000 };
			nanosleep(&ts, NULL);
#endif
		}

	tick_continue:;
	} while (work_done && otlp_platform_now_mono_ms() < deadline);

	return OTLP_OK;
}

otlp_status_t
otlp_exporter_flush(otlp_exporter_t *e)
{
	uint64_t deadline;
	uint64_t now;

	if (!e)
		return OTLP_ERR_NULL;
	deadline = otlp_platform_now_mono_ms() + e->flush_timeout_ms;

	do
	{
		otlp_exporter_tick(e, 100);
		now = otlp_platform_now_mono_ms();
	} while ((e->sig[SIGNAL_SPAN].pending_count > 0 || e->in_flight ||
			 mpsc_queue_size(&e->sig[SIGNAL_SPAN].queue) > 0 ||
			 e->sig[SIGNAL_METRIC].pending_count > 0 ||
			 mpsc_queue_size(&e->sig[SIGNAL_METRIC].queue) > 0 ||
			 e->sig[SIGNAL_LOG].pending_count > 0 ||
			 mpsc_queue_size(&e->sig[SIGNAL_LOG].queue) > 0) &&
		now < deadline);

	/* The return-status check must match the loop condition. The
	 * loop exits on "no work" OR "deadline reached"; if the
	 * deadline was reached with items still queued (drain cap hit,
	 * or a tight race after a POST completion cleared pending but
	 * before the next drain), the user needs to know items remain.
	 * Pre-v0.5.58 this check omitted the queue sizes, so flush
	 * could silently return OK with unsent items. */
	if (e->sig[SIGNAL_SPAN].pending_count > 0 || e->in_flight ||
		e->sig[SIGNAL_METRIC].pending_count > 0 ||
		e->sig[SIGNAL_LOG].pending_count > 0 ||
		mpsc_queue_size(&e->sig[SIGNAL_SPAN].queue) > 0 ||
		mpsc_queue_size(&e->sig[SIGNAL_METRIC].queue) > 0 ||
		mpsc_queue_size(&e->sig[SIGNAL_LOG].queue) > 0)
		return OTLP_ERR_NETWORK;
	return OTLP_OK;
}

void
otlp_exporter_set_null_transport(otlp_exporter_t *e, bool enabled)
{
	if (e)
		e->null_transport = enabled;
}

void
otlp_exporter_set_null_transport_status_fn(otlp_exporter_t *e,
	otlp_null_transport_status_fn fn,
	void *ctx)
{
	if (e)
	{
		e->null_transport_status_fn = fn;
		e->null_transport_status_ctx = ctx;
	}
}

void
otlp_exporter_set_logger(otlp_exporter_t *e, otlp_log_fn fn, void *ctx)
{
	if (e)
	{
		e->log_fn = fn;
		e->log_ctx = ctx;
	}
}

void
otlp_exporter_set_event_logger(otlp_exporter_t *e, otlp_event_fn fn, void *ctx)
{
	if (e)
	{
		e->event_fn = fn;
		e->event_ctx = ctx;
	}
}

/* ── Synchronous metric / log flush ───────────────────────────── */

/* One POST attempt. Returns OTLP_OK on 2xx; sets *got_response
 * when the server answered at all (any HTTP status). A network-
 * level failure before a response (*got_response == false) is
 * transient — the caller retries with the async path's backoff
 * budget. A non-2xx response is permanent for the sync path. */
static otlp_status_t
flush_post_once(struct otlp_exporter *e,
	otlp_signal_id_t signal,
	const struct otlp_http_url *url,
	const char *path,
	const uint8_t *body,
	size_t body_len,
	bool *got_response)
{
	otlp_http_request_t *req = NULL;
	otlp_status_t st;
	uint64_t deadline;
	uint64_t now;
	struct otlp_http_url u;

	u = *url;
	snprintf(u.path, sizeof(u.path), "%s", path);
	st = otlp_http_request_start(&req,
		&u,
		e->user_agent,
		body,
		body_len,
		e->connect_timeout_ms,
		e->read_timeout_ms);
	if (st != OTLP_OK)
	{
		otlp_event_t ev = {
			.code = OTLP_EVT_SYNC_FLUSH_FAILED,
			.level = OTLP_LOG_ERROR,
			.signal = signal,
			.status = st,
		};

		event_log(e, &ev);
		return st;
	}
	deadline = otlp_platform_now_mono_ms() + e->flush_timeout_ms;
	for (;;)
	{
		st = otlp_http_request_step(req);
		otlp_http_req_state_t s = otlp_http_request_state(req);

		if (s == OTLP_HTTP_REQ_DONE)
		{
			int http = otlp_http_request_http_status(req);

			if (http >= 200 && http < 300)
			{
				/* Surface server-reported data loss, if any
				 * (no stats counter on the one-shot path —
				 * the message is the observability surface).
				 * path + 5 skips "/v1/" to the signal name. */
				size_t blen = 0;
				const uint8_t *b =
					otlp_http_request_body(req, &blen);

				if (b && blen > 0)
					report_partial_success(
						e, signal, NULL, 0, b, blen);
			}
			otlp_http_request_free(req);
			*got_response = true;
			if (http >= 200 && http < 300)
				return OTLP_OK;
			otlp_event_t ev = {
				.code = OTLP_EVT_SYNC_FLUSH_FAILED,
				.level = OTLP_LOG_ERROR,
				.signal = signal,
				.http_status = http,
			};

			event_log(e, &ev);
			return OTLP_ERR_NETWORK;
		}
		if (s == OTLP_HTTP_REQ_FAILED)
		{
			otlp_event_t ev = {
				.code = OTLP_EVT_SYNC_FLUSH_FAILED,
				.level = OTLP_LOG_ERROR,
				.signal = signal,
				.status = OTLP_ERR_NETWORK,
			};

			otlp_http_request_free(req);
			event_log(e, &ev);
			return OTLP_ERR_NETWORK;
		}
		if (st != OTLP_OK && st != OTLP_ERR_WOULDBLOCK)
		{
			otlp_event_t ev = {
				.code = OTLP_EVT_SYNC_FLUSH_FAILED,
				.level = OTLP_LOG_ERROR,
				.signal = signal,
				.status = st,
			};

			otlp_http_request_free(req);
			event_log(e, &ev);
			return st;
		}
		now = otlp_platform_now_mono_ms();
		if (now >= deadline)
		{
			otlp_event_t ev = {
				.code = OTLP_EVT_SYNC_FLUSH_FAILED,
				.level = OTLP_LOG_ERROR,
				.signal = signal,
				.status = OTLP_ERR_TIMEOUT,
				.timeout_ms = e->flush_timeout_ms,
			};

			otlp_http_request_free(req);
			event_log(e, &ev);
			return OTLP_ERR_TIMEOUT;
		}
#if defined(_WIN32)
		Sleep(1);
#else
		{
			struct timespec ts = { 0, 1000 * 1000 };
			nanosleep(&ts, NULL);
		}
#endif
	}
	otlp_http_request_free(req);
	return OTLP_ERR_TIMEOUT;
}

static otlp_status_t
flush_sync(struct otlp_exporter *e,
	otlp_signal_id_t signal,
	const char *path,
	const uint8_t *body,
	size_t body_len)
{
	otlp_status_t st = OTLP_ERR_NETWORK;
	bool got_response = false;
	uint32_t attempt;

	if (!e || !path || (!body && body_len > 0))
		return OTLP_ERR_NULL;
	if (e->null_transport)
		return OTLP_OK;
	/* Retry transient (pre-response) network failures with the
	 * same budget the async pipeline uses. The first connect in
	 * a fresh process occasionally fails transiently (DNS/order-
	 * of-addresses, collector still warming its accept queue);
	 * the async path recovers via backoff — the sync path
	 * deserves the same resilience. Non-2xx responses and
	 * timeouts are permanent (no retry). */
	for (attempt = 0; attempt <= e->max_retries; attempt++)
	{
		st = flush_post_once(e,
			signal,
			&e->url,
			path,
			body,
			body_len,
			&got_response);
		if (st == OTLP_OK || got_response)
			return st;
		if (attempt < e->max_retries)
		{
			uint32_t delay = e->backoff_initial_ms;
			otlp_event_t ev = {
				.code = OTLP_EVT_RETRY_ARMED,
				.level = OTLP_LOG_WARN,
				.signal = signal,
				.attempt = attempt + 1,
				.max_retries = e->max_retries,
				.delay_ms = delay > 100 ? 100 : delay,
			};

			if (delay > 100)
				delay = 100; /* sync path: short backoff */
			ev.delay_ms = delay;
			event_log(e, &ev);
#if defined(_WIN32)
			Sleep(delay);
#else
			{
				struct timespec ts = { 0,
					(long) delay * 1000 * 1000 };
				nanosleep(&ts, NULL);
			}
#endif
		}
	}
	return st;
}

otlp_status_t
otlp_exporter_flush_metric(otlp_exporter_t *e, const otlp_metric_t *m)
{
	struct otlp_pb_buf body = { 0 };
	otlp_status_t st;
	const otlp_metric_t *arr[1];

	if (!e || !m)
		return OTLP_ERR_NULL;
	otlp_atomic_fetch_add_u64(
		&e->sig[SIGNAL_METRIC].emitted, 1, OTLP_MEMORY_ORDER_RELAXED);
	st = otlp_pb_buf_init(&body, 0);
	if (st != OTLP_OK)
	{
		/* Accounting invariant: emitted == sent + dropped_err.
		 * Pre-v0.5.59 this path returned without updating
		 * dropped_err, breaking the invariant under OOM. */
		otlp_atomic_fetch_add_u64(&e->sig[SIGNAL_METRIC].dropped_err,
			1,
			OTLP_MEMORY_ORDER_RELAXED);
		return st;
	}
	arr[0] = m;
	st = otlp_encode_export_metrics_service_request(&body,
		e->service_name,
		e->resource_attrs.items,
		e->resource_attrs.n,
		NULL,
		NULL,
		arr,
		1);
	if (st == OTLP_OK)
		st = flush_sync(e,
			OTLP_SIGNAL_METRICS,
			"/v1/metrics",
			body.data,
			body.len);
	otlp_pb_buf_free(&body);
	if (st == OTLP_OK)
		otlp_atomic_fetch_add_u64(&e->sig[SIGNAL_METRIC].sent,
			1,
			OTLP_MEMORY_ORDER_RELAXED);
	else
		otlp_atomic_fetch_add_u64(&e->sig[SIGNAL_METRIC].dropped_err,
			1,
			OTLP_MEMORY_ORDER_RELAXED);
	return st;
}

otlp_status_t
otlp_exporter_flush_log(otlp_exporter_t *e, const otlp_log_record_t *lr)
{
	struct otlp_pb_buf body = { 0 };
	otlp_status_t st;
	const otlp_log_record_t *arr[1];

	if (!e || !lr)
		return OTLP_ERR_NULL;
	otlp_atomic_fetch_add_u64(
		&e->sig[SIGNAL_LOG].emitted, 1, OTLP_MEMORY_ORDER_RELAXED);
	st = otlp_pb_buf_init(&body, 0);
	if (st != OTLP_OK)
	{
		/* Accounting invariant: emitted == sent + dropped_err.
		 * Pre-v0.5.59 this path returned without updating
		 * dropped_err, breaking the invariant under OOM. */
		otlp_atomic_fetch_add_u64(&e->sig[SIGNAL_LOG].dropped_err,
			1,
			OTLP_MEMORY_ORDER_RELAXED);
		return st;
	}
	arr[0] = lr;
	st = otlp_encode_export_logs_service_request(&body,
		e->service_name,
		e->resource_attrs.items,
		e->resource_attrs.n,
		NULL,
		NULL,
		arr,
		1);
	if (st == OTLP_OK)
		st = flush_sync(
			e, OTLP_SIGNAL_LOGS, "/v1/logs", body.data, body.len);
	otlp_pb_buf_free(&body);
	if (st == OTLP_OK)
		otlp_atomic_fetch_add_u64(
			&e->sig[SIGNAL_LOG].sent, 1, OTLP_MEMORY_ORDER_RELAXED);
	else
		otlp_atomic_fetch_add_u64(&e->sig[SIGNAL_LOG].dropped_err,
			1,
			OTLP_MEMORY_ORDER_RELAXED);
	return st;
}

otlp_status_t
otlp_exporter_shutdown(otlp_exporter_t *e)
{
	if (!e)
		return OTLP_ERR_NULL;
	otlp_atomic_store_int(
		&e->shutdown_requested, 1, OTLP_MEMORY_ORDER_RELEASE);
	return OTLP_OK;
}

otlp_status_t
otlp_exporter_poll_fds(otlp_exporter_t *e,
	otlp_poll_fd_t *out,
	size_t cap,
	size_t *n_out)
{
	if (!e || !n_out)
		return OTLP_ERR_NULL;
	/* Argument validation BEFORE the state check: out=NULL with
	 * cap>0 is a caller bug regardless of whether a request is
	 * in flight (previously returned OK+n=0 in that case — a
	 * contract wart the first poll_fds test caught). */
	if (!out && cap > 0)
	{
		*n_out = 0;
		return OTLP_ERR_NULL;
	}
	if (!e->in_flight || cap == 0)
	{
		*n_out = 0;
		return OTLP_OK;
	}
	out[0].fd = otlp_http_request_fd(e->in_flight);
	out[0].events = otlp_http_request_events(e->in_flight);
	*n_out = 1;
	return OTLP_OK;
}

otlp_status_t
otlp_exporter_get_stats(otlp_exporter_t *e, otlp_exporter_stats_t *out)
{
	int k;

	if (!e || !out)
		return OTLP_ERR_NULL;
	for (k = 0; k < N_SIGNALS; k++)
	{
		otlp_exporter_stats_t *o = out;
		uint64_t emitted = otlp_atomic_load_u64(
			&e->sig[k].emitted, OTLP_MEMORY_ORDER_RELAXED);
		uint64_t dropped_full = otlp_atomic_load_u64(
			&e->sig[k].dropped_full, OTLP_MEMORY_ORDER_RELAXED);
		uint64_t dropped_err = otlp_atomic_load_u64(
			&e->sig[k].dropped_err, OTLP_MEMORY_ORDER_RELAXED);
		uint64_t sent = otlp_atomic_load_u64(
			&e->sig[k].sent, OTLP_MEMORY_ORDER_RELAXED);
		uint64_t rejected = otlp_atomic_load_u64(
			&e->sig[k].rejected, OTLP_MEMORY_ORDER_RELAXED);

		switch (k)
		{
			case SIGNAL_SPAN:
				o->emitted = emitted;
				o->dropped_full = dropped_full;
				o->dropped_err = dropped_err;
				o->sent = sent;
				o->rejected_spans = rejected;
				break;
			case SIGNAL_METRIC:
				o->emitted_metrics = emitted;
				o->dropped_metrics_full = dropped_full;
				o->dropped_metrics_err = dropped_err;
				o->sent_metrics = sent;
				o->rejected_metrics = rejected;
				break;
			case SIGNAL_LOG:
				o->emitted_logs = emitted;
				o->dropped_logs_full = dropped_full;
				o->dropped_logs_err = dropped_err;
				o->sent_logs = sent;
				o->rejected_logs = rejected;
				break;
		}
	}
	out->http_2xx =
		otlp_atomic_load_u64(&e->http_2xx, OTLP_MEMORY_ORDER_RELAXED);
	out->http_4xx =
		otlp_atomic_load_u64(&e->http_4xx, OTLP_MEMORY_ORDER_RELAXED);
	out->http_5xx =
		otlp_atomic_load_u64(&e->http_5xx, OTLP_MEMORY_ORDER_RELAXED);
	out->network_err = otlp_atomic_load_u64(
		&e->network_err, OTLP_MEMORY_ORDER_RELAXED);
	return OTLP_OK;
}
