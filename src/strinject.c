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
#include "printf.h"

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
	{STRINJECT_TYPE_STANDARD, NULL}
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
	{STRINJECT_FUNC_READV, "readv"},
	{STRINJECT_FUNC_WRITE, "write"},
	{STRINJECT_FUNC_WRITEV, "writev"},
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
	const char *arg1, *arg2, *arg3,
	     *func_list, *inject_param, *inject_param2;
	double inject_rate;
	enum RTR_STRINJECT_TYPE type;
	enum RTR_STRINJECT_FUNC_ID fid;
	int reverse;

	RTR_CONFIG_HANDLE config = RTR_CONFIG_START;
	while (1) {
		/* get configuration line */
		if (rtr_get_config_multiple(&config, "stringinject", ARGUMENT_TYPE_STRING, ARGUMENT_TYPE_STRING,
			ARGUMENT_TYPE_STRING, ARGUMENT_TYPE_DOUBLE, ARGUMENT_TYPE_END,
			&arg1, &arg2, &arg3, &inject_rate) == 0)
			break;

		/* get inject type */
		for (type = 0; type < STRINJECT_TYPE_STANDARD; type++)
			if (real_strcmp(arg1, g_inject_types[type].type_str) == 0)
				break;

		func_list = arg2;
		inject_param = arg3;
		inject_param2 = NULL;
		if (type == STRINJECT_TYPE_STANDARD) {
			func_list = arg1;
			inject_param = arg2;
			inject_param2 = arg3;
		}

		/* get function list */
		for (fid = 0; fid < STRINJECT_FUNC_MAX; fid++) {
			if (g_strinject_infos[fid].exist)
				continue;
			if (rtr_check_config_token(g_inject_funcs[fid].name, func_list, "|", &reverse)) {
				g_strinject_infos[fid].type = type;
				g_strinject_infos[fid].param = inject_param;
				g_strinject_infos[fid].param2 = inject_param2;
				g_strinject_infos[fid].rate = inject_rate;
				g_strinject_infos[fid].exist = 1;
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
		void **inject_buffer, size_t *inject_len, off_t *off)
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
	*off = offset;

	return 1;
}

/*
 * inject format string
 */

static int inject_fmt_str(const char *param, const void *buffer, size_t len,
		void **inject_buffer, size_t *inject_len, off_t *off)
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
	*off = offset;

	return 1;
}

/*
 * inject overflow buffer
 */

static int inject_buf_overflow(const char *param, const void *buffer, size_t len,
		void **inject_buffer, size_t *inject_len, off_t *off)
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

	real_memcpy(p, buffer, offset);
	memset(p + offset, 'A', count);
	real_memcpy(p + offset + count, buffer + offset, len - offset);

	*inject_buffer = p;
	*inject_len = len + count;
	*off = offset;

	return 1;
}

/*
 * inject file line
 */

static int inject_file_line(const char *param, const void *buffer, size_t len,
		void **inject_buffer, size_t *inject_len, off_t *off)
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
	*off = offset;

	return 1;
}

static char *
get_param(const char *src)
{
	const char *s;
	char *d, *buf;

	s = src;
	while (*s != '\0' && *s != '(')
		++s;

	if (*s++ == '\0')
		return NULL;

	buf = real_malloc(real_strlen(s) + 1);

	if (buf) {
		d = buf;
		while (*s != '\0' && *s != ')')
			*d++ = *s++;
		if (*s != ')' && *(s+1) != '\0') {
			real_free(buf);
			return NULL;
		}
		*d = '\0';
	}

	return buf;
}

static char *
gen_random(size_t len, int mask)
{
	char *buf;

	buf = real_malloc(len);
	if (buf != NULL)
		while (len--)
			buf[len] = rand() & mask;

	return buf;
}

static char *
parse_immediate(const char *imm, size_t *len)
{
	static const char hex[] = "0123456789abcdef";
	const char *s, *phi, *plo;
	char *buf, *d;
	int hi, lo;

	buf = real_malloc(real_strlen(imm) + 1);

	if (buf == NULL)
		goto fail;

	for (s = imm, d = buf; *s != '\0'; ++s, ++d) {
		*d = *s;
		if (*d == '\\') {
			if (s[1] == '\0' || s[2] == '\0')
				goto fail;
			phi = real_strchr(hex, s[1] | 0x20);
			plo = real_strchr(hex, s[2] | 0x20);
			if (phi == NULL || plo == NULL)
				goto fail;
			*d = ((phi - hex) << 4) + (plo - hex);
			s += 2;
		}
	}

	*len = d - buf;
	return buf;

fail:
	real_free(buf);
	*len = 0;
	return NULL;
}

static void
copy_buffer(char *dst, const char *src, size_t srclen, size_t addlen)
{
	int i = 0;

	while (addlen--) {
		*(dst++) = src[i++];
		if (i == srclen)
			i = 0;
	}
}

static void
or_buffer(char *dst, const char *src, size_t srclen, size_t addlen)
{
	int i = 0;

	while (addlen--) {
		*(dst++) |= src[i++];
		if (i == srclen)
			i = 0;
	}
}

static void
and_buffer(char *dst, const char *src, size_t srclen, size_t addlen)
{
	int i = 0;

	while (addlen--) {
		*(dst++) &= src[i++];
		if (i == srclen)
			i = 0;
	}
}

static void
xor_buffer(char *dst, const char *src, size_t srclen, size_t addlen)
{
	int i = 0;

	while (addlen--) {
		*(dst++) ^= src[i++];
		if (i == srclen)
			i = 0;
	}
}

static int
inject(struct rtr_strinject_info *info, const void *buffer, size_t len,
		void **inject_buffer, size_t *inject_len, off_t *off)
{
	long offset, addlen, cutlen, temp[] = {-1, -1, -1};
	const char *p;
	char *pp, *srcbuf = NULL, *param = NULL;
	size_t srclen = 0;
	int i, headlen, taillen, retval = 0;
	void (*src_fn)(char *, const char *, size_t, size_t);

	/* control off[:addlen[:cutlen]] */
	/* parse up to 3 ':' separated unsigned values */
	p = info->param2;
	for (i = 0; i < 3 && *p != '\0'; i++) {
		if (*p != ':') {
			errno = 0;
			temp[i] = strtol(p, &pp, 10);

			if (errno != 0
			    || (*pp != '\0' && *pp != ':')
			    || temp[i] < 0)
				goto done;
			p = pp;
		}
		if (*p == ':')
			++p;
	}

	if (*p != '\0' || temp[0] == -1)
		goto done;

	offset = temp[0];
	addlen = temp[1];
	cutlen = temp[2];

	if (offset == -1)
		goto done;

	param = get_param(info->param);
	if (param == NULL)
		goto done;

	/* inject type */
	if (!real_strcmp(info->param, "ascii()")) {
		src_fn = copy_buffer;
		srclen = addlen;
		if (srclen == -1)
			srclen = 1;
		srcbuf = gen_random(srclen, 0x7f);
	} else if (!real_strcmp(info->param, "random()")) {
		src_fn = copy_buffer;
		srclen = addlen;
		if (srclen == -1)
			srclen = 1;
		srcbuf = gen_random(srclen, 0xff);
	} else if (!real_strncmp(info->param, "or(", 3)) {
		src_fn = or_buffer;
		srcbuf = parse_immediate(param, &srclen);
	} else if (!real_strncmp(info->param, "and(", 4)) {
		src_fn = and_buffer;
		srcbuf = parse_immediate(param, &srclen);
	} else if (!real_strncmp(info->param, "xor(", 4)) {
		src_fn = xor_buffer;
		srcbuf = parse_immediate(param, &srclen);
	} else if (!real_strncmp(info->param, "chr(", 4)) {
		src_fn = copy_buffer;
		srcbuf = parse_immediate(param, &srclen);
	} else
		goto done;

	/* sanitise offset and lengths */
	if (addlen == -1)
		addlen = srclen;

	if (srclen == 0 && addlen > 0)
		goto done;

	if (cutlen == -1)
		cutlen = addlen;

	if (addlen == 0 && cutlen == 0)
		goto done;

	taillen = len - offset - cutlen;
	if (taillen < 0)
		taillen = 0;

	headlen = offset + addlen;
	if (headlen > len)
		headlen = len;

	*inject_len = offset + addlen + taillen;
	*inject_buffer = real_malloc(*inject_len);
	if (inject_buffer == 0)
		goto done;
	real_memcpy(*inject_buffer, buffer, headlen);
	real_memcpy(*inject_buffer + offset + addlen,
	    buffer + offset + cutlen, taillen);

	src_fn(*inject_buffer + offset, srcbuf, srclen, addlen);
	*off = offset;
	retval = 1;

done:
	real_free(srcbuf);
	real_free(param);
	return retval;
}

/*
 * check if injection is enabled in config
 */

static int str_inject_enabled(enum RTR_STRINJECT_FUNC_ID func_id)
{
	if (!get_tracing_enabled())
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

	return 1;
}

/*
 * process string injection for single buffer
 */

static int str_inject(enum RTR_STRINJECT_FUNC_ID func_id, const void *buffer, size_t len,
		    void **inject_buffer, size_t *inject_len, off_t *off)
{
	off_t offset;
	int ret;

	/* check buffer length */
	if (len == 0)
		return 0;

	/* check injection status */
	if (!str_inject_enabled(func_id))
		return 0;

	/* inject string to original buffer */
	switch (g_strinject_infos[func_id].type) {
	case STRINJECT_TYPE_HEX:
		ret = inject_single_hex(g_strinject_infos[func_id].param, buffer, len,
			inject_buffer, inject_len, &offset);
		break;

	case STRINJECT_TYPE_FMT_STR:
		ret = inject_fmt_str(g_strinject_infos[func_id].param, buffer, len,
			inject_buffer, inject_len, &offset);
		break;

	case STRINJECT_TYPE_BUF_OVERFLOW:
		ret = inject_buf_overflow(g_strinject_infos[func_id].param, buffer, len,
			inject_buffer, inject_len, &offset);
		break;

	case STRINJECT_TYPE_FILE_LINE:
		ret = inject_file_line(g_strinject_infos[func_id].param, buffer, len,
			inject_buffer, inject_len, &offset);
		break;
	case STRINJECT_TYPE_STANDARD:
		ret = inject(&g_strinject_infos[func_id], buffer, len,
			inject_buffer, inject_len, &offset);
		break;
	}

	if (ret && off)
		*off = offset;

	return ret;
}

int rtr_str_inject(enum RTR_STRINJECT_FUNC_ID func_id, const void *buffer, size_t len,
		    void **inject_buffer, size_t *inject_len)
{
	return str_inject(func_id, buffer, len, inject_buffer, inject_len, NULL);
}

/*
 * merge iov multiple buffers
 */

static size_t merge_iov_buffers(const struct iovec *iov, int iovcount, void **mbuf)
{
	void *p = NULL;
	size_t msize = 0;

	int i;

	/* merging iov buffers */
	for (i = 0; i < iovcount; i++) {
		p = real_realloc(p, msize + iov[i].iov_len);
		if (!p)
			return -1;

		real_memcpy((char *) p + msize, iov[i].iov_base, iov[i].iov_len);
		msize += iov[i].iov_len;
	}

	/* set result buffer */
	*mbuf = p;

	return msize;
}

/*
 * unmerge buffer to iov buffers
 */

static int unmerge_to_iov_buffers(const struct iovec *iov, int iovcount, const void *inject_buffer,
			size_t inject_len, off_t offset, struct iovec **inject_iov, int *inject_iov_idx)
{
	size_t orig_iov_size = 0, inject_iov_start;
	int i, inject_idx = -1;

	struct iovec *v = NULL;
	size_t inject_iov_len;

	/* get original iov index from offset */
	for (i = 0; i < iovcount; i++) {
		if (inject_idx == -1 && (orig_iov_size + iov[i].iov_len) >= offset) {
			inject_iov_start = orig_iov_size;
			inject_idx = i;
		}

		orig_iov_size += iov[i].iov_len;
	}

	if (inject_idx == -1)
		return 0;

	/* get buffer len of injected iov buffer */
	inject_iov_len = iov[inject_idx].iov_len + inject_len - orig_iov_size;

	/* create new iovec structure */
	v = (struct iovec *) real_malloc(iovcount * sizeof(struct iovec));
	if (!v)
		return 0;

	/* set iovec for injection */
	for (i = 0; i < iovcount; i++) {
		if (i != inject_idx) {
			v[i].iov_base = iov[i].iov_base;
			v[i].iov_len = iov[i].iov_len;

			continue;
		}

		v[i].iov_base = real_malloc(inject_iov_len);
		if (!v[i].iov_base)
			return 0;

		real_memcpy(v[i].iov_base, inject_buffer + inject_iov_start, inject_iov_len);
		v[i].iov_len = inject_iov_len;
	}

	*inject_iov = v;
	*inject_iov_idx = inject_idx;

	return 1;
}

/*
 * process string injection for multiple buffer
 */

int rtr_str_inject_v(enum RTR_STRINJECT_FUNC_ID func_id, const struct iovec *iov, int iovcount,
		struct iovec **inject_iov, int *inject_iov_idx)
{
	int i, inject_idx;

	void *inject_buffer;
	size_t inject_len;
	off_t offset;

	void *mbuf;
	size_t msize;

	struct iovec *v;

	int ret;

	/* merge iov buffers */
	msize = merge_iov_buffers(iov, iovcount, &mbuf);
	if (msize < 0)
		return 0;

	/* perform injection for selected buffer */
	ret = str_inject(func_id, mbuf, msize, &inject_buffer, &inject_len, &offset);
	if (ret == 0)
		return 0;

	/* unmerge buffer to iov */
	ret = unmerge_to_iov_buffers(iov, iovcount, inject_buffer,
					inject_len, offset, inject_iov, inject_iov_idx);

	return ret;
}
