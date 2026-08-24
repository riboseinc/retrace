/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Exporter — batches spans and POSTs them to an OTLP collector.
 *
 * Lifetime: caller-owned. Construct via otlp_exporter_create();
 * free via otlp_exporter_free() (call shutdown first if you want
 * any pending batch to flush).
 *
 * Caller-driven I/O: the library never spawns threads. The caller
 * drives progress by calling otlp_exporter_tick() from a thread it
 * controls — its event loop, a periodic timer, a worker, etc. See
 * docs/deployment.md for embedding patterns.
 *
 * DNS note: the first tick() that opens a connection (and any
 * tick() that reconnects after a connection failure) performs a
 * blocking getaddrinfo() call. This can take up to several
 * seconds on a network with slow/broken DNS. If the caller's
 * thread cannot tolerate this latency, resolve the collector's
 * hostname to an IP address before constructing the exporter's
 * endpoint, or run tick() from a thread that can block briefly.
 * The library does not cache DNS results (the OS resolver
 * usually does).
 *
 * Thread-safety: emit() is safe to call from any thread, and so
 * is get_stats() (every counter is an atomic load; safe from any
 * thread during the exporter's lifetime). tick(), flush(),
 * shutdown(), and free() are NOT — the caller must serialise them
 * (typically by always calling from the same thread that owns the
 * exporter's lifetime).
 *
 * UTF-8: every string the library puts on the wire (attribute
 * keys/values, span/metric/log names, service_name, ...) is
 * validated as UTF-8 at the API boundary. proto3 string fields
 * must be valid UTF-8, and Go-based collectors (otelcol) reject
 * the whole request otherwise — so invalid input fails the setter
 * with OTLP_ERR_UTF8 instead of poisoning a batch. `bytes` values
 * are exempt.
 *
 * Flow:
 *   1. Caller calls otlp_exporter_emit() once per span from any
 *      thread. Spans are deep-copied into a lock-free MPSC queue.
 *   2. Caller calls otlp_exporter_tick() to drain the queue, encode
 *      a batch, and drive the in-flight HTTP POST.
 *   3. Retry with exponential backoff on transient errors.
 */
#ifndef OTLP_C_EXPORTER_H
#define OTLP_C_EXPORTER_H

#include <stddef.h>
#include <stdint.h>

#include "log.h"
#include "metric.h"
#include "span.h"
#include "status.h"
#include "visibility.h"

#ifdef __cplusplus
extern "C"
{
#endif

	typedef struct otlp_exporter otlp_exporter_t;

	/* Value type for a Resource attribute. Maps 1:1 to the OTLP
	 * AnyValue oneof variants the library supports for Resource
	 * attributes. STRING is the default (0) for backward
	 * compatibility — callers who only set `.value` get string
	 * encoding with no code change. */
	/* A key-value pair attached to the OTLP Resource of every
	 * batch this exporter emits. Resource attributes describe the
	 * process being instrumented (service.version,
	 * deployment.environment, host.name, process.pid, ...) and are
	 * constant for the exporter's lifetime. See the OpenTelemetry
	 * semantic-conventions spec for standard keys.
	 *
	 * Model (v0.5.92): one key + one otlp_value_t - the same value
	 * model as every other attribute surface, supporting all seven
	 * AnyValue types. The library deep-copies everything at
	 * otlp_exporter_create() time; the caller may free the input
	 * array immediately after.
	 *
	 * Map semantics (v0.5.78): duplicate keys collapse
	 * last-write-wins; a "service.name" entry is dropped when the
	 * dedicated service_name opt is set.
	 *
	 * Example:
	 *   otlp_resource_attr_t pid = {
	 *       .key = "process.pid",
	 *       .value = {.type = OTLP_VALUE_INT64,
	 *                 .v = {.int64_val = 4242}},
	 *   };
	 */
	typedef struct
	{
		const char *key;
		otlp_value_t value;
	} otlp_resource_attr_t;

	/* Configuration for otlp_exporter_create. Pass zero-initialized +
	 * fill the fields you care about; the library supplies defaults
	 * for the rest. */
	typedef struct
	{
		/* OTLP/HTTP endpoint. Default:
		 * "http://localhost:4318/v1/traces". Must include scheme + host
		 * + port + path. The library talks plain HTTP to localhost; the
		 * otelcol sidecar terminates TLS to the real backend. See
		 * docs/deployment.md. */
		const char *endpoint;

		/* Service name attached to every batch's Resource. Default: "".
		 * Override per-tracer if you need different service names for
		 * different spans. */
		const char *service_name;

		/* Additional Resource attributes emitted on every batch
		 * alongside service.name (e.g., service.version,
		 * deployment.environment, host.name). The library copies
		 * these at create time; the caller may free the array
		 * immediately after otlp_exporter_create() returns.
		 *
		 * service.name (from the service_name field above) is always
		 * emitted first as a Resource attribute; entries in this
		 * array follow in order. May be NULL (n_resource_attributes
		 * is then ignored). */
		const otlp_resource_attr_t *resource_attributes;
		size_t n_resource_attributes;

		/* Max spans per HTTP request. Default: 512. */
		size_t batch_size;

		/* Max milliseconds to wait before flushing a partial batch.
		 * Default: 100. */
		uint32_t batch_ms;

		/* Max retry attempts on transient errors (429, 5xx, network).
		 * Default: 5. */
		uint32_t max_retries;

		/* Initial backoff in milliseconds. Default: 1000 (1s). */
		uint32_t backoff_initial_ms;

		/* Max backoff in milliseconds. Default: 30000 (30s).
		 *
		 * Also the ceiling for server-directed retry pacing: on a
		 * throttled response (429/503/5xx) carrying Retry-After
		 * (delta-seconds form), the next attempt waits
		 * max(jittered backoff, Retry-After) — never sooner than
		 * the server asked — but never longer than this cap, so a
		 * hostile server cannot stall exports indefinitely. */
		uint32_t backoff_max_ms;

		/* Connect timeout in milliseconds. Default: 5000 (5s). */
		uint32_t connect_timeout_ms;

		/* Read timeout in milliseconds. Default: 10000 (10s). */
		uint32_t read_timeout_ms;

		/* Max wall-clock milliseconds for otlp_exporter_flush() and
		 * the synchronous metric/log flush paths before they give up
		 * and return OTLP_ERR_NETWORK (flush) or OTLP_ERR_TIMEOUT
		 * (flush_metric/flush_log). Covers the case where the
		 * collector is unreachable or slow and the caller wants
		 * bounded shutdown latency. Default: 30000 (30s). Set to
		 * UINT32_MAX for effectively unbounded (callers wanting
		 * longer should loop tick() manually instead). */
		uint32_t flush_timeout_ms;

		/* User-Agent header. Default: "otlp-c/<version>". */
		const char *user_agent;

		/* MPSC queue capacity (must be power of 2). Default: 4096. */
		size_t queue_capacity;
	} otlp_exporter_opts_t;

	/* Construct an exporter. The opts are copied; the caller may free
	 * them after this returns. Returns NULL on allocation failure.
	 *
	 * The constructor does NOT open a socket. The first tick() after
	 * emit() triggers the network I/O. */
	OTLP_C_EXPORT
	otlp_exporter_t *otlp_exporter_create(const otlp_exporter_opts_t *opts);

	/* Free the exporter. Drains and frees anything still queued
	 * or batched (the safe order is shutdown() -> drain via tick()
	 * / flush() -> free()).
	 *
	 * CONCURRENCY CONTRACT: shutdown() is a cooperative stop
	 * signal, NOT a barrier — emit*() calls that already passed
	 * the shutdown check may still return OTLP_OK and enqueue for
	 * a short window after shutdown() returns. The caller MUST
	 * ensure no emit*() is executing (or called afterwards) by
	 * the time free() runs: join producer threads after they
	 * observe OTLP_ERR_SHUTDOWN before freeing. Violating this is
	 * a use-after-free on the queues. */
	OTLP_C_EXPORT
	void otlp_exporter_free(otlp_exporter_t *exp);

	/* Add a span to the queue. Safe to call from any thread.
	 * The span is deep-copied; the caller may free or reuse it
	 * immediately.
	 *
	 * Returns:
	 *   OTLP_OK on success.
	 *   OTLP_ERR_NULL if exp or span is NULL.
	 *   OTLP_ERR_BUFFER_FULL if the MPSC queue is at capacity.
	 *     emitted is NOT incremented; dropped_full is.
	 *   OTLP_ERR_SHUTDOWN if otlp_exporter_shutdown was called.
	 */
	OTLP_C_EXPORT
	otlp_status_t otlp_exporter_emit(otlp_exporter_t *exp,
		const otlp_span_t *span);

	/* Take ownership of `span` and enqueue it. The exporter frees
	 * the span once it has been encoded (or dropped on shutdown).
	 * The caller MUST NOT touch, free, or re-emit the span after
	 * this call returns.
	 *
	 * Identical semantics to otlp_exporter_emit() otherwise (OK,
	 * NULL, BUFFER_FULL, SHUTDOWN). Faster because it skips the
	 * deep clone. Use this in hot paths where the span is built
	 * fresh for emission and never reused.
	 *
	 * Mixing emit() and emit_move() on the same span is a
	 * use-after-free; pass the span to exactly one of them. */
	OTLP_C_EXPORT
	otlp_status_t otlp_exporter_emit_move(otlp_exporter_t *exp,
		otlp_span_t *span);

	/* Drive exporter progress by one step. Drains the queue into a
	 * pending batch, starts or advances the in-flight HTTP request,
	 * fires batch/backoff timers. Returns when there is nothing left
	 * to do OR when max_wait_ms elapses (whichever comes first).
	 *
	 * THREAD-SAFETY: NOT safe to call concurrently from multiple
	 * threads. Pick one thread (your event loop / worker / main) and
	 * call tick from there.
	 *
	 * Returns OTLP_OK on every iteration, including when WOULDBLOCK
	 * would have been returned internally — the caller does not need
	 * to differentiate. Check get_stats() for outcome counters. */
	OTLP_C_EXPORT
	otlp_status_t otlp_exporter_tick(otlp_exporter_t *exp,
		uint32_t max_wait_ms);

	/* Flush any pending spans synchronously. Blocks the calling
	 * thread by internally looping tick() until the queue is empty
	 * and no request is in flight, or until the retry budget is
	 * exhausted. Use at clean shutdown.
	 *
	 * Returns:
	 *   OTLP_OK on success.
	 *   OTLP_ERR_* on the last failure. */
	OTLP_C_EXPORT
	otlp_status_t otlp_exporter_flush(otlp_exporter_t *exp);

	/* Signal that the exporter should stop accepting new spans.
	 * Subsequent emit() calls return OTLP_ERR_SHUTDOWN. The exporter
	 * is still owned by the caller and must be freed with
	 * otlp_exporter_free(). Pending spans are NOT auto-flushed;
	 * call tick() (or flush()) to drain them. */
	OTLP_C_EXPORT
	otlp_status_t otlp_exporter_shutdown(otlp_exporter_t *exp);

	/* Poll-fd descriptor for event-loop integration. events is
	 * POLLIN=1, POLLOUT=2 (matches <poll.h>). */
	typedef struct
	{
		int fd;
		int events;
	} otlp_poll_fd_t;

	/* Get fds + interest bits the caller should register in its event
	 * loop. Returns 0 fds if there's no in-flight request; the caller
	 * should still call tick() periodically (at least every batch_ms)
	 * to drain the queue and start new requests.
	 *
	 * THREAD-SAFETY: same as tick(). */
	OTLP_C_EXPORT
	otlp_status_t otlp_exporter_poll_fds(otlp_exporter_t *exp,
		otlp_poll_fd_t *out,
		size_t cap,
		size_t *n_out);

	/* TEST ONLY: when enabled, the exporter skips all HTTP I/O and
	 * marks batches as sent. Used by property tests to avoid threaded
	 * echo server timing flakes. Do NOT use in production. */
	OTLP_C_EXPORT
	void otlp_exporter_set_null_transport(otlp_exporter_t *exp,
		bool enabled);

	/* TEST ONLY: set a callback that determines the HTTP status code
	 * returned by each null-transport "send". Default is 200. The
	 * callback receives `ctx` and returns an HTTP status code (e.g.
	 * 500 to trigger retry behavior, then 200 for success). */
	typedef int (*otlp_null_transport_status_fn)(void *ctx);
	OTLP_C_EXPORT
	void otlp_exporter_set_null_transport_status_fn(otlp_exporter_t *exp,
		otlp_null_transport_status_fn fn,
		void *ctx);

	/* ── Diagnostics ────────────────────────────────────────────
	 *
	 * Optional callback the library invokes at notable events:
	 * batch sent, HTTP error, retry armed, span dropped (queue
	 * full or max retries), network failure. Gives the caller
	 * real-time visibility into exporter behavior for production
	 * debugging — the stats counters tell you what happened after
	 * the fact; this tells you WHY.
	 *
	 * Severity follows the standard syslog/OpenTelemetry model:
	 *   DEBUG — routine operation (batch sent successfully).
	 *   INFO  — notable but expected (retry armed).
	 *   WARN  — degraded operation (queue full, transient retry,
	 *           collector PartialSuccess: server-side data loss
	 *           reported on a 200 OK).
	 *   ERROR — unexpected failure (max retries, permanent 4xx).
	 *
	 * Thread-safety: the callback may be invoked from any thread
	 * that touches the exporter (emit from any caller thread,
	 * tick/flush from the tick thread). The implementation MUST
	 * be thread-safe. A common pattern is to write to a ring
	 * buffer or atomic flag inside the callback, then drain from
	 * a dedicated logger thread.
	 *
	 * `message` is a NUL-terminated formatted string, valid only
	 * for the duration of the call. Copy if you need it longer.
	 *
	 * Performance: when no callback is installed (default), every
	 * log site compiles to a NULL-pointer check — zero observable
	 * overhead in hot paths. */
	typedef enum
	{
		OTLP_LOG_DEBUG = 0,
		OTLP_LOG_INFO = 1,
		OTLP_LOG_WARN = 2,
		OTLP_LOG_ERROR = 3,
	} otlp_log_level_t;

	typedef void (*otlp_log_fn)(void *ctx,
		otlp_log_level_t level,
		const char *message);

	/* Install a diagnostic callback. Pass fn=NULL to disable.
	 * Default: no callback. Safe to call at any time during the
	 * exporter's lifetime; the next event observes the new
	 * callback. */
	OTLP_C_EXPORT
	void otlp_exporter_set_logger(otlp_exporter_t *exp,
		otlp_log_fn fn,
		void *ctx);

	/* ── Structured diagnostics events (v0.5.100) ──────────────
	 *
	 * The string logger above renders diagnostics for humans;
	 * this callback delivers the same diagnostics as DATA.
	 * otlp_event_t is the single model behind every diagnostic
	 * the exporter emits — the string messages are DERIVED from
	 * it by one formatter, so the two views cannot diverge.
	 * Install either, both, or neither.
	 *
	 * Programmatic consumers (metrics, alerting, self-telemetry)
	 * should prefer this surface: no string parsing, stable enum
	 * codes, exact counts.
	 *
	 * Thread-safety: same contract as the string logger — the
	 * callback may fire from any thread that touches the exporter
	 * (emit* from producer threads, tick/flush from the tick
	 * thread) and MUST be thread-safe.
	 *
	 * `detail` (PARTIAL_SUCCESS) is NOT NUL-terminated and points
	 * into the response body — valid only for the duration of the
	 * call; copy (pairing with detail_len) if needed. All other
	 * fields are values. */
	typedef enum
	{
		OTLP_SIGNAL_TRACES = 0,
		OTLP_SIGNAL_METRICS = 1,
		OTLP_SIGNAL_LOGS = 2,
	} otlp_signal_id_t;

	/* Why items were lost. */
	typedef enum
	{
		OTLP_DROP_MAX_RETRIES = 1, /* retry budget exhausted */
		OTLP_DROP_HTTP_STATUS = 2, /* permanent non-2xx (non-429) */
		OTLP_DROP_QUEUE_FULL = 3, /* MPSC queue at capacity */
	} otlp_drop_reason_t;

	typedef enum
	{
		OTLP_EVT_QUEUE_FULL = 1, /* emit() dropped 1 item */
		OTLP_EVT_BATCH_SENT = 2, /* a batch reached 2xx */
		OTLP_EVT_RETRY_ARMED = 3, /* transient failure; backoff
					   * or Retry-After wait armed */
		OTLP_EVT_ITEMS_DROPPED = 4, /* items lost permanently */
		OTLP_EVT_PARTIAL_SUCCESS = 5, /* 200 with server-reported
					       * rejections */
		OTLP_EVT_SYNC_FLUSH_FAILED = 6, /* one-shot
						 * flush_metric/flush_log
						 * error */
	} otlp_event_code_t;

	typedef struct
	{
		otlp_event_code_t code;
		otlp_log_level_t level;
		otlp_signal_id_t signal;
		uint64_t count; /* items affected (1 on QUEUE_FULL) */
		uint64_t rejected; /* PARTIAL_SUCCESS: server-reported
				    * rejected count */
		int http_status; /* 0 = no response (network-level) */
		unsigned attempt; /* RETRY_ARMED: 1-based attempt */
		unsigned max_retries;
		uint32_t delay_ms; /* RETRY_ARMED: armed delay */
		uint32_t timeout_ms; /* SYNC_FLUSH_FAILED: timeout case */
		otlp_status_t status; /* SYNC_FLUSH_FAILED: failure st */
		otlp_drop_reason_t drop_reason; /* ITEMS_DROPPED */
		bool server_driven; /* RETRY_ARMED: Retry-After >=
				     * jittered backoff */
		const char *detail; /* PARTIAL_SUCCESS message (see
				     * above) */
		size_t detail_len;
	} otlp_event_t;

	typedef void (*otlp_event_fn)(void *ctx, const otlp_event_t *event);

	/* Install the structured-event callback. Pass fn=NULL to
	 * disable. Default: none. Safe to call at any time during the
	 * exporter's lifetime; the next event observes the new
	 * callback. */
	OTLP_C_EXPORT
	void otlp_exporter_set_event_logger(otlp_exporter_t *exp,
		otlp_event_fn fn,
		void *ctx);

	/* Synchronously encode and POST a single metric to the OTLP
	 * collector at /v1/metrics. Blocks the calling thread until
	 * the HTTP request completes (or fails). The metric is NOT
	 * enqueued — this is a one-shot synchronous export.
	 *
	 * For high-volume metric streams, prefer batching. This API
	 * is for low-frequency metric export (e.g., gauges sampled
	 * periodically, counters flushed at shutdown).
	 *
	 * Thread-safety: same as flush() — call from the exporter's
	 * owner thread.
	 *
	 * Returns:
	 *   OTLP_OK on a 2xx response.
	 *   OTLP_ERR_NULL if exp or metric is NULL.
	 *   OTLP_ERR_TIMEOUT if flush_timeout_ms elapsed first.
	 *   OTLP_ERR_NETWORK on transport failure after retries. */
	OTLP_C_EXPORT
	otlp_status_t otlp_exporter_flush_metric(otlp_exporter_t *exp,
		const otlp_metric_t *metric);

	/* Synchronously encode and POST a single log record to the OTLP
	 * collector at /v1/logs. Same semantics and return codes as
	 * flush_metric. */
	OTLP_C_EXPORT
	otlp_status_t otlp_exporter_flush_log(otlp_exporter_t *exp,
		const otlp_log_record_t *log);

	/* ── Async metric / log emission (v0.5.28+) ─────────────────
	 *
	 * These enqueue a metric/log for asynchronous batch POST via
	 * tick(), the same pipeline spans use. The caller gives up
	 * ownership of the metric/log (move semantics — the exporter
	 * frees it after encoding or on drop).
	 *
	 * Returns:
	 *   OTLP_OK             — enqueued; will be POSTed by tick().
	 *   OTLP_ERR_NULL       — exp or metric/log is NULL.
	 *   OTLP_ERR_SHUTDOWN   — shutdown() was called.
	 *   OTLP_ERR_BUFFER_FULL — queue is full; the metric/log is
	 *     FREED and dropped_metrics_full / dropped_logs_full is
	 *     incremented. (Same contract as otlp_exporter_emit_move.)
	 *
	 * Thread-safety: safe to call from any thread (lock-free MPSC). */
	OTLP_C_EXPORT
	otlp_status_t otlp_exporter_emit_metric_move(otlp_exporter_t *exp,
		otlp_metric_t *metric);

	OTLP_C_EXPORT
	otlp_status_t otlp_exporter_emit_log_move(otlp_exporter_t *exp,
		otlp_log_record_t *log);

	/* Same as emit_metric_move / emit_log_move but deep-copies
	 * the metric/log first. The caller keeps ownership and may
	 * reuse or free the original immediately. Slower than the
	 * move variant (one extra alloc); use when the caller needs
	 * the original after emit (e.g., emitting to multiple
	 * exporters). */
	OTLP_C_EXPORT
	otlp_status_t otlp_exporter_emit_metric(otlp_exporter_t *exp,
		const otlp_metric_t *metric);

	OTLP_C_EXPORT
	otlp_status_t otlp_exporter_emit_log(otlp_exporter_t *exp,
		const otlp_log_record_t *log);

	/* Diagnostic counters. All monotonically increasing.
	 *
	 * Span counters (emitted, sent, dropped_*) track SPANS only.
	 * Metric and log counters (added v0.5.28) track their
	 * respective signals. HTTP-level counters (http_2xx, http_4xx,
	 * http_5xx, network_err) are global across all signals. */
	typedef struct
	{
		uint64_t emitted; /* spans accepted by emit() */
		uint64_t
			dropped_full; /* spans dropped because queue was full */
		uint64_t dropped_err; /* spans dropped after max_retries */
		uint64_t sent; /* spans successfully POSTed */
		uint64_t http_2xx; /* HTTP responses in 2xx (all signals) */
		uint64_t http_4xx; /* HTTP responses in 4xx, INCLUDING the
				    * retryable 429 (all signals) */
		uint64_t http_5xx; /* HTTP responses in 5xx (all signals) */
		uint64_t network_err; /* network failures (all signals) */
		uint64_t emitted_metrics; /* metrics accepted by
					     emit_metric_move */
		uint64_t sent_metrics; /* metrics successfully POSTed */
		uint64_t dropped_metrics_full; /* metrics dropped: queue full */
		uint64_t dropped_metrics_err; /* metrics dropped: max retries */
		uint64_t emitted_logs; /* logs accepted by emit_log_move */
		uint64_t sent_logs; /* logs successfully POSTed */
		uint64_t dropped_logs_full; /* logs dropped: queue full */
		uint64_t dropped_logs_err; /* logs dropped: max retries */
		uint64_t rejected_spans; /* spans the collector reported
					  * rejected via PartialSuccess on
					  * a 200 OK (server-side data
					  * loss; the batch is not
					  * retried) */
		uint64_t rejected_metrics; /* same, for metrics */
		uint64_t rejected_logs; /* same, for logs */
	} otlp_exporter_stats_t;

	OTLP_C_EXPORT
	otlp_status_t otlp_exporter_get_stats(otlp_exporter_t *exp,
		otlp_exporter_stats_t *out);

#ifdef __cplusplus
}
#endif

#endif
