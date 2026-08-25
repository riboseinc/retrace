/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

#include "protocol.h"

#include <string.h>

static const struct {
	uint16_t id;
	const char *name;
} g_msg_names[] = {
#define X(id, name, dir) { id, #name },
	RETRACE_RPC_MSG_TABLE(X)
#undef X
};

static void put_u16le(uint8_t *p, uint16_t v)
{
	p[0] = (uint8_t)(v & 0xff);
	p[1] = (uint8_t)(v >> 8);
}

static void put_u32le(uint8_t *p, uint32_t v)
{
	p[0] = (uint8_t)(v & 0xff);
	p[1] = (uint8_t)((v >> 8) & 0xff);
	p[2] = (uint8_t)((v >> 16) & 0xff);
	p[3] = (uint8_t)((v >> 24) & 0xff);
}

static uint16_t get_u16le(const uint8_t *p)
{
	return (uint16_t)(p[0] | (p[1] << 8));
}

static uint32_t get_u32le(const uint8_t *p)
{
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
	       ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

int retrace_rpc_frame_encode(uint8_t *out, size_t out_cap,
	uint16_t version, uint16_t type,
	const void *payload, uint32_t payload_len)
{
	if (out == NULL)
		return -1;
	if (payload_len > RETRACE_RPC_PAYLOAD_MAX)
		return -1;
	if (payload_len > 0 && payload == NULL)
		return -1;
	if (out_cap < RETRACE_RPC_HEADER_SZ + (size_t)payload_len)
		return -2;

	out[0] = RETRACE_RPC_MAGIC0;
	out[1] = RETRACE_RPC_MAGIC1;
	out[2] = RETRACE_RPC_MAGIC2;
	out[3] = RETRACE_RPC_MAGIC3;
	put_u16le(out + 4, version);
	put_u16le(out + 6, type);
	put_u32le(out + 8, payload_len);
	if (payload_len > 0)
		memcpy(out + RETRACE_RPC_HEADER_SZ, payload,
			payload_len);
	return 0;
}

int retrace_rpc_frame_decode(const uint8_t *in, size_t in_len,
	struct retrace_rpc_frame *out)
{
	if (in == NULL || out == NULL)
		return -1;
	if (in_len < RETRACE_RPC_HEADER_SZ)
		return -2;
	if (in[0] != RETRACE_RPC_MAGIC0 || in[1] != RETRACE_RPC_MAGIC1 ||
	    in[2] != RETRACE_RPC_MAGIC2 || in[3] != RETRACE_RPC_MAGIC3)
		return -3;
	out->version = get_u16le(in + 4);
	out->type = get_u16le(in + 6);
	out->length = get_u32le(in + 8);
	if (out->length > RETRACE_RPC_PAYLOAD_MAX)
		return -4;
	return 0;
}

const char *retrace_rpc_msg_name(uint16_t type)
{
	size_t i;

	for (i = 0; i < sizeof(g_msg_names) /
			     sizeof(g_msg_names[0]); i++) {
		if (g_msg_names[i].id == type)
			return g_msg_names[i].name;
	}
	return "?";
}
