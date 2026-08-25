/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

#include "agent.h"

#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#include "logger.h"
#include "real_impls.h"
#include "reentrance_guard.h"
#include "engine.h"
#include "thread_context.h"
#include "protocol.h"

#ifndef _WIN32
#include <pthread.h>

/*
 * The in-process control agent (TODO.supervisor/03), POSIX
 * body. Windows stubs at the bottom (plan 12 brings pipes).
 *
 * Producers (any target thread, engine guard held) only
 * enqueue a fully-built EVENT payload; the agent thread owns
 * connection, framing, heartbeats, and the final drain.
 */

#define AGENT_QUEUE_CAP 256
#define AGENT_BACKOFF_MIN_MS 500
#define AGENT_BACKOFF_MAX_MS 30000
#define AGENT_FLUSH_BUDGET_MS 2000
#define AGENT_SOCK_DEFAULT "/tmp/retraced.agent.sock"

struct agent_state {
	int armed;
	char sock_path[108];
	char agent_id[96];

	pthread_mutex_t mu;
	char *queue[AGENT_QUEUE_CAP];
	size_t q_head;
	size_t q_count;
	uint64_t dropped;

	_Atomic uint64_t seq;

	_Atomic int thread_spawned;
	_Atomic int thread_done;
	_Atomic int stop;
	int fd;		/* agent thread only */
	int connected;	/* agent thread only */
	rc_thread_h tid;
};

static struct agent_state g_agent;
static void *agent_thread_main(void *arg);

/* ---------- producer side (any thread) ---------- */

static void queue_push(char *item)
{
	pthread_mutex_lock(&g_agent.mu);
	if (g_agent.q_count >= AGENT_QUEUE_CAP) {
		g_agent.dropped++;
		pthread_mutex_unlock(&g_agent.mu);
		free(item);
		return;
	}
	g_agent.queue[(g_agent.q_head + g_agent.q_count) %
		AGENT_QUEUE_CAP] = item;
	g_agent.q_count++;
	pthread_mutex_unlock(&g_agent.mu);
}

static char *queue_pop(void)
{
	char *item;

	pthread_mutex_lock(&g_agent.mu);
	if (g_agent.q_count == 0) {
		pthread_mutex_unlock(&g_agent.mu);
		return NULL;
	}
	item = g_agent.queue[g_agent.q_head];
	g_agent.q_head = (g_agent.q_head + 1) % AGENT_QUEUE_CAP;
	g_agent.q_count--;
	pthread_mutex_unlock(&g_agent.mu);
	return item;
}

/* JSON string escape (paths may contain quotes); caller frees */
static char *jesc(const char *s)
{
	size_t n = 0;
	const char *p;
	char *out;
	size_t o = 0;

	for (p = s; *p != '\0'; p++) {
		if (*p == '"' || *p == '\\')
			n += 2;
		else if ((unsigned char)*p < 0x20)
			n += 6;
		else
			n++;
	}
	out = malloc(n + 1);
	if (out == NULL)
		return NULL;
	for (p = s; *p != '\0'; p++) {
		if (*p == '"' || *p == '\\') {
			out[o++] = '\\';
			out[o++] = *p;
		} else if ((unsigned char)*p < 0x20) {
			o += (size_t)snprintf(out + o, 7, "\\u%04x",
				(unsigned int)*p);
		} else {
			out[o++] = *p;
		}
	}
	out[o] = '\0';
	return out;
}

/*
 * Build the COMPLETE EVENT payload per the protocol schema:
 * {"agent_id","seq","ts","name","attrs":{...}}. seq is taken
 * at build time (atomic) so queue order == seq order.
 */
static char *build_event_json(const char *name,
	const char *const *kv, size_t n_kv)
{
	char *n_esc = jesc(name);
	size_t cap = 96 + (n_esc != NULL ? strlen(n_esc) : 4);
	char *out;
	size_t i;
	size_t o;
	uint64_t seq;

	if (n_esc == NULL)
		return NULL;
	for (i = 0; i < n_kv; i++)
		cap += strlen(kv[2 * i]) + strlen(kv[2 * i + 1]) + 12;
	out = malloc(cap + 64);
	if (out == NULL) {
		free(n_esc);
		return NULL;
	}
	size_t total = cap + 64;

	seq = atomic_fetch_add(&g_agent.seq, 1) + 1;
	o = (size_t)snprintf(out, total,
		"{\"agent_id\":\"%s\",\"seq\":%llu,\"ts\":%ld,"
		"\"name\":\"%s\",\"attrs\":{",
		g_agent.agent_id, (unsigned long long)seq,
		time(NULL), n_esc);
	free(n_esc);
	if (o > total)
		o = total;
	for (i = 0; i < n_kv && o < total; i++) {
		char *k = jesc(kv[2 * i]);
		char *v = jesc(kv[2 * i + 1]);
		int w;

		if (k == NULL || v == NULL) {
			free(k);
			free(v);
			continue;
		}
		w = snprintf(out + o, total - o,
			"%s\"%s\":\"%s\"", i ? "," : "", k, v);
		free(k);
		free(v);
		if (w < 0)
			break;
		o += (size_t)w;
		if (o > total)
			o = total;
	}
	if (o < total)
		snprintf(out + o, total - o, "}}");
	return out;
}

int retrace_agent_emit_event(const char *name,
	const char *const *kv, size_t n_kv)
{
	char *payload;
	int expected = 0;

	if (name == NULL)
		return -1;
	if (!g_agent.armed)
		return 0;
	if (kv == NULL && n_kv > 0)
		return -1;

	payload = build_event_json(name, kv, n_kv);
	if (payload == NULL)
		return -1;
	queue_push(payload);

	/* ONE spawner (CAS): the first event boots the thread --
	 * never the constructor (the musl hazard)
	 */
	if (atomic_compare_exchange_strong(&g_agent.thread_spawned,
		&expected, 1)) {
		if (retrace_real_impls.rc_thread_create(&g_agent.tid,
			agent_thread_main, NULL) != 0) {
			atomic_store(&g_agent.thread_spawned, 0);
			return -1;
		}
	}
	return 0;
}

/* ---------- consumer side (the agent thread) ---------- */

static int send_frame(uint16_t type, const char *payload)
{
	uint8_t out[RETRACE_RPC_HEADER_SZ + 2048];
	size_t plen = payload != NULL ? strlen(payload) : 0;

	if (plen > 2048)
		plen = 2048;
	if (retrace_rpc_frame_encode(out, sizeof(out),
		RETRACE_RPC_VERSION, type, payload,
		(uint32_t)plen) != 0)
		return -1;
	if (write(g_agent.fd, out,
		RETRACE_RPC_HEADER_SZ + plen) <= 0)
		return -1;
	return 0;
}

static void drop_connection(void)
{
	if (g_agent.fd >= 0)
		close(g_agent.fd);
	g_agent.fd = -1;
	g_agent.connected = 0;
}

/* Daemon frames: WELCOME captures the minted agent_id (later
 * sends cite it); PING -> PONG; unknown types skip by length.
 */
static void handle_frame(const uint8_t *buf, size_t len)
{
	struct retrace_rpc_frame fr;

	if (retrace_rpc_frame_decode(buf, len, &fr) != 0)
		return;
	if (fr.type == RETRACE_RPC_MSG_WELCOME && len >=
	    RETRACE_RPC_HEADER_SZ + fr.length) {
		char payload[512];
		const char *tag = "\"agent_id\":\"";
		const char *s;
		size_t plen = fr.length < sizeof(payload) - 1 ?
			fr.length : sizeof(payload) - 1;

		memcpy(payload, buf + RETRACE_RPC_HEADER_SZ, plen);
		payload[plen] = '\0';
		s = strstr(payload, tag);
		if (s != NULL) {
			char *e;
			size_t n = 0;

			s += strlen(tag);
			e = strchr(s, '"');
			if (e != NULL)
				n = (size_t)(e - s) <
					sizeof(g_agent.agent_id) - 1 ?
					(size_t)(e - s) :
					sizeof(g_agent.agent_id) - 1;
			memcpy(g_agent.agent_id, s, n);
			g_agent.agent_id[n] = '\0';
		}
	} else if (fr.type == RETRACE_RPC_MSG_PING) {
		send_frame(RETRACE_RPC_MSG_PING, "{}");
	}
}

static int try_connect(void)
{
	struct sockaddr_un sa;
	int fd = socket(AF_UNIX, SOCK_STREAM, 0);

	if (fd < 0)
		return -1;
	memset(&sa, 0, sizeof(sa));
	sa.sun_family = AF_UNIX;
	strncpy(sa.sun_path, g_agent.sock_path,
		sizeof(sa.sun_path) - 1);
	if (connect(fd, (struct sockaddr *)&sa, sizeof(sa)) != 0) {
		close(fd);
		return -1;
	}
	g_agent.fd = fd;

	{
		char hello[512];

		snprintf(hello, sizeof(hello),
			"{\"session_token\":\"\",\"pid\":%ld,"
			"\"ppid\":%ld,\"boot_id\":\"proc\","
			"\"cmdline\":\"\",\"retrace_version\":\"agent\"}",
			(long)getpid(), (long)getppid());
		if (send_frame(RETRACE_RPC_MSG_HELLO, hello) != 0) {
			drop_connection();
			return -1;
		}
	}
	return 0;
}

static void drain_queue(void)
{
	char *item;

	while ((item = queue_pop()) != NULL) {
		if (send_frame(RETRACE_RPC_MSG_EVENT, item) != 0) {
			drop_connection();
			free(item);
			return;
		}
		free(item);
	}
}

static void *agent_thread_main(void *arg)
{
	struct timespec beat = {.tv_sec = 0, .tv_nsec = 100000000};
	long backoff_ms = AGENT_BACKOFF_MIN_MS;

	(void)arg;

	/* Logging off FIRST, then the permanent guard (the v2.36
	 * ordering law): the guard's TSS registration itself goes
	 * through an interposed pthread_setspecific.
	 */
	retrace_logger_disable_for_this_thread();
	{
		struct ThreadContext *ctx = retrace_thread_context_get();

		if (ctx != NULL)
			retrace_reentrance_guard_enter_permanent(ctx,
				NULL);
	}

	while (!atomic_load(&g_agent.stop)) {
		if (!g_agent.connected) {
			if (try_connect() == 0) {
				g_agent.connected = 1;
				backoff_ms = AGENT_BACKOFF_MIN_MS;
			} else {
				/* FAIL-OPEN LIVENESS: back off and
				 * retry -- never die, never unhook
				 */
				struct timespec wait = {
					.tv_sec = backoff_ms / 1000,
					.tv_nsec = (backoff_ms % 1000) *
						1000000};

				nanosleep(&wait, NULL);
				backoff_ms *= 2;
				if (backoff_ms > AGENT_BACKOFF_MAX_MS)
					backoff_ms = AGENT_BACKOFF_MAX_MS;
				continue;
			}
		}

		drain_queue();

		{
			fd_set rfds;
			struct timeval tv = {0, 200000};
			uint8_t buf[RETRACE_RPC_HEADER_SZ + 2048];

			FD_ZERO(&rfds);
			FD_SET(g_agent.fd, &rfds);
			if (select(g_agent.fd + 1, &rfds, NULL, NULL,
				&tv) > 0) {
				ssize_t n = read(g_agent.fd, buf,
					sizeof(buf));

				if (n <= 0) {
					drop_connection();
					continue;
				}
				handle_frame(buf, (size_t)n);
			}
		}

		if (g_agent.connected) {
			char hb[256];

			snprintf(hb, sizeof(hb),
				"{\"agent_id\":\"%s\",\"seq\":%llu}",
				g_agent.agent_id,
				(unsigned long long)atomic_load(
					&g_agent.seq));
			if (send_frame(RETRACE_RPC_MSG_HEARTBEAT, hb) != 0)
				drop_connection();
		}
		nanosleep(&beat, NULL);
	}

	/* Bounded final drain: short-lived targets must deliver
	 * (the daemon is alive; the queue is bounded; the socket
	 * flushes synchronously -- the budget guards a stuck peer)
	 */
	if (g_agent.connected)
		drain_queue();
	if (g_agent.connected) {
		char bye[128];

		snprintf(bye, sizeof(bye), "{\"agent_id\":\"%s\"}",
			g_agent.agent_id);
		send_frame(RETRACE_RPC_MSG_BYE, bye);
	}
	if (g_agent.fd >= 0)
		close(g_agent.fd);
	atomic_store(&g_agent.thread_done, 1);
	return NULL;
}

int retrace_agent_init(void)
{
	char *env;

	memset(&g_agent, 0, sizeof(g_agent));
	pthread_mutex_init(&g_agent.mu, NULL);

	env = retrace_real_impls.getenv("RETRACE_SUPERVISOR");
	if (env == NULL || env[0] != '1')
		return 0; /* not armed: silent, zero-delta */

	env = retrace_real_impls.getenv("RETRACE_SUPERVISOR_SOCK");
	if (env != NULL && env[0] != '\0')
		strncpy(g_agent.sock_path, env,
			sizeof(g_agent.sock_path) - 1);
	else
		strncpy(g_agent.sock_path, AGENT_SOCK_DEFAULT,
			sizeof(g_agent.sock_path) - 1);
	snprintf(g_agent.agent_id, sizeof(g_agent.agent_id),
		"pending");
	g_agent.armed = 1;
	log_info("retrace_agent: armed, supervisor %s",
		g_agent.sock_path);
	return 0;
}

void retrace_agent_deinit(void)
{
	if (!g_agent.armed)
		return;
	if (!atomic_load(&g_agent.thread_spawned))
		return;

	atomic_store(&g_agent.stop, 1);
#if defined(__APPLE__)
	{
		/* the dyld-destructor join hazard (flusher law):
		 * bounded spin on the thread's own done flag
		 */
		struct timespec poll = {.tv_sec = 0, .tv_nsec = 100000};
		int waits = 0;

		while (atomic_load(&g_agent.thread_done) != 1 &&
		       waits < AGENT_FLUSH_BUDGET_MS * 10) {
			nanosleep(&poll, NULL);
			waits++;
		}
	}
#else
	retrace_real_impls.rc_thread_join(&g_agent.tid);
#endif
	if (g_agent.dropped > 0)
		log_info("retrace_agent: dropped %llu events (full)",
			(unsigned long long)g_agent.dropped);
}

#else /* _WIN32: named pipes arrive with plan 12 */

int retrace_agent_init(void)
{
	return 0;
}

void retrace_agent_deinit(void)
{
}

int retrace_agent_emit_event(const char *name,
	const char *const *kv, size_t n_kv)
{
	(void)name;
	(void)kv;
	(void)n_kv;
	return 0;
}

#endif
