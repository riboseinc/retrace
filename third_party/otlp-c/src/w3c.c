/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * W3C Trace Context utilities. See include/otlp-c/w3c.h.
 */
#include <otlp-c/w3c.h>

#include "span_internal.h"

#include <stddef.h>
#include <stdint.h>

static char
hex_digit(uint8_t nibble)
{
	return nibble < 10 ? (char) ('0' + nibble) : (char) ('a' + nibble - 10);
}

static int
hex_value(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

otlp_status_t
otlp_traceparent_format_raw(const uint8_t trace_id[16],
	const uint8_t span_id[8],
	bool sampled,
	char *buf,
	size_t cap,
	size_t *out_len)
{
	size_t i;

	if (!trace_id || !span_id || !buf)
		return OTLP_ERR_NULL;
	if (cap < OTLP_TRACEPARENT_BUF_SIZE)
		return OTLP_ERR_OVERFLOW;

	/* version */
	buf[0] = '0';
	buf[1] = '0';
	buf[2] = '-';

	/* trace-id: 16 bytes → 32 hex chars */
	for (i = 0; i < 16; i++)
	{
		buf[3 + i * 2] = hex_digit((uint8_t) (trace_id[i] >> 4));
		buf[3 + i * 2 + 1] = hex_digit((uint8_t) (trace_id[i] & 0x0F));
	}
	buf[35] = '-';

	/* span-id: 8 bytes → 16 hex chars */
	for (i = 0; i < 8; i++)
	{
		buf[36 + i * 2] = hex_digit((uint8_t) (span_id[i] >> 4));
		buf[36 + i * 2 + 1] = hex_digit((uint8_t) (span_id[i] & 0x0F));
	}
	buf[52] = '-';

	/* trace-flags: 1 byte → 2 hex chars (bit 0 = sampled) */
	buf[53] = '0';
	buf[54] = sampled ? '1' : '0';
	buf[55] = '\0';

	if (out_len)
		*out_len = OTLP_TRACEPARENT_LEN;
	return OTLP_OK;
}

otlp_status_t
otlp_traceparent_format(const otlp_span_t *span,
	bool sampled,
	char *buf,
	size_t cap,
	size_t *out_len)
{
	const uint8_t *trace_id;
	const uint8_t *span_id;

	if (!span)
		return OTLP_ERR_NULL;

	trace_id = otlp_span_get_trace_id(span);
	span_id = otlp_span_get_span_id(span);
	if (!trace_id || !span_id)
		return OTLP_ERR_INVALID_ARGUMENT;

	return otlp_traceparent_format_raw(
		trace_id, span_id, sampled, buf, cap, out_len);
}

otlp_status_t
otlp_traceparent_parse(const char *header,
	uint8_t trace_id[16],
	uint8_t span_id[8],
	uint8_t *flags)
{
	size_t i;
	bool trace_nz = false;
	bool span_nz = false;

	if (!header || !trace_id || !span_id || !flags)
		return OTLP_ERR_NULL;

	/* Validate version (2 hex chars + '-'). */
	if (hex_value(header[0]) < 0 || hex_value(header[1]) < 0)
		return OTLP_ERR_INVALID_ARGUMENT;
	if (header[2] != '-')
		return OTLP_ERR_INVALID_ARGUMENT;

	/* Parse trace-id (32 hex chars). */
	for (i = 0; i < 16; i++)
	{
		int hi = hex_value(header[3 + i * 2]);
		int lo = hex_value(header[4 + i * 2]);

		if (hi < 0 || lo < 0)
			return OTLP_ERR_INVALID_ARGUMENT;
		trace_id[i] = (uint8_t) ((hi << 4) | lo);
		if (trace_id[i] != 0)
			trace_nz = true;
	}
	if (header[35] != '-')
		return OTLP_ERR_INVALID_ARGUMENT;

	/* Parse span-id (16 hex chars). */
	for (i = 0; i < 8; i++)
	{
		int hi = hex_value(header[36 + i * 2]);
		int lo = hex_value(header[37 + i * 2]);

		if (hi < 0 || lo < 0)
			return OTLP_ERR_INVALID_ARGUMENT;
		span_id[i] = (uint8_t) ((hi << 4) | lo);
		if (span_id[i] != 0)
			span_nz = true;
	}
	if (header[52] != '-')
		return OTLP_ERR_INVALID_ARGUMENT;

	/* Parse trace-flags (2 hex chars). */
	{
		int hi = hex_value(header[53]);
		int lo = hex_value(header[54]);

		if (hi < 0 || lo < 0)
			return OTLP_ERR_INVALID_ARGUMENT;
		*flags = (uint8_t) ((hi << 4) | lo);
	}

	/* W3C: trace-id and span-id must not be all-zero. */
	if (!trace_nz || !span_nz)
		return OTLP_ERR_INVALID_ARGUMENT;

	/* Version rules (W3C Trace Context §3.3.2):
	 * - version 0xff is invalid outright;
	 * - version 00 is exactly 4 fields — any trailing content
	 *   makes the header invalid;
	 * - future versions may carry additional fields, which this
	 *   parser ignores (forward compatibility). header[55] is at
	 *   most the NUL terminator here (the flags chars passed), so
	 *   the read stays in bounds. */
	{
		int version = hex_value(header[0]) * 16 + hex_value(header[1]);

		if (version == 0xff)
			return OTLP_ERR_INVALID_ARGUMENT;
		if (version == 0x00 && header[55] != '\0')
			return OTLP_ERR_INVALID_ARGUMENT;
	}

	return OTLP_OK;
}
