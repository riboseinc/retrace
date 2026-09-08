/*
 * Copyright (c) 2017, [Ribose Inc](https://ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

#ifndef RETRACE_CORE_REDACT_H_
#define RETRACE_CORE_REDACT_H_

#include <stddef.h>

/*
 * Evidence redaction (TODO.impl/01): secrets never leave the
 * process. Patterns compile once (config or env); every
 * evidence payload passes through retrace_redact_apply at the
 * two fan-out points -- the logger's emit and the agent event
 * builder -- so stdout, the logfile, every sink (OTLP rides
 * the logger seam), and the daemon journal all inherit one
 * transform.
 *
 * Zero-delta contract: with no patterns compiled, active() is
 * false and callers skip the transform entirely -- one load on
 * the evidence hot path.
 */

/* Compile and own copies of the patterns (max 32, each <= 127
 * chars, '*' is a wildcard run). Replaces any previous set.
 */
void retrace_redact_set(const char *const *patterns, size_t n);

/* Env form: comma-separated (RETRACE_REDACT="*_TOKEN,AWS_*") */
void retrace_redact_set_csv(const char *csv);

/* 0 when no patterns are compiled (the fast-path gate) */
int retrace_redact_active(void);

/* In-place: every substring matching a pattern becomes "***".
 * Bounded work: at most 64 replacements per buffer. The caller
 * owns the buffer; safe on the evidence threads (pure C, no
 * libc, read-only pattern state after set).
 */
void retrace_redact_apply(char *buf);

#endif /* RETRACE_CORE_REDACT_H_ */
