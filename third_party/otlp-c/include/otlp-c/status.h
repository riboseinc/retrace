/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Status and error codes. Every fallible public function returns an
 * otlp_status_t. OTLP_OK is 0; errors are negative to make the
 * common case (success) cheap to check.
 *
 * Conventions:
 *   - Always check the return value.
 *   - Treat unknown negative values as "unknown error".
 *   - Network errors are recoverable (retry). Argument errors are
 *     not (the caller is buggy).
 */
#ifndef OTLP_C_STATUS_H
#define OTLP_C_STATUS_H

typedef enum
{
	OTLP_OK = 0,

	/* Caller errors (not recoverable). */
	OTLP_ERR_INVALID_ARGUMENT = -1,
	OTLP_ERR_NOMEM = -2,
	OTLP_ERR_NULL = -3,
	OTLP_ERR_OVERFLOW = -4,
	OTLP_ERR_UTF8 = -5, /* string input is not valid UTF-8
			     * (proto3 string contract); rejected at the
			     * setter so the collector never sees it */

	/* Network errors (recoverable with backoff). */
	OTLP_ERR_NETWORK = -10,
	OTLP_ERR_TIMEOUT = -11,
	OTLP_ERR_DNS = -12,
	OTLP_ERR_CONNECT = -13,
	OTLP_ERR_WRITE = -14,
	OTLP_ERR_READ = -15,

	/* Protocol errors (server problem; usually retry). */
	OTLP_ERR_PROTOCOL = -20,
	OTLP_ERR_INVALID_RESPONSE = -21,
	OTLP_ERR_HTTP_STATUS = -22,
	OTLP_ERR_THROTTLED = -23,
	OTLP_ERR_SERVER = -24,

	/* Library errors (recoverable; usually retry). */
	OTLP_ERR_BUFFER_FULL = -30,
	OTLP_ERR_SHUTDOWN = -31,
	OTLP_ERR_WOULDBLOCK = -32, /* non-blocking op would block; caller should
				      poll and retry */

	/* Placeholder for unimplemented code (Phase 0). */
	OTLP_ERR_NOT_IMPLEMENTED = -100
} otlp_status_t;

/* Human-readable name for a status code. Useful for logs. */
const char *
otlp_strerror(otlp_status_t status);

#endif
