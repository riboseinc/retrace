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

/*
 * decode_http -- HTTP/1.x protocol decoder (TODO.complete/23 MVP).
 *
 * Reads a named buffer param (typically from send/recv), checks
 * whether the first line looks like HTTP/1.x, and if so logs
 * the parsed fields (method/path/status) as a JSON entry.
 *
 * If the buffer does not contain HTTP data, the action is a
 * no-op (returns 0, does not abort the script).
 *
 * action_params:
 *   param_name  - string, required. Name of the buf param.
 *
 * Example JSON (decode send() data as HTTP):
 *   {
 *     "func_name": "send",
 *     "actions": [
 *       { "action_name": "decode_http",
 *         "action_params": { "param_name": "buf" } },
 *       { "action_name": "log_params" },
 *       { "action_name": "call_real" }
 *     ]
 *   }
 *
 * For recv(), place decode_http AFTER call_real so the buffer
 * contains the received response:
 *
 *   {
 *     "func_name": "recv",
 *     "actions": [
 *       { "action_name": "call_real" },
 *       { "action_name": "decode_http",
 *         "action_params": { "param_name": "buf" } },
 *       { "action_name": "log_params" }
 *     ]
 *   }
 *
 * Part of TODO.complete/23.
 */

#include <string.h>

#include "actions.h"
#include "logger.h"
#include "real_impls.h"
#include "action_utils.h"

#define HTTP_MAX_LINE 8192

/* Known HTTP methods (RFC 7231 + RFC 5789). Checked by prefix
 * match -- the method is followed by a space in a valid request
 * line.
 */
static const char *const http_methods[] = {
	"GET ", "POST ", "PUT ", "DELETE ", "PATCH ",
	"HEAD ", "OPTIONS ", "CONNECT ", "TRACE ", NULL
};

static int starts_with_http_method(const char *s)
{
	int i;

	for (i = 0; http_methods[i] != NULL; i++) {
		size_t len = retrace_real_impls.strlen(http_methods[i]);

		if (retrace_real_impls.strncmp(s, http_methods[i], len) == 0)
			return 1;
	}
	return 0;
}

/* Find the end of the first line (delimited by \r\n or \n).
 * Returns the length of the line excluding the delimiter, or
 * 0 if no delimiter is found within max_len.
 */
static size_t find_line_end(const char *s, size_t max_len)
{
	size_t i;

	for (i = 0; i < max_len; i++) {
		if (s[i] == '\0')
			return i;
		if (s[i] == '\n')
			return i > 0 && s[i - 1] == '\r' ? i - 1 : i;
	}
	return 0;
}

/* Parse an HTTP request line: "METHOD SP TARGET SP VERSION".
 * Fills method_buf, target_buf, version_buf (each NUL-terminated).
 * Returns 0 on success, -1 if the line doesn't parse.
 */
static int parse_request_line(const char *line, size_t len,
			      char *method_buf, size_t method_cap,
			      char *target_buf, size_t target_cap,
			      char *version_buf, size_t version_cap)
{
	const char *sp1, *sp2;
	size_t method_len, target_len, version_len;

	sp1 = memchr(line, ' ', len);
	if (sp1 == NULL || sp1 == line)
		return -1;

	method_len = (size_t)(sp1 - line);
	if (method_len >= method_cap)
		return -1;
	memcpy(method_buf, line, method_len);
	method_buf[method_len] = '\0';

	sp2 = memchr(sp1 + 1, ' ', len - method_len - 1);
	if (sp2 == NULL)
		return -1;

	target_len = (size_t)(sp2 - sp1 - 1);
	if (target_len >= target_cap)
		return -1;
	memcpy(target_buf, sp1 + 1, target_len);
	target_buf[target_len] = '\0';

	version_len = len - (size_t)(sp2 + 1 - line);
	if (version_len >= version_cap)
		return -1;
	memcpy(version_buf, sp2 + 1, version_len);
	version_buf[version_len] = '\0';

	return 0;
}

/* Parse an HTTP response line: "VERSION SP STATUS SP REASON".
 * Fills version_buf, status_buf. Reason phrase is optional.
 */
static int parse_response_line(const char *line, size_t len,
			       char *version_buf, size_t version_cap,
			       int *status_code)
{
	const char *sp1, *sp2;
	size_t version_len;
	char status_buf[8];
	size_t status_len;
	char *endp;
	long code;

	sp1 = memchr(line, ' ', len);
	if (sp1 == NULL)
		return -1;

	version_len = (size_t)(sp1 - line);
	if (version_len >= version_cap)
		return -1;
	memcpy(version_buf, line, version_len);
	version_buf[version_len] = '\0';

	sp2 = memchr(sp1 + 1, ' ', len - version_len - 1);
	if (sp2 == NULL) {
		status_len = len - (size_t)(sp1 + 1 - line);
		sp2 = line + len;
	} else {
		status_len = (size_t)(sp2 - sp1 - 1);
	}

	if (status_len >= sizeof(status_buf))
		return -1;
	memcpy(status_buf, sp1 + 1, status_len);
	status_buf[status_len] = '\0';

	code = strtol(status_buf, &endp, 10);

	if (*endp != '\0' || code < 100 || code > 599)
		return -1;

	*status_code = (int)code;
	return 0;
}

static int ia_decode_http(struct ThreadContext *t_ctx,
			  const JSON_Object *action_params)
{
	const char *param_name;
	const char *buf;
	int param_idx;
	size_t line_len;
	char line_buf[HTTP_MAX_LINE];
	char method[16];
	char target[2048];
	char version[16];
	int status_code;

	if (action_params == NULL) {
		log_err("decode_http: action_params required");
		return -1;
	}

	param_name = json_object_get_string(action_params, "param_name");
	if (param_name == NULL) {
		log_err("decode_http: param_name required");
		return -1;
	}

	param_idx = retrace_action_find_param(t_ctx, param_name);
	if (param_idx < 0) {
		log_dbg("decode_http: param '%s' not found", param_name);
		return 0;
	}

	buf = (const char *)t_ctx->params[param_idx].val;
	if (buf == NULL)
		return 0;

	line_len = find_line_end(buf, HTTP_MAX_LINE - 1);
	if (line_len == 0 || line_len >= HTTP_MAX_LINE) {
		log_dbg("decode_http: no HTTP line found");
		return 0;
	}

	memcpy(line_buf, buf, line_len);
	line_buf[line_len] = '\0';

	if (starts_with_http_method(line_buf)) {
		if (parse_request_line(line_buf, line_len,
				       method, sizeof(method),
				       target, sizeof(target),
				       version, sizeof(version)) == 0) {
			log_info("decode_http: %s %s %s (request)",
				method, target, version);
		} else {
			log_info("decode_http: malformed request line");
		}
	} else if (retrace_real_impls.strncmp(line_buf, "HTTP/", 5) == 0) {
		if (parse_response_line(line_buf, line_len,
					version, sizeof(version),
					&status_code) == 0) {
			log_info("decode_http: %s %d (response)",
				version, status_code);
		} else {
			log_info("decode_http: malformed response line");
		}
	} else {
		return 0;
	}

	return 0;
}

retrace_actions_define_package(decode_http) = {
	{
		.name = "decode_http",
		.action = ia_decode_http
	}
};
