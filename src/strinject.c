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

#include <limits.h>

#include "common.h"
#include "str.h"
#include "malloc.h"
#include "file.h"

#include "strinject.h"

static struct rtr_strinject_info g_strinject_infos[STRINJECT_FUNC_MAX];
static int g_strinject_init;

/*
 * string inject types
 */

static struct {
	enum RTR_STRINJECT_TYPE type;
	const char *type_str;
} g_inject_types[] = {
	{STRINJECT_TYPE_HEX, "INJECT_SINGLE_HEX"},
	{STRINJECT_TYPE_FMT_STR, "INJECT_FORMAT_STR"},
	{STRINJECT_TYPE_BUF_OVERFLOW, "INJECT_BUF_OVERFLOW"},
	{STRINJECT_TYPE_FILE_LINE, "INJECT_FILE_LINE"},
	{STRINJECT_TYPE_UNKNOWN, NULL}
};

/*
 * string inject functions
 */

static struct {
	enum RTR_STRINJECT_FUNC_ID id;
	const char *name;
} g_inject_funcs[] = {
	{STRINJECT_FUNC_FWRITE, "fwrite"},
	{STRINJECT_FUNC_FREAD, "fread"},
	{STRINJECT_FUNC_READ, "read"},
	{STRINJECT_FUNC_WRITE, "write"},
	{STRINJECT_FUNC_SEND, "send"},
	{STRINJECT_FUNC_SENDTO, "sendto"},
	{STRINJECT_FUNC_SENDMSG, "sendmsg"},
	{STRINJECT_FUNC_RECV, "recv"},
	{STRINJECT_FUNC_RECVFROM, "recvfrom"},
	{STRINJECT_FUNC_RECVMSG, "recvmsg"},
	{STRINJECT_FUNC_SSL_READ, "SSL_read"},
	{STRINJECT_FUNC_SSL_WRITE, "SSL_write"},
	{STRINJECT_FUNC_MAX, NULL}
};

/*
 * string injection init function
 */

static void rtr_strinject_init(void)
{
	RTR_CONFIG_HANDLE config = RTR_CONFIG_START;

	while (1) {
		char *inject_type_str = NULL;
		char *func_list = NULL;
		char *inject_param = NULL;
		double inject_rate;

		enum RTR_STRINJECT_TYPE inject_type = STRINJECT_TYPE_UNKNOWN;

		int i, reverse;

		/* get configuration line */
		if (rtr_get_config_multiple(&config, "stringinject", ARGUMENT_TYPE_STRING, ARGUMENT_TYPE_STRING,
			ARGUMENT_TYPE_STRING, ARGUMENT_TYPE_DOUBLE, ARGUMENT_TYPE_END,
			&inject_type_str, &func_list, &inject_param, &inject_rate) == 0)
			break;

		/* get inject type */
		for (i = 0; i < STRINJECT_TYPE_UNKNOWN; i++) {
			if (real_strcmp(inject_type_str, g_inject_types[i].type_str) == 0) {
				inject_type = g_inject_types[i].type;
				break;
			}
		}

		if (inject_type == STRINJECT_TYPE_UNKNOWN)
			continue;

		/* get function list */
		for (i = 0; i < STRINJECT_FUNC_MAX; i++) {
			if (rtr_check_config_token(g_inject_funcs[i].name, func_list, "|", &reverse) &&
				!g_strinject_infos[i].exist) {
				g_strinject_infos[i].type = inject_type;

				if (strlen(inject_param) > sizeof(g_strinject_infos[i].param) - 1)
					inject_param[sizeof(g_strinject_infos[i].param) -  1] = '\0';

				real_strcpy(g_strinject_infos[i].param, inject_param);

				g_strinject_infos[i].rate = inject_rate;
				g_strinject_infos[i].exist = 1;
			}
		}
	}

	/* set init flag */
	g_strinject_init = 1;
}

/*
 * parse injection parameter
 */

static int parse_inject_param(enum RTR_STRINJECT_TYPE type, const char *param, size_t len, void *val, off_t *offset)
{
	char param_val[512];
	char *p;

	int fmt_count, overflow_buf_len;

	/* parse param */
	p = real_strchr(param, ':');
	if (!p || p - param >= 512)
		return -1;

	real_strncpy(param_val, param, p - param);
	param_val[p - param] = '\0';

	if (real_strcmp(p + 1, "RANDOM") == 0)
		*offset = rand() % len;
	else {
		*offset = strtol(p + 1, NULL, 10);
		if (*offset == LONG_MIN || *offset == LONG_MAX)
			return -1;
	}

	/* check offset */
	if (*offset >= len)
		*offset = len - 1;

	/* inject string by types */
	switch (type) {
	case STRINJECT_TYPE_HEX:
		if (real_strcmp(param_val, "RANDOM") == 0)
			((char *) val)[0] = rand() % 0x100;
		else if (real_strcmp(param_val, "ASCII") == 0)
			((char *) val)[0] = rand() % 0x80;
		else if (real_strncmp(param_val, "0x", 2) == 0) {
			unsigned long hex_val;

			hex_val = strtol(param_val + 2, NULL, 16);
			if (hex_val > 0xff)
				return -1;

			((char *) val)[0] = (char) hex_val;
		} else
			return -1;

		break;

	case STRINJECT_TYPE_FMT_STR:
		fmt_count = strtol(param_val, NULL, 10);
		if (fmt_count <= 0)
			return -1;

		if (fmt_count > (len - *offset) / 2)
			fmt_count = (len - *offset) / 2;

		*((int *) val) = fmt_count;

		break;

	case STRINJECT_TYPE_BUF_OVERFLOW:
		overflow_buf_len = strtol(param_val, NULL, 10);
		if (overflow_buf_len <= 0)
			return -1;

		if (overflow_buf_len > (len - *offset))
			overflow_buf_len = (len - *offset);

		*((int *) val) = overflow_buf_len;

		break;

	case STRINJECT_TYPE_FILE_LINE:
		real_strncpy((char *) val, param_val, real_strlen(param_val));
		break;

	default:
		break;
	}

	return 0;
}

/*
 * inject single hex value
 */

static int inject_single_hex(const char *param, const void *buffer, size_t len,
		void **inject_buffer, size_t *inject_len)
{
	char hex_value;
	off_t offset;

	char *p;

	/* parse injection parameter */
	if (parse_inject_param(STRINJECT_TYPE_HEX, param, len, (void *) &hex_value, &offset) != 0)
		return 0;

	/* inject hex value */
	p = (char *) real_malloc(len);
	if (!p)
		return 0;

	real_memcpy(p, buffer, len);
	p[offset] = hex_value;

	*inject_buffer = p;
	*inject_len = len;

	return 1;
}

/*
 * inject format string
 */

static int inject_fmt_str(const char *param, const void *buffer, size_t len,
		void **inject_buffer, size_t *inject_len)
{
	char *p, *fmt;
	int count, i;

	off_t offset;

	/* parse injection parameter */
	if (parse_inject_param(STRINJECT_TYPE_FMT_STR, param, len, (void *) &count, &offset) != 0 &&
		count == 0)
		return 0;

	/* set inject buffer */
	p = (char *) real_malloc(len + 2 * count);
	if (!p)
		return 0;

	if (offset > 0)
		real_memcpy(p, buffer, offset);

	/* set format string */
	fmt = p + offset;
	for (i = 0; i < count; i++) {
		*(fmt++) = '%';
		*(fmt++) = 's';
	}

	if (len - offset > 0)
		real_memcpy(&p[offset + 2 * count], buffer + offset, len - offset);

	*inject_buffer = p;
	*inject_len = len + 2 * count;

	return 1;
}

/*
 * inject overflow buffer
 */

static int inject_buf_overflow(const char *param, const void *buffer, size_t len,
		void **inject_buffer, size_t *inject_len)
{
	char *p, *ov;
	int count, i;

	off_t offset;

	/* parse injection parameter */
	if (parse_inject_param(STRINJECT_TYPE_BUF_OVERFLOW, param, len, (void *) &count, &offset) != 0 &&
		count == 0)
		return 0;

	/* set inject buffer */
	p = (char *) real_malloc(len + count);
	if (!p)
		return 0;

	if (offset > 0)
		real_memcpy(p, buffer, offset);

	/* set overflow buffer */
	ov = p + offset;
	for (i = 0; i < count; i++)
		*(ov++) = 'A';

	if (len - offset > 0)
		real_memcpy(&p[offset + count], buffer + offset, len - offset);

	*inject_buffer = p;
	*inject_len = len + count;

	return 1;
}

/*
 * inject file line
 */

static int inject_file_line(const char *param, const void *buffer, size_t len,
		void **inject_buffer, size_t *inject_len)
{
	char fpath[128];

	FILE *fp;
	int line_count = 0, selected_line;

	char buf[1024];
	size_t buf_len = 0;

	char *p;

	off_t offset;

	/* parse injection parameter */
	memset(fpath, 0, sizeof(fpath));
	if (parse_inject_param(STRINJECT_TYPE_FILE_LINE, param, len, (void *) fpath, &offset) != 0)
		return 0;

	/* open file */
	fp = real_fopen(fpath, "r");
	if (!fp)
		return 0;

	/* get whole line count of file */
	while (real_fgets(buf, sizeof(buf), fp) != NULL)
		line_count++;

	/* check line count */
	if (line_count == 0) {
		real_fclose(fp);
		return 0;
	}

	real_fseek(fp, 0, SEEK_SET);

	/* get line buffer with randomely selected line number */
	selected_line = rand() % line_count;

	line_count = 0;
	while (real_fgets(buf, sizeof(buf), fp) != NULL) {
		if (line_count == selected_line) {
			buf_len = real_strlen(buf);

			/* remove end line */
			if (buf[real_strlen(buf) - 1] == '\n')
				--buf_len;

			break;
		}

		line_count++;
	}

	/* close file */
	real_fclose(fp);

	if (inject_len == 0)
		return 0;

	/* set inject buffer */
	p = (char *) real_malloc(len + buf_len);
	if (!p)
		return 0;

	if (offset > 0)
		real_memcpy(p, buffer, offset);

	real_memcpy(&p[offset], buf, buf_len);
	if (len - offset > 0)
		real_memcpy(&p[offset + buf_len], buffer + offset, len - offset);

	*inject_buffer = p;
	*inject_len = len + buf_len;

	return 1;
}

/*
 * process string injection
 */

int rtr_str_inject(enum RTR_STRINJECT_FUNC_ID func_id, const void *buffer, size_t len,
		    void **inject_buffer, size_t *inject_len)
{
	enum RTR_STRINJECT_TYPE type;
	int ret;

	if (!get_tracing_enabled() || len == 0)
		return 0;

	/* init string injection configuration */
	if (!g_strinject_init)
		rtr_strinject_init();

	/* get string inject type */
	if (!g_strinject_infos[func_id].exist)
		return 0;

	/* get fuzzing flag */
	if (!rtr_get_fuzzing_flag(g_strinject_infos[func_id].rate))
		return 0;

	type = g_strinject_infos[func_id].type;

	/* inject string to original buffer */
	switch (type) {
	case STRINJECT_TYPE_HEX:
		ret = inject_single_hex(g_strinject_infos[func_id].param, buffer, len,
			inject_buffer, inject_len);
		break;

	case STRINJECT_TYPE_FMT_STR:
		ret = inject_fmt_str(g_strinject_infos[func_id].param, buffer, len,
			inject_buffer, inject_len);
		break;

	case STRINJECT_TYPE_BUF_OVERFLOW:
		ret = inject_buf_overflow(g_strinject_infos[func_id].param, buffer, len,
			inject_buffer, inject_len);
		break;

	case STRINJECT_TYPE_FILE_LINE:
		ret = inject_file_line(g_strinject_infos[func_id].param, buffer, len,
			inject_buffer, inject_len);
		break;

	default:
		break;
	}

	return ret;
}
