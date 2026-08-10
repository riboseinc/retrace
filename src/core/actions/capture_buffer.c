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

#include "actions.h"
#include "logger.h"
#include "real_impls.h"
#include "action_utils.h"

/*
 * Note: deliberately does NOT include <string.h>.
 * On macOS with _USE_FORTIFY_LEVEL > 0 (default), that header
 * macro-substitutes memcpy/strcmp/etc., which would rewrite
 * the retrace_real_impls.* calls. Same pattern as log_ring.c,
 * sockaddr_inspect.c, and the other action files.
 */

#define CAPTURE_MAX_BYTES 4096
#define CAPTURE_HEX_BUF (CAPTURE_MAX_BYTES * 2 + 1)

static int ia_capture_buffer(struct ThreadContext *t_ctx,
			     const JSON_Object *action_params)
{
	const char *param_name;
	const char *size_param_name;
	const char *format;
	int buf_idx;
	int size_idx;
	const char *buf_ptr;
	long capture_size;
	long max_bytes;
	size_t actual;
	char hex_buf[CAPTURE_HEX_BUF];
	char str_buf[CAPTURE_MAX_BYTES + 1];
	size_t i;

	if (action_params == NULL) {
		log_err("capture_buffer: action_params required");
		return -1;
	}

	param_name = json_object_get_string(action_params, "param_name");
	if (param_name == NULL) {
		log_err("capture_buffer: param_name required");
		return -1;
	}

	buf_idx = retrace_action_find_param(t_ctx, param_name);
	if (buf_idx < 0) {
		log_dbg("capture_buffer: param '%s' not found", param_name);
		return 0;
	}

	buf_ptr = (const char *)t_ctx->params[buf_idx].val;
	if (buf_ptr == NULL) {
		log_dbg("capture_buffer: '%s' is NULL", param_name);
		return 0;
	}

	size_param_name = json_object_get_string(action_params,
		"size_param");
	if (size_param_name != NULL) {
		size_idx = retrace_action_find_param(t_ctx,
			size_param_name);
		if (size_idx >= 0)
			capture_size = (long)t_ctx->params[size_idx].val;
		else
			capture_size = CAPTURE_MAX_BYTES;
	} else {
		capture_size = CAPTURE_MAX_BYTES;
	}

	max_bytes = (long)json_object_get_number(action_params,
		"max_bytes");
	if (max_bytes <= 0 || max_bytes > CAPTURE_MAX_BYTES)
		max_bytes = CAPTURE_MAX_BYTES;

	if (capture_size > max_bytes)
		capture_size = max_bytes;
	if (capture_size <= 0)
		capture_size = max_bytes;

	format = json_object_get_string(action_params, "format");
	if (format == NULL)
		format = "hex";

	actual = (size_t)capture_size;

	if (retrace_real_impls.strcmp(format, "string") == 0) {
		size_t copy_len = actual < sizeof(str_buf) - 1
			? actual : sizeof(str_buf) - 1;

		retrace_real_impls.memcpy(str_buf, buf_ptr, copy_len);
		str_buf[copy_len] = '\0';
		for (i = 0; i < copy_len; i++) {
			if (str_buf[i] < 0x20 || str_buf[i] > 0x7e) {
				if (str_buf[i] == '\n' || str_buf[i] == '\r'
				    || str_buf[i] == '\t') {
					continue;
				}
				str_buf[i] = '.';
			}
		}
		log_info("capture_buffer: %s[%ld]=%s", param_name,
			capture_size, str_buf);
	} else {
		static const char hex[] = "0123456789abcdef";

		if (actual > CAPTURE_MAX_BYTES)
			actual = CAPTURE_MAX_BYTES;
		for (i = 0; i < actual; i++) {
			unsigned char b = (unsigned char)buf_ptr[i];

			hex_buf[i * 2] = hex[b >> 4];
			hex_buf[i * 2 + 1] = hex[b & 0xf];
		}
		hex_buf[actual * 2] = '\0';
		log_info("capture_buffer: %s[%ld]=%s", param_name,
			capture_size, hex_buf);
	}

	return 0;
}

retrace_actions_define_package(capture_buffer) = {
	{
		.name = "capture_buffer",
		.action = ia_capture_buffer
	}
};
