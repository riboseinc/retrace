/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */
#ifndef RETRACE_SUPERVISOR_PROTOCOL_H_
#define RETRACE_SUPERVISOR_PROTOCOL_H_

#include <stddef.h>
#include <stdint.h>

/*
 * The retraced control protocol -- wire framing + the message
 * table (TODO.supervisor/01).
 *
 * SINGLE SOURCE OF TRUTH: this header's message table is the
 * only authoring point for message ids. The conformance suite
 * (test/conformance/test_rpc_conformance.py) parses this header
 * to generate the JSON schemas and wire goldens under
 * share/rpc-schema/ -- nothing hand-copies ids, and the C frame
 * test asserts byte-equality against those same goldens. Both
 * implementations are pinned to one artifact set.
 *
 * Wire format (TODO.supervisor/01, framing v1):
 *
 *   offset  size  field
 *   0       4     magic "RTRD"
 *   4       2     protocol_version (u16 LE)
 *   6       2     message_type (u16 LE, from the table below)
 *   8       4     payload_length (u32 LE, capped 1 MiB)
 *   12      ...   payload: UTF-8 JSON (both sides use parson)
 *
 * Compatibility rules:
 *   - Unknown message_type MUST be skipped via payload_length
 *     (forward compatibility; decode succeeds, name is "?").
 *   - The table is FROZEN once shipped: new fields optional,
 *     new messages take new ids, ids are never reused.
 *   - Protocol versions negotiate at HELLO/WELCOME (plan 04's
 *     sessions wire this); framing itself is version-stable.
 */

#define RETRACE_RPC_MAGIC0 'R'
#define RETRACE_RPC_MAGIC1 'T'
#define RETRACE_RPC_MAGIC2 'R'
#define RETRACE_RPC_MAGIC3 'D'

#define RETRACE_RPC_VERSION 1

/* Hard payload cap: control is low-rate; a frame claiming more
 * is hostile or corrupt, and the receiver must reject without
 * allocating (plan 08's threat model).
 */
#define RETRACE_RPC_PAYLOAD_MAX (1u << 20)

/* The message table. X(id, NAME, DIRECTION). The conformance
 * suite greps exactly this block -- keep the format.
 */
#define RETRACE_RPC_MSG_TABLE(X) \
	X(1, HELLO, agent_to_daemon) \
	X(2, HEARTBEAT, agent_to_daemon) \
	X(3, POLICY_ACK, agent_to_daemon) \
	X(4, EVENT, agent_to_daemon) \
	X(5, RING_DATA, agent_to_daemon) \
	X(6, BYE, agent_to_daemon) \
	X(16, WELCOME, daemon_to_agent) \
	X(17, POLICY_SET, daemon_to_agent) \
	X(18, CMD, daemon_to_agent) \
	X(19, PING, daemon_to_agent)

enum retrace_rpc_msg_type {
#define X(id, name, dir) RETRACE_RPC_MSG_##name = id,
	RETRACE_RPC_MSG_TABLE(X)
#undef X
};

enum retrace_rpc_dir {
	RETRACE_RPC_DIR_AGENT_TO_DAEMON,
	RETRACE_RPC_DIR_DAEMON_TO_AGENT,
	RETRACE_RPC_DIR_CTL_TO_DAEMON, /* reserved (plan 07) */
};

struct retrace_rpc_frame {
	uint16_t version;
	uint16_t type;
	uint32_t length; /* payload bytes following the header */
};

/* Header size (12). Exposed for buffer math + tests. */
#define RETRACE_RPC_HEADER_SZ 12

/*
 * Encode one frame. Returns 0 on success; -1 on invalid args;
 * -2 when out_cap < header + payload. The header is written
 * little-endian regardless of host.
 */
int retrace_rpc_frame_encode(uint8_t *out, size_t out_cap,
	uint16_t version, uint16_t type,
	const void *payload, uint32_t payload_len);

/*
 * Decode a frame header from a complete buffer. Returns 0 and
 * fills `out` on success (unknown types decode fine -- skip via
 * length). Returns -1 on invalid args, -2 on short buffer,
 * -3 on wrong magic, -4 on length over the cap.
 */
int retrace_rpc_frame_decode(const uint8_t *in, size_t in_len,
	struct retrace_rpc_frame *out);

/*
 * Message name for logs/diagnostics ("?" for unknown ids --
 * the forward-compatibility case, never an error).
 */
const char *retrace_rpc_msg_name(uint16_t type);

#endif /* RETRACE_SUPERVISOR_PROTOCOL_H_ */
