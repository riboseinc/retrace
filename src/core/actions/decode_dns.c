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
 * decode_dns -- DNS protocol decoder (TODO.complete/23 expansion).
 *
 * Reads a named buffer param (typically from sendto/recvfrom on
 * port 53), parses the DNS wire format header + first question,
 * and logs the decoded fields.
 *
 * DNS wire format (RFC 1035):
 *   Header (12 bytes):
 *     ID (2), flags (2), QDCOUNT (2), ANCOUNT (2),
 *     NSCOUNT (2), ARCOUNT (2)
 *   Question:
 *     QNAME (length-prefixed labels, NUL-terminated),
 *     QTYPE (2), QCLASS (2)
 *
 * action_params:
 *   param_name  - string, required. Name of the buffer param.
 *
 * Example:
 *   {
 *     "func_name": "sendto",
 *     "actions": [
 *       { "action_name": "decode_dns",
 *         "action_params": { "param_name": "buf" } },
 *       { "action_name": "log_params" },
 *       { "action_name": "call_real" }
 *     ]
 *   }
 *
 * Non-DNS data is silently skipped (returns 0, no abort).
 *
 * Part of TODO.complete/23.
 */

#include <string.h>

#include "actions.h"
#include "logger.h"
#include "real_impls.h"

/* DNS header is 12 bytes. */
#define DNS_HEADER_LEN 12
#define DNS_MAX_NAME 256

/* Read a 16-bit big-endian value from a byte buffer. */
static unsigned int read_u16be(const unsigned char *p)
{
	return ((unsigned int)p[0] << 8) | (unsigned int)p[1];
}

/* Parse a DNS QNAME (sequence of length-prefixed labels) into
 * a dot-separated string. Returns the number of bytes consumed,
 * or 0 on error (malformed name).
 */
static size_t parse_qname(const unsigned char *buf, size_t buf_len,
			  size_t offset, char *out, size_t out_cap)
{
	size_t pos = offset;
	size_t out_pos = 0;
	int label_count = 0;

	while (pos < buf_len) {
		unsigned char label_len = buf[pos];

		if (label_len == 0) {
			pos++;
			break;
		}

		if ((label_len & 0xC0) == 0xC0) {
			pos += 2;
			break;
		}

		if (label_len > 63)
			return 0;

		pos++;
		if (pos + label_len > buf_len)
			return 0;

		if (label_count > 0 && out_pos < out_cap - 1)
			out[out_pos++] = '.';
		label_count++;

		{
			size_t i;

			for (i = 0; i < label_len && out_pos < out_cap - 1;
			     i++)
				out[out_pos++] = (char)buf[pos + i];
		}

		pos += label_len;
	}

	out[out_pos] = '\0';
	return pos - offset;
}

/* Map a DNS QTYPE to a human-readable string. */
static const char *qtype_str(unsigned int qtype)
{
	switch (qtype) {
	case 1:  return "A";
	case 2:  return "NS";
	case 5:  return "CNAME";
	case 6:  return "SOA";
	case 12: return "PTR";
	case 15: return "MX";
	case 16: return "TXT";
	case 28: return "AAAA";
	case 33: return "SRV";
	case 65: return "HTTPS";
	default: return "UNKNOWN";
	}
}

static int ia_decode_dns(struct ThreadContext *t_ctx,
			 const JSON_Object *action_params)
{
	const char *param_name;
	const unsigned char *buf;
	int param_idx;
	int i;
	unsigned int tx_id;
	unsigned int flags;
	unsigned int qdcount;
	unsigned int ancount;
	int is_response;
	char qname[DNS_MAX_NAME];
	size_t qname_consumed;
	unsigned int qtype;
	const char *qtype_name;

	if (action_params == NULL) {
		log_err("decode_dns: action_params required");
		return -1;
	}

	param_name = json_object_get_string(action_params, "param_name");
	if (param_name == NULL) {
		log_err("decode_dns: param_name required");
		return -1;
	}

	for (i = 0; i < t_ctx->params_cnt; i++) {
		if (retrace_real_impls.strcmp(
			    t_ctx->params[i].param_meta.name,
			    param_name) == 0)
			break;
	}
	if (i == t_ctx->params_cnt) {
		log_dbg("decode_dns: param '%s' not found", param_name);
		return 0;
	}

	buf = (const unsigned char *)t_ctx->params[i].val;
	if (buf == NULL)
		return 0;

	/* DNS header is 12 bytes minimum. The len param might
	 * tell us the buffer size, but for safety we just check
	 * the header fits.
	 */
	{
		long buf_len = 0;
		int j;

		for (j = 0; j < t_ctx->params_cnt; j++) {
			const char *nm =
				t_ctx->params[j].param_meta.name;

			if (retrace_real_impls.strcmp(nm, "len") == 0 ||
			    retrace_real_impls.strcmp(nm, "size") == 0) {
				buf_len = t_ctx->params[j].val;
				break;
			}
		}

		if (buf_len > 0 && buf_len < DNS_HEADER_LEN) {
			log_dbg("decode_dns: buffer too short (%ld bytes)",
				buf_len);
			return 0;
		}
	}

	tx_id = read_u16be(buf);
	flags = read_u16be(buf + 2);
	qdcount = read_u16be(buf + 4);
	ancount = read_u16be(buf + 6);
	is_response = (flags & 0x8000) ? 1 : 0;

	if (qdcount == 0 && ancount == 0) {
		log_dbg("decode_dns: no questions or answers");
		return 0;
	}

	qname[0] = '\0';
	qtype = 0;
	qtype_name = "";

	if (qdcount > 0) {
		qname_consumed = parse_qname(buf, 65536, DNS_HEADER_LEN,
			qname, sizeof(qname));
		if (qname_consumed > 0 &&
		    DNS_HEADER_LEN + qname_consumed + 4 <= 65536) {
			qtype = read_u16be(buf + DNS_HEADER_LEN +
				qname_consumed);
			qtype_name = qtype_str(qtype);
		}
	}

	if (is_response) {
		log_info("decode_dns: response id=0x%04x qname=%s qtype=%s answers=%u",
			tx_id,
			qname[0] ? qname : "(none)",
			qtype_name,
			ancount);
	} else {
		log_info("decode_dns: query id=0x%04x qname=%s qtype=%s",
			tx_id,
			qname[0] ? qname : "(none)",
			qtype_name);
	}

	return 0;
}

retrace_actions_define_package(decode_dns) = {
	{
		.name = "decode_dns",
		.action = ia_decode_dns
	}
};
