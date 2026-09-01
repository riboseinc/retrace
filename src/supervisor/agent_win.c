/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

#include "agent.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "logger.h"
#include "real_impls.h"
#include "reentrance_guard.h"
#include "engine.h"
#include "thread_context.h"
#include "config_cache.h"
#include "conf.h"
#include "parson.h"
#include "protocol.h"
#include "agent_ring.h"
#include "policy_sig.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/*
 * Same doctrine as the POSIX body, Win32 primitives: producers
 * only enqueue (never block the target thread); the agent
 * thread owns the pipe, framing, heartbeats, and the final
 * drain; a dead supervisor means backoff and retry -- fail-open
 * liveness, never a crash. Losses are counted and reported
 * (retrace.agent.dropped) -- never silent. Flags are volatile +
 * Interlocked (no C11 atomics in the MSVC core context).
 *
 * P0 honesty: queue items are inline-only (768 bytes); longer
 * events are dropped-and-counted rather than heap-copied. The
 * heap fallback can follow the POSIX shape when a target needs
 * it.
 */
#define WIN_AGENT_BACKOFF_MIN_MS 500
#define WIN_AGENT_BACKOFF_MAX_MS 30000

static struct {
	int enabled;
	int inited;
	char sock_path[256];
	HANDLE pipe;
	char agent_id[64];
	/* flags are volatile + Interlocked: no C11 atomics in the
	 * MSVC core context. LONG is long and LONG64 is long long
	 * (windef.h) -- spelled natively so the types are what
	 * they are everywhere
	 */
	volatile long thread_spawned;
	volatile long stop;
	volatile long connected;
	volatile long long seq;
	uint64_t drops_reported;
	uint64_t policy_epoch;
	struct agent_ring ring;
	CRITICAL_SECTION mu;
	CONDITION_VARIABLE cv;
} w_agent;

static void w_set_reason(char *out, size_t cap, const char *msg)
{
	if (out != NULL && cap > 0)
		snprintf(out, cap, "%s", msg);
}

/* ---- framing (little-endian RTRD over the byte-mode pipe) ---- */

static int w_pipe_write(const void *buf, DWORD len)
{
	const char *p = (const char *)buf;
	size_t left = len;

	while (left > 0) {
		DWORD put = 0;

		if (!WriteFile(w_agent.pipe, p, (DWORD)left, &put,
			    NULL) || put == 0)
			return -1;
		p += put;
		left -= put;
	}
	return 0;
}

static int w_send_frame(uint16_t type, const char *payload)
{
	uint8_t out[RETRACE_RPC_HEADER_SZ + 2048];
	size_t plen = payload != NULL ? strlen(payload) : 0;

	/* the shared codec + the same cap as the POSIX agent: an
	 * over-cap frame would be DROPPED by the daemon's receiver
	 * (length > RETRACE_RPC_PAYLOAD_MAX) -- truncate instead
	 * of losing the event whole
	 */
	if (plen > 2048)
		plen = 2048;
	if (retrace_rpc_frame_encode(out, sizeof(out),
		RETRACE_RPC_VERSION, type, payload,
		(uint32_t)plen) != 0)
		return -1;
	if (w_pipe_write(out, RETRACE_RPC_HEADER_SZ + plen) != 0)
		return -1;
	return 0;
}

static int w_pipe_read(void *buf, DWORD n)
{
	char *p = (char *)buf;
	size_t left = n;

	while (left > 0) {
		DWORD got = 0;

		if (!ReadFile(w_agent.pipe, p, (DWORD)left, &got,
			    NULL) || got == 0)
			return -1;
		p += got;
		left -= got;
	}
	return 0;
}

static int w_bytes_avail(void)
{
	DWORD avail = 0;

	if (!PeekNamedPipe(w_agent.pipe, NULL, 0, NULL, &avail, NULL))
		return -1;
	return (int)avail;
}

static int w_recv_frame(char *payload, size_t cap)
{
	uint8_t hdr[RETRACE_RPC_HEADER_SZ];
	struct retrace_rpc_frame fr;

	if (w_pipe_read(hdr, sizeof(hdr)) != 0)
		return -1;
	if (retrace_rpc_frame_decode(hdr, sizeof(hdr), &fr) != 0)
		return -1;
	if (fr.version != RETRACE_RPC_VERSION || fr.length >= cap)
		return -1;
	if (fr.length > 0 &&
	    w_pipe_read(payload, fr.length) != 0)
		return -1;
	payload[fr.length] = '\0';
	return (int)fr.type;
}

/* ---- queue (producers enqueue only; thread drains) ---- */

static void w_drop_connection(void);

static int w_queue_push(const char *item, size_t len)
{
	int rc;

	EnterCriticalSection(&w_agent.mu);
	rc = agent_ring_push_copy(&w_agent.ring, item, len);
	LeaveCriticalSection(&w_agent.mu);
	if (rc == 0)
		WakeConditionVariable(&w_agent.cv);
	return rc;
}

static void w_queue_send_one(void)
{
	const char *item;

	EnterCriticalSection(&w_agent.mu);
	item = agent_ring_peek(&w_agent.ring, NULL);
	LeaveCriticalSection(&w_agent.mu);
	if (item == NULL)
		return;
	if (w_send_frame(RETRACE_RPC_MSG_EVENT, item) != 0) {
		w_drop_connection();
		return;
	}
	EnterCriticalSection(&w_agent.mu);
	agent_ring_pop(&w_agent.ring);
	LeaveCriticalSection(&w_agent.mu);
}

static void w_drop_connection(void)
{
	if (w_agent.pipe != INVALID_HANDLE_VALUE)
		CloseHandle(w_agent.pipe);
	w_agent.pipe = INVALID_HANDLE_VALUE;
	InterlockedExchange(&w_agent.connected, 0);
}

/* ---- policy (mirrors the POSIX apply) ---- */

int retrace_agent_policy_apply(const char *payload_wrapped,
	char *reason_out, size_t reason_cap)
{
	struct json_object_t *root;
	uint64_t epoch;
	int rc = retrace_policy_validate(payload_wrapped,
		reason_out, reason_cap, w_agent.policy_epoch,
		&root, &epoch);

	if (rc != 0)
		return rc > 0 ? 0 : -1;	/* HELD: idempotent ACK */
	if (retrace_config_cache_build(root) != 0) {
		json_value_free(json_object_get_wrapping_value(root));
		w_set_reason(reason_out, reason_cap,
			"cache rebuild failed");
		return -1;
	}
	retrace_conf = root;
	w_agent.policy_epoch = epoch;
	return 0;
}

static void w_send_policy_ack(int applied, const char *reason)
{
	char ack[320];

	snprintf(ack, sizeof(ack),
		"{\"agent_id\":\"%s\",\"policy_epoch\":%llu,"
		"\"applied\":%s,\"reason\":\"%s\"}",
		w_agent.agent_id,
		(unsigned long long)w_agent.policy_epoch,
		applied ? "true" : "false",
		applied ? "" :
			(reason[0] != '\0' ? reason : "refused"));
	w_send_frame(RETRACE_RPC_MSG_POLICY_ACK, ack);
}

/* ---- daemon frames ---- */

static void w_jscan_str(const char *payload, const char *key,
	char *dst, size_t cap)
{
	char tag[48];
	const char *p;

	snprintf(tag, sizeof(tag), "\"%s\":\"", key);
	p = strstr(payload, tag);
	dst[0] = '\0';
	if (p == NULL)
		return;
	p += strlen(tag);
	{
		size_t n = 0;

		while (p[n] != '\0' && p[n] != '"' && n + 1 < cap)
			n++;
		memcpy(dst, p, n);
		dst[n] = '\0';
	}
}

static void w_handle_frame(int type, const char *payload)
{
	if (type == RETRACE_RPC_MSG_WELCOME) {
		w_jscan_str(payload, "agent_id", w_agent.agent_id,
			sizeof(w_agent.agent_id));
	} else if (type == RETRACE_RPC_MSG_POLICY_SET) {
		char reason[64];
		int rc = retrace_agent_policy_apply(payload, reason,
			sizeof(reason));

		w_send_policy_ack(rc == 0, reason);
	} else if (type == RETRACE_RPC_MSG_PING) {
		w_send_frame(RETRACE_RPC_MSG_PING, "{}");
	}
}

static int w_try_connect(void)
{
	HANDLE h;
	char hello[512];
	const char *sess = NULL;
	const char *nonce = NULL;

	h = CreateFileA(w_agent.sock_path, GENERIC_READ | GENERIC_WRITE,
		0, NULL, OPEN_EXISTING, 0, NULL);
	if (h == INVALID_HANDLE_VALUE) {
		if (GetLastError() == ERROR_PIPE_BUSY)
			WaitNamedPipeA(w_agent.sock_path, 200);
		return -1;
	}
	w_agent.pipe = h;
	if (retrace_real_impls.getenv != NULL) {
		sess = retrace_real_impls.getenv("RETRACE_SESSION");
		nonce = retrace_real_impls.getenv(
			"RETRACE_SUPERVISOR_NONCE");
	}
	snprintf(hello, sizeof(hello),
		"{\"session_token\":\"%s\",\"nonce\":\"%s\","
		"\"pid\":%lu,\"ppid\":0,\"boot_id\":\"proc\","
		"\"cmdline\":\"\",\"retrace_version\":\"agent\"}",
		sess != NULL ? sess : "",
		nonce != NULL ? nonce : "",
		(unsigned long)GetCurrentProcessId());
	if (w_send_frame(RETRACE_RPC_MSG_HELLO, hello) != 0) {
		w_drop_connection();
		return -1;
	}
	{
		char payload[1024];
		int type = w_recv_frame(payload, sizeof(payload) - 1);

		if (type != RETRACE_RPC_MSG_WELCOME) {
			w_drop_connection();
			return -1;
		}
		w_handle_frame(type, payload);
	}
	InterlockedExchange(&w_agent.connected, 1);
	return 0;
}

static void w_drain_queue(void)
{
	for (;;) {
		const char *item;

		EnterCriticalSection(&w_agent.mu);
		item = agent_ring_peek(&w_agent.ring, NULL);
		LeaveCriticalSection(&w_agent.mu);
		if (item == NULL)
			break;
		w_queue_send_one();
		if (!w_agent.connected)
			break;
	}
	/* loss signaling: report the counted delta, never silent */
	{
		uint64_t now;

		EnterCriticalSection(&w_agent.mu);
		now = w_agent.ring.dropped;
		LeaveCriticalSection(&w_agent.mu);
		if (now != w_agent.drops_reported) {
			char marker[256];
			uint64_t delta = now - w_agent.drops_reported;

			snprintf(marker, sizeof(marker),
				"{\"agent_id\":\"%s\",\"seq\":0,"
				"\"ts\":%ld,"
				"\"name\":\"retrace.agent.dropped\","
				"\"attrs\":{\"count\":\"%llu\","
				"\"total\":\"%llu\"}}}",
				w_agent.agent_id, (long)time(NULL),
				(unsigned long long)delta,
				(unsigned long long)now);
			if (w_queue_push(marker, strlen(marker)) == 0)
				w_agent.drops_reported = now;
		}
	}
}

static DWORD WINAPI w_agent_thread(LPVOID arg)
{
	long backoff_ms = WIN_AGENT_BACKOFF_MIN_MS;

	(void)arg;
	Sleep(250);	/* settle past init-adjacent dispatch */
	while (!w_agent.stop) {
		if (!w_agent.connected) {
			if (w_try_connect() == 0) {
				backoff_ms = WIN_AGENT_BACKOFF_MIN_MS;
			} else {
				Sleep(backoff_ms);
				backoff_ms *= 2;
				if (backoff_ms > WIN_AGENT_BACKOFF_MAX_MS)
					backoff_ms =
						WIN_AGENT_BACKOFF_MAX_MS;
				continue;
			}
		}
		w_drain_queue();
		{
			int avail = w_bytes_avail();

			if (avail > 0) {
				char payload[1024];
				int type = w_recv_frame(payload,
					sizeof(payload) - 1);

				if (type < 0)
					w_drop_connection();
				else
					w_handle_frame(type, payload);
			} else if (avail < 0) {
				w_drop_connection();
			}
		}
		{
			char hb[256];

			snprintf(hb, sizeof(hb),
				"{\"agent_id\":\"%s\",\"seq\":%lld}",
				w_agent.agent_id,
				InterlockedAdd64(&w_agent.seq, 0));
			if (w_send_frame(RETRACE_RPC_MSG_HEARTBEAT, hb)
			    != 0)
				w_drop_connection();
		}
		Sleep(100);
	}
	/* bounded final flush + BYE */
	if (!w_agent.connected)
		(void)w_try_connect();
	if (w_agent.connected) {
		w_drain_queue();
		{
			char bye[128];

			snprintf(bye, sizeof(bye),
				"{\"agent_id\":\"%s\"}",
				w_agent.agent_id);
			w_send_frame(RETRACE_RPC_MSG_BYE, bye);
		}
	}
	w_drop_connection();
	return 0;
}

int retrace_agent_init(void)
{
	const char *sock;

	if (w_agent.inited)
		return 0;
	w_agent.inited = 1;
	agent_ring_init(&w_agent.ring);
	w_agent.pipe = INVALID_HANDLE_VALUE;
	InitializeCriticalSection(&w_agent.mu);
	InitializeConditionVariable(&w_agent.cv);
	if (retrace_real_impls.getenv == NULL ||
	    retrace_real_impls.getenv("RETRACE_SUPERVISOR") == NULL)
		return 0;
	sock = retrace_real_impls.getenv("RETRACE_SUPERVISOR_SOCK");
	if (sock == NULL || sock[0] == '\0')
		return 0;
	snprintf(w_agent.sock_path, sizeof(w_agent.sock_path), "%s",
		sock);
	w_agent.enabled = 1;
	return 0;
}

void retrace_agent_kick(void)
{
	HANDLE th;

	if (!w_agent.enabled || w_agent.stop)
		return;
	if (InterlockedCompareExchange(&w_agent.thread_spawned, 1, 0)
	    != 0)
		return;
	th = CreateThread(NULL, 0, w_agent_thread, NULL, 0, NULL);
	if (th != NULL)
		CloseHandle(th);
}

void retrace_agent_deinit(void)
{
	if (!w_agent.inited)
		return;
	InterlockedExchange(&w_agent.stop, 1);
	if (w_agent.thread_spawned) {
		/* bounded wait: a stuck pipe must not hang teardown */
		DWORD end = GetTickCount() + 3000;

		while (w_agent.connected &&
		       GetTickCount() < end)
			Sleep(20);
	}
}

int retrace_agent_emit_event(const char *name,
	const char *const *kv, size_t n_kv)
{
	char item[AGENT_RING_INLINE];
	size_t o = 0;
	size_t i;
	int n;

	if (!w_agent.enabled)
		return 0;
	n = snprintf(item + o, sizeof(item) - o,
		"{\"agent_id\":\"%s\",\"seq\":%lld,\"ts\":%ld,"
		"\"name\":\"%s\",\"attrs\":{",
		w_agent.agent_id,
		InterlockedIncrement64(&w_agent.seq),
		(long)time(NULL), name);
	if (n <= 0 || (size_t)n >= sizeof(item) - o)
		return -1;
	o += (size_t)n;
	for (i = 0; i < n_kv; i++) {
		n = snprintf(item + o, sizeof(item) - o,
			"%s\"%s\":\"%s\"", i > 0 ? "," : "", kv[i * 2],
			kv[i * 2 + 1]);
		if (n <= 0 || (size_t)n >= sizeof(item) - o)
			return -1;
		o += (size_t)n;
	}
	n = snprintf(item + o, sizeof(item) - o, "},\"source\":\"libc\"}");
	if (n <= 0 || (size_t)n >= sizeof(item) - o)
		return -1;
	o += (size_t)n;
	/* every write above is bounded by the slot, so the ring's
	 * own heap fallback covers nothing here -- the oversized
	 * event simply refuses at its guard
	 */
	if (w_queue_push(item, o) != 0)
		return -1;	/* the ring counted the refusal */
	return 0;
}
