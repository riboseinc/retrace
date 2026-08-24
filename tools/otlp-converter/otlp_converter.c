/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * retrace-to-otlp: converts retrace's JSON-lines log to OTLP/JSON
 * (TODO.complete/21 MVP).
 *
 * Reads retrace log entries from stdin, extracts call_real timing
 * entries (which contain func + call_duration_us + ret_val), and
 * wraps each as an OpenTelemetry span in OTLP/JSON format.
 *
 * Usage:
 *   retrace run -- /bin/ls 2>&1 | retrace-to-otlp > spans.json
 *   # Then feed to otelcol or any OTLP-compatible collector:
 *   curl -X POST http://localhost:4318/v1/traces \
 *     -H "Content-Type: application/json" \
 *     -d @spans.json
 *
 * The converter generates:
 *   - One traceId per process (deterministic: derived from the
 *     first entry's timestamp, padded to 32 hex chars)
 *   - One spanId per call_real entry (sequential counter as
 *     16 hex chars)
 *   - SPAN_KIND_INTERNAL for all spans
 *   - startTimeUnixNano / endTimeUnixNano from the entry's time
 *     field + call_duration_us
 *
 * This is a post-processing tool -- no engine change, no runtime
 * overhead. The OTLP/JSON format is accepted by otelcol's
 * HTTP receiver (Content-Type: application/json on /v1/traces).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "parson.h"

#include "otlp-c/exporter.h"
#include "otlp-c/span.h"
#include "otlp-c/tracer.h"

/*
 * Wave A (TODO.trace-profile/30): --endpoint URL switches from
 * OTLP/JSON-on-stdout to REAL protobuf export through the
 * vendored otlp-c client (POST /v1/traces, keep-alive). The
 * stdout mode stays (pipes, tests, offline inspection).
 */
static const char *g_endpoint;

static const char *g_trace_id = "00000000000000000000000000000001";

/* Generate a 16-hex-char spanId from a counter. */
static void make_span_id(unsigned long counter, char *buf, size_t cap)
{
	snprintf(buf, cap, "%016lx", counter);
}

/* Check if a JSON object has the call_real timing fields. */
static int is_timing_entry(const JSON_Object *msg)
{
	return json_object_has_value(msg, "func") &&
	       json_object_has_value(msg, "call_duration_us");
}

/* Read all of stdin into a buffer. */
static char *read_stdin(size_t *out_len)
{
	size_t cap = 65536;
	size_t len = 0;
	char *buf = (char *)malloc(cap);

	if (buf == NULL)
		return NULL;

	while (1) {
		size_t n;

		if (len + 4096 > cap) {
			char *new_buf;

			cap *= 2;
			new_buf = (char *)realloc(buf, cap);
			if (new_buf == NULL) {
				free(buf);
				return NULL;
			}
			buf = new_buf;
		}

		n = fread(buf + len, 1, 4096, stdin);
		len += n;

		if (n < 4096)
			break;
	}

	buf[len] = '\0';
	*out_len = len;
	return buf;
}

/* Extract span data from a call_real timing entry. */
static void emit_span(FILE *out, const JSON_Object *msg,
		      unsigned long span_counter, int *first)
{
	const char *func;
	double duration_us;
	double ret_val;
	JSON_Value *span_val;
	JSON_Object *span;
	JSON_Array *attrs;
	char span_id[20];

	func = json_object_get_string(msg, "func");
	duration_us = json_object_get_number(msg, "call_duration_us");
	ret_val = json_object_get_number(msg, "ret_val");

	make_span_id(span_counter, span_id, sizeof(span_id));

	span_val = json_value_init_object();
	span = json_value_get_object(span_val);

	json_object_set_string(span, "traceId", g_trace_id);
	json_object_set_string(span, "spanId", span_id);
	json_object_set_string(span, "name", func ? func : "unknown");
	json_object_set_number(span, "kind", 0);
	json_object_set_number(span, "startTimeUnixNano", 0);
	json_object_set_number(span, "endTimeUnixNano",
		duration_us * 1000.0);

	{
		JSON_Value *attrs_val = json_value_init_array();

		attrs = json_array(attrs_val);

		{
			JSON_Value *a1 = json_value_init_object();
			JSON_Object *o1 = json_value_get_object(a1);

			json_object_set_string(o1, "key",
				"call_duration_us");
			{
				JSON_Value *v1 = json_value_init_object();
				JSON_Object *vo1 = json_value_get_object(v1);

				json_object_set_number(vo1, "doubleValue",
					duration_us);
				json_object_set_value(o1, "value", v1);
			}
			json_array_append_value(attrs, a1);
		}

		{
			JSON_Value *a2 = json_value_init_object();
			JSON_Object *o2 = json_value_get_object(a2);

			json_object_set_string(o2, "key", "ret_val");
			{
				JSON_Value *v2 = json_value_init_object();
				JSON_Object *vo2 = json_value_get_object(v2);

				json_object_set_number(vo2, "intValue",
					ret_val);
				json_object_set_value(o2, "value", v2);
			}
			json_array_append_value(attrs, a2);
		}

		json_object_set_value(span, "attributes", attrs_val);
	}

	if (*first) {
		*first = 0;
		fprintf(out, "\t\t\t");
	} else {
		fprintf(out, ",\n\t\t\t");
	}

	{
		char *serialized = json_serialize_to_string(span_val);

		if (serialized) {
			fputs(serialized, out);
			json_free_serialized_string(serialized);
		}
	}

	json_value_free(span_val);
}

/* One trace entry -> one otlp-c span (protobuf path). */
static int emit_span_otlp(otlp_tracer_t *tracer, otlp_exporter_t *exp,
	const JSON_Object *msg, unsigned long span_counter)
{
	otlp_span_t *span;
	const char *func = json_object_get_string(msg, "func");
	double duration_us = json_object_get_number(msg, "call_duration_us");
	double ret_val = json_object_get_number(msg, "ret_val");

	span = otlp_tracer_start_span(tracer, func ? func : "unknown");
	if (span == NULL)
		return -1;
	otlp_span_set_attribute_double(span, "retrace.duration_us",
		duration_us);
	otlp_span_set_attribute_double(span, "retrace.ret_val", ret_val);
	if (otlp_exporter_emit_move(exp, span) != OTLP_OK) {
		otlp_span_free(span);
		return -1;
	}
	(void)span_counter;
	return 0;
}

int main(int argc, char **argv)
{
	int i;

	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--endpoint") == 0 && i + 1 < argc)
			g_endpoint = argv[++i];
	}
	if (g_endpoint != NULL) {
		otlp_exporter_opts_t opts;
		otlp_exporter_t *exp;
		otlp_tracer_t *tracer;
		char *buf;
		size_t len;
		JSON_Value *root;
		JSON_Array *arr;
		unsigned long counter = 0;
		size_t k;

		memset(&opts, 0, sizeof(opts));
		opts.endpoint = g_endpoint;
		opts.service_name = "retrace";
		exp = otlp_exporter_create(&opts);
		if (exp == NULL) {
			fprintf(stderr, "retrace-to-otlp: exporter create failed\n");
			return 2;
		}
		tracer = otlp_tracer_create("retrace", NULL, NULL);

		buf = read_stdin(&len);
		if (buf == NULL)
			return 2;
		root = json_parse_string(buf);
		free(buf);
		if (root == NULL) {
			fprintf(stderr, "retrace-to-otlp: parse failed\n");
			return 2;
		}
		arr = json_value_get_array(root);
		for (k = 0; k < json_array_get_count(arr); k++) {
			JSON_Object *entry = json_array_get_object(arr, k);
			JSON_Object *msg = entry != NULL ?
				json_object_get_object(entry, "message") :
				NULL;

			if (msg != NULL && is_timing_entry(msg)) {
				if (emit_span_otlp(tracer, exp, msg,
						   ++counter) != 0)
					break;
			}
		}
		json_value_free(root);

		otlp_exporter_flush(exp);
		otlp_exporter_shutdown(exp);
		otlp_exporter_free(exp);
		if (tracer != NULL)
			otlp_tracer_free(tracer);
		fprintf(stderr, "retrace-to-otlp: %lu spans -> %s\n",
			counter, g_endpoint);
		return 0;
	}
	{
	char *input;
	size_t input_len;
	JSON_Value *root_val;
	JSON_Array *entries;
	size_t i, n;
	unsigned long span_counter = 1;
	int first_span = 1;

	input = read_stdin(&input_len);
	if (input == NULL) {
		fprintf(stderr, "retrace-to-otlp: failed to read stdin\n");
		return 1;
	}

	root_val = json_parse_string(input);
	free(input);

	if (root_val == NULL) {
		fprintf(stderr, "retrace-to-otlp: invalid JSON input\n");
		return 1;
	}

	if (json_value_get_type(root_val) != JSONArray) {
		fprintf(stderr,
			"retrace-to-otlp: expected JSON array, got %d\n",
			json_value_get_type(root_val));
		json_value_free(root_val);
		return 1;
	}

	entries = json_array(root_val);
	n = json_array_get_count(entries);

	fprintf(stdout, "{\n");
	fprintf(stdout, "\t\"resourceSpans\": [{\n");
	fprintf(stdout, "\t\t\"resource\": {\n");
	fprintf(stdout, "\t\t\t\"attributes\": [{\n");
	fprintf(stdout, "\t\t\t\t\"key\": \"service.name\",\n");
	fprintf(stdout,
		"\t\t\t\t\"value\": {\"stringValue\": \"retrace\"}\n");
	fprintf(stdout, "\t\t\t}]\n");
	fprintf(stdout, "\t\t},\n");
	fprintf(stdout, "\t\t\"scopeSpans\": [{\n");
	fprintf(stdout, "\t\t\t\"scope\": {\"name\": \"retrace\"},\n");
	fprintf(stdout, "\t\t\t\"spans\": [\n");

	for (i = 0; i < n; i++) {
		JSON_Object *entry = json_array_get_object(entries, i);
		JSON_Object *msg;

		if (entry == NULL)
			continue;

		msg = json_object_get_object(entry, "message");
		if (msg == NULL)
			continue;

		if (is_timing_entry(msg)) {
			emit_span(stdout, msg, span_counter, &first_span);
			span_counter++;
		}
	}

	if (!first_span)
		fprintf(stdout, "\n");

	fprintf(stdout, "\t\t\t]\n");
	fprintf(stdout, "\t\t}]\n");
	fprintf(stdout, "\t}]\n");
	fprintf(stdout, "}\n");

	json_value_free(root_val);

	fprintf(stderr,
		"retrace-to-otlp: converted %lu spans\n",
		span_counter - 1);

	return 0;
	}
}
