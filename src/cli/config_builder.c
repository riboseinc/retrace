/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

#include "config_builder.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/*
 * Append formatted text to buf at offset *pos. Bounds-checked.
 *
 * Returns:
 *   0  on success (some text written, *pos advanced)
 *   -1 on truncation or error (buf[*pos] is preserved as '\0' if the
 *      caller initialized it; pos is set to jsonsz to mark "full" so
 *      subsequent calls short-circuit)
 *
 * On truncation, snprintf's return value is the WOULD-BE length, not
 * the bytes written. We treat any (size_t)n >= remaining as overflow
 * and pin *pos to jsonsz so further appends no-op.
 */
static int append(char *buf, size_t jsonsz, size_t *pos,
		  const char *fmt, ...)
{
	va_list ap;
	int n;
	size_t remaining;

	if (*pos >= jsonsz)
		return -1;

	remaining = jsonsz - *pos;
	va_start(ap, fmt);
	n = vsnprintf(buf + *pos, remaining, fmt, ap);
	va_end(ap);

	if (n < 0)
		return -1;

	if ((size_t)n >= remaining) {
		buf[jsonsz - 1] = '\0';
		*pos = jsonsz;
		return -1;
	}

	*pos += (size_t)n;
	return 0;
}

/* JSON-escape a string into buf. Returns 0 on success, -1 on overflow.
 *
 * We only escape the characters that can appear unescaped in argv
 * strings and break JSON parsing: backslash, double-quote, and
 * control chars (0x00-0x1F). Forward slash and non-ASCII pass through.
 *
 * This is required because argv strings are user-controlled and may
 * contain quotes (e.g. retrace trace 'foo","bar' -- /bin/ls would
 * otherwise break out of the JSON string).
 */
static int append_escaped(char *buf, size_t jsonsz, size_t *pos,
			  const char *s)
{
	const char *p;
	char chbuf[8];

	for (p = s; *p != '\0'; p++) {
		const char *sub = NULL;

		switch (*p) {
		case '\\':
			sub = "\\\\";
			break;
		case '"':
			sub = "\\\"";
			break;
		case '\b':
			sub = "\\b";
			break;
		case '\f':
			sub = "\\f";
			break;
		case '\n':
			sub = "\\n";
			break;
		case '\r':
			sub = "\\r";
			break;
		case '\t':
			sub = "\\t";
			break;
		default:
			if ((unsigned char)*p < 0x20) {
				int n = snprintf(chbuf, sizeof(chbuf),
						 "\\u%04x",
						 (unsigned char)*p);

				if (n < 0 || (size_t)n >= sizeof(chbuf))
					return -1;
				if (append(buf, jsonsz, pos, "%s", chbuf) < 0)
					return -1;
			} else {
				if (append(buf, jsonsz, pos, "%c", *p) < 0)
					return -1;
			}
			continue;
		}
		if (append(buf, jsonsz, pos, "%s", sub) < 0)
			return -1;
	}
	return 0;
}

int retrace_cli_build_trace_config(char *json, size_t jsonsz,
				   const char *const *funcs, size_t nfuncs)
{
	size_t pos = 0;
	size_t i;

	if (json == NULL || jsonsz == 0)
		return -1;
	json[0] = '\0';

	if (nfuncs > 0 && funcs == NULL)
		return -1;

	if (append(json, jsonsz, &pos, "{\"intercept_scripts\":[") < 0)
		return -1;

	if (nfuncs == 0) {
		if (append(json, jsonsz, &pos,
			   "{\"func_name\":\"*\","
			   "\"actions\":[{\"action_name\":\"log_params\"},"
			   "{\"action_name\":\"call_real\"}]}") < 0)
			return -1;
	} else {
		for (i = 0; i < nfuncs; i++) {
			if (funcs[i] == NULL)
				return -1;
			if (append(json, jsonsz, &pos, "%s",
				   i > 0 ? "," : "") < 0)
				return -1;
			if (append(json, jsonsz, &pos,
				   "{\"func_name\":\"") < 0)
				return -1;
			if (append_escaped(json, jsonsz, &pos, funcs[i]) < 0)
				return -1;
			if (append(json, jsonsz, &pos,
				   "\",\"actions\":[{\"action_name\":"
				   "\"log_params\"},{\"action_name\":"
				   "\"call_real\"}]}") < 0)
				return -1;
		}
	}

	if (append(json, jsonsz, &pos, "]}") < 0)
		return -1;

	return 0;
}

int retrace_cli_build_mock_config(char *json, size_t jsonsz,
				  const char *func, long retval)
{
	size_t pos = 0;

	if (json == NULL || jsonsz == 0 || func == NULL)
		return -1;
	json[0] = '\0';

	if (append(json, jsonsz, &pos, "{\"intercept_scripts\":[") < 0)
		return -1;
	if (append(json, jsonsz, &pos, "{\"func_name\":\"") < 0)
		return -1;
	if (append_escaped(json, jsonsz, &pos, func) < 0)
		return -1;
	if (append(json, jsonsz, &pos,
		   "\",\"actions\":[{\"action_name\":\"call_real\"},"
		   "{\"action_name\":\"modify_return_value_int\","
		   "\"action_params\":{\"retval_int\":%ld}]}]}",
		   retval) < 0)
		return -1;

	return 0;
}

int retrace_cli_build_fuzz_config(char *json, size_t jsonsz,
				  const char *func, double rate)
{
	size_t pos = 0;

	if (json == NULL || jsonsz == 0 || func == NULL)
		return -1;
	json[0] = '\0';

	if (append(json, jsonsz, &pos, "{\"intercept_scripts\":[") < 0)
		return -1;
	if (append(json, jsonsz, &pos, "{\"func_name\":\"") < 0)
		return -1;
	if (append_escaped(json, jsonsz, &pos, func) < 0)
		return -1;
	if (append(json, jsonsz, &pos,
		   "\",\"actions\":[{\"action_name\":\"call_real\"},"
		   "{\"action_name\":\"memory_fuzz\","
		   "\"action_params\":{\"fail_rate\":%.4f}]}]}",
		   rate) < 0)
		return -1;

	return 0;
}

int retrace_cli_build_slow_config(char *json, size_t jsonsz,
				  const char *func, int ms)
{
	size_t pos = 0;

	if (json == NULL || jsonsz == 0 || func == NULL)
		return -1;
	json[0] = '\0';

	if (append(json, jsonsz, &pos, "{\"intercept_scripts\":[") < 0)
		return -1;
	if (append(json, jsonsz, &pos, "{\"func_name\":\"") < 0)
		return -1;
	if (append_escaped(json, jsonsz, &pos, func) < 0)
		return -1;
	if (append(json, jsonsz, &pos,
		   "\",\"actions\":[{\"action_name\":\"call_real\"},"
		   "{\"action_name\":\"delay\","
		   "\"action_params\":{\"ms\":%d}]}]}",
		   ms) < 0)
		return -1;

	return 0;
}
