/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * otlp_value_t — the public scalar AnyValue.
 *
 * A borrowed-data tagged union describing one attribute value.
 * The attribute setters copy everything they need; the caller
 * keeps ownership of the input strings/bytes and may free them
 * as soon as the setter returns.
 *
 * Array and KeyValueList attributes are set by passing arrays of
 * these values (otlp_span_set_attribute_array /
 * _kvlist and the metric/log equivalents). The public type is
 * flat (arrays of scalars); nested array/kvlist values inside
 * other arrays are not expressible through the public API.
 */
#ifndef OTLP_C_VALUE_H
#define OTLP_C_VALUE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

	typedef enum
	{
		OTLP_VALUE_STRING = 0,
		OTLP_VALUE_BOOL,
		OTLP_VALUE_INT64,
		OTLP_VALUE_DOUBLE,
		OTLP_VALUE_BYTES,
	} otlp_value_type_t;

	/* One scalar value. Set `type` and the matching union member;
	 * unread members are ignored. */
	typedef struct
	{
		otlp_value_type_t type;
		union
		{
			const char *string_val; /* NUL-terminated */
			bool bool_val;
			int64_t int64_val;
			double double_val;
			struct
			{
				const uint8_t
					*data; /* may be NULL when len==0 */
				size_t len;
			} bytes_val;
		} v;
	} otlp_value_t;

	/* One KeyValueList entry (key + scalar value). */
	typedef struct
	{
		const char *key; /* NUL-terminated, must be non-NULL */
		otlp_value_t value;
	} otlp_kv_t;

#ifdef __cplusplus
}
#endif

#endif
