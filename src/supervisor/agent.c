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

#ifndef _WIN32
#include <dlfcn.h>
#include <fcntl.h>
#include <poll.h>

#include <stdatomic.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
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
	uint64_t policy_epoch;

	_Atomic int thread_spawned;
	_Atomic int thread_done;
	_Atomic int stop;
	int fd;		/* agent thread only */
	int connected;	/* agent thread only */
	uint8_t rbuf[RETRACE_RPC_HEADER_SZ + 4096];	/* agent thread */
	size_t rfill;	/* agent thread only */
	rc_thread_h tid;
};

static struct agent_state g_agent;
static void *agent_thread_main(void *arg);
static void breadcrumb(const char *stage, const char *detail);
static void agent_atfork_prepare(void);
static void agent_atfork_parent(void);
static void agent_atfork_child(void);

/*
 * Resolution rules (the Linux CI SEGV lesson):
 *   - pthread_atfork IS wrapped -> the as_get_real_safe table
 *     resolves its REAL pointer (no engine dispatch, callable
 *     from the constructor).
 *   - setenv is NOT wrapped -> the table returns NULL for it;
 *     retrace_real_impls.dlsym is NULL at agent-init time on
 *     Linux (dlopen_guard owns dlsym interposition), so setenv
 *     resolves LAZILY on the agent thread, where dlsym is live.
 *     Until then the token stamp is skipped -- inheritance only
 *     works for processes that connect, which is exactly the
 *     eager/lazy spawn population anyway.
 */
static int (*g_real_setenv)(const char *, const char *, int);
static int (*g_real_atfork)(void (*)(void), void (*)(void),
	void (*)(void));

static void resolve_reals(void)
{
	if (retrace_real_impls.dlsym == NULL)
		return;
	if (g_real_setenv == NULL)
		g_real_setenv = (int (*)(const char *, const char *,
			int))retrace_real_impls.dlsym(RTLD_NEXT,
				"setenv");
	if (g_real_atfork == NULL)
		g_real_atfork = (int (*)(void (*)(void),
			void (*)(void), void (*)(void)))
			retrace_real_impls.dlsym(RTLD_NEXT,
				"pthread_atfork");
}

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

/*
 * ONE spawner (CAS): the first need boots the thread -- never
 * the constructor (the musl hazard). Returns 0 if the thread is
 * running or was started by this call.
 */
static int spawn_agent_thread(void)
{
	int expected = 0;

	if (atomic_compare_exchange_strong(&g_agent.thread_spawned,
		&expected, 1)) {
		int rc = retrace_real_impls.rc_thread_create(
			&g_agent.tid, agent_thread_main, NULL);

		if (rc != 0) {
			char detail[48];

			snprintf(detail, sizeof(detail), "%d", rc);
			breadcrumb("spawn_failed_rc", detail);
			atomic_store(&g_agent.thread_spawned, 0);
			return -1;
		}
		breadcrumb("spawn_ok", NULL);
	}
	return 0;
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

	if (spawn_agent_thread() != 0)
		return -1;
	return 0;
}

/* ---------- consumer side (the agent thread) ---------- */

/*
 * Signal-free socket write: a supervisor death mid-write must
 * surface as an ERROR RETURN, never as SIGPIPE -- the default
 * disposition would kill the target, the exact catastrophe
 * fail-open liveness exists to prevent (found on CI: the target
 * died rc=-13 at a daemon restart). Linux/BSD: MSG_NOSIGNAL;
 * macOS: SO_NOSIGPIPE on the socket (set at connect); Windows:
 * no SIGPIPE concept.
 */
static int sock_write(const void *buf, size_t len)
{
#ifdef MSG_NOSIGNAL
	return (int)retrace_real_impls.rc_send(g_agent.fd, buf, len,
		MSG_NOSIGNAL);
#else
	return (int)retrace_real_impls.rc_send(g_agent.fd, buf, len,
		0);
#endif
}

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
	if (sock_write(out, RETRACE_RPC_HEADER_SZ + plen) <= 0)
		return -1;
	return 0;
}

static void drop_connection(void)
{
	if (g_agent.fd >= 0)
		retrace_real_impls.rc_close(g_agent.fd);
	g_agent.fd = -1;
	g_agent.connected = 0;
	g_agent.rfill = 0;
}

/* Stop-aware sleep: exit waits must never ride out a long backoff */
static void nap(long ms)
{
	while (ms > 0 && !atomic_load(&g_agent.stop)) {
		struct timespec w = {.tv_sec = 0, .tv_nsec = 50000000};

		nanosleep(&w, NULL);
		ms -= 50;
	}
}

static void set_reason(char *dst, size_t cap, const char *msg)
{
	if (dst == NULL || cap == 0)
		return;
	snprintf(dst, cap, "%s", msg);
}

/*
 * Apply a supervisor policy (TODO.supervisor/05). The payload
 * is the daemon's policy file verbatim: a "policy" header plus
 * a full retrace config. Fail-closed acceptance: the header
 * must carry a strictly-greater epoch (replay protection) and
 * must not be expired; anything else keeps the ACTIVE policy.
 *
 * The swap itself gives every individual CALL a consistent
 * view: a dispatch resolves its script object once, from one
 * tree. Old trees are never freed -- in-flight dispatches and
 * the name cache hold raw pointers into them; policies are
 * small and swaps are rare, so retention is the bounded price
 * of lock-free readers on the dispatch path.
 */
int retrace_agent_policy_apply(const char *payload_json,
	char *reason_out, size_t reason_cap)
{
	JSON_Value *v;
	JSON_Object *root, *pol;
	double epoch, expires;

	if (reason_out != NULL && reason_cap > 0)
		reason_out[0] = '\0';
	if (payload_json == NULL) {
		set_reason(reason_out, reason_cap, "null payload");
		return -1;
	}

	v = json_parse_string(payload_json);
	if (v == NULL) {
		set_reason(reason_out, reason_cap, "malformed json");
		return -1;
	}
	root = json_value_get_object(v);
	pol = root != NULL ? json_object_get_object(root, "policy") : NULL;
	if (pol == NULL) {
		json_value_free(v);
		set_reason(reason_out, reason_cap, "no policy header");
		return -1;
	}

	epoch = json_object_get_number(pol, "epoch");
	expires = json_object_get_number(pol, "expires");
	if (epoch < 1.0) {
		json_value_free(v);
		set_reason(reason_out, reason_cap, "policy.epoch missing");
		return -1;
	}
	if ((uint64_t)epoch == g_agent.policy_epoch) {
		/* re-delivery of the HELD epoch (daemon restart, fork
		 * child re-registration): idempotent -- the policy
		 * is already in force, the ACK says so
		 */
		json_value_free(v);
		return 0;
	}
	if ((uint64_t)epoch < g_agent.policy_epoch) {
		json_value_free(v);
		set_reason(reason_out, reason_cap,
			"epoch regression refused");
		return -1;
	}
	if (expires > 0.0 && (time_t)expires <= time(NULL)) {
		json_value_free(v);
		set_reason(reason_out, reason_cap, "policy expired");
		return -1;
	}
	if (json_object_get_array(root, "intercept_scripts") == NULL) {
		json_value_free(v);
		set_reason(reason_out, reason_cap, "no intercept_scripts");
		return -1;
	}

	if (retrace_config_cache_build(root) != 0) {
		json_value_free(v);
		set_reason(reason_out, reason_cap, "cache rebuild failed");
		return -1;
	}
	retrace_conf = root;
	g_agent.policy_epoch = (uint64_t)epoch;
	return 0;
}

static void send_policy_ack(int applied, const char *reason)
{
	char ack[320];

	snprintf(ack, sizeof(ack),
		"{\"agent_id\":\"%s\",\"policy_epoch\":%llu,"
		"\"applied\":%s,\"reason\":\"%s\"}",
		g_agent.agent_id,
		(unsigned long long)g_agent.policy_epoch,
		applied ? "true" : "false",
		applied ? "" : (reason[0] != '\0' ? reason : "refused"));
	send_frame(RETRACE_RPC_MSG_POLICY_ACK, ack);
}

/*
 * Pull a \"key\":\"value\" string out of a small JSON payload
 * (WELCOME-grade; no nesting in that schema).
 */
static void jscan_str(const char *payload, const char *key,
	char *dst, size_t cap)
{
	char tag[48];
	const char *s;
	char *e;
	size_t n = 0;

	snprintf(tag, sizeof(tag), "\"%s\":\"", key);
	s = strstr(payload, tag);
	dst[0] = '\0';
	if (s == NULL)
		return;
	s += strlen(tag);
	e = strchr(s, '"');
	if (e == NULL)
		return;
	n = (size_t)(e - s) < cap - 1 ? (size_t)(e - s) : cap - 1;
	memcpy(dst, s, n);
	dst[n] = '\0';
}

/* Daemon frames: WELCOME captures the minted agent_id (later
 * sends cite it) and the SESSION TOKEN -- stamped into the env
 * with the REAL setenv so exec'd children inherit it (supervisor
 * plumbing, deliberately out of reach of env-jail policies);
 * POLICY_SET applies a pushed policy; PING -> PONG; unknown
 * types skip by length.
 */
static void handle_frame(const uint8_t *buf, size_t len)
{
	struct retrace_rpc_frame fr;

	if (retrace_rpc_frame_decode(buf, len, &fr) != 0)
		return;
	if (fr.type == RETRACE_RPC_MSG_WELCOME && len >=
	    RETRACE_RPC_HEADER_SZ + fr.length) {
		char payload[512];
		size_t plen = fr.length < sizeof(payload) - 1 ?
			fr.length : sizeof(payload) - 1;

		memcpy(payload, buf + RETRACE_RPC_HEADER_SZ, plen);
		payload[plen] = '\0';
		jscan_str(payload, "agent_id", g_agent.agent_id,
			sizeof(g_agent.agent_id));
		{
			char token[80];
			const char *cur = retrace_real_impls.getenv(
				"RETRACE_SESSION");

			jscan_str(payload, "session_token", token,
				sizeof(token));
			if (token[0] != '\0' && g_real_setenv != NULL &&
			    (cur == NULL || cur[0] == '\0' ||
			     strcmp(cur, token) != 0)) {
				g_real_setenv("RETRACE_SESSION", token, 1);
				log_info("agent: joined session %s",
					token);
			}
		}
	} else if (fr.type == RETRACE_RPC_MSG_POLICY_SET && len >=
		   RETRACE_RPC_HEADER_SZ + fr.length) {
		char payload[4096];
		char reason[96];
		size_t plen = fr.length < sizeof(payload) - 1 ?
			fr.length : sizeof(payload) - 1;

		memcpy(payload, buf + RETRACE_RPC_HEADER_SZ, plen);
		payload[plen] = '\0';
		reason[0] = '\0';
		if (retrace_agent_policy_apply(payload, reason,
				sizeof(reason)) == 0)
			log_info("agent: policy epoch %llu applied",
				(unsigned long long)
					g_agent.policy_epoch);
		else
			log_info("agent: policy refused: %s", reason);
		send_policy_ack(reason[0] == '\0', reason);
	} else if (fr.type == RETRACE_RPC_MSG_PING) {
		send_frame(RETRACE_RPC_MSG_PING, "{}");
	}
}

static int try_connect(void)
{
	struct sockaddr_un sa;

	breadcrumb("try_connect", g_agent.sock_path);
	int fd = retrace_real_impls.rc_socket != NULL ?
		retrace_real_impls.rc_socket(AF_UNIX, SOCK_STREAM, 0)
		: socket(AF_UNIX, SOCK_STREAM, 0);

	if (fd < 0)
		return -1;
#ifdef SO_NOSIGPIPE
	{
		int one = 1;

		(void)setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE,
			&one, sizeof(one));
	}
#endif
	memset(&sa, 0, sizeof(sa));
	sa.sun_family = AF_UNIX;
	strncpy(sa.sun_path, g_agent.sock_path,
		sizeof(sa.sun_path) - 1);
	{
		int crc = retrace_real_impls.rc_connect != NULL ?
			retrace_real_impls.rc_connect(fd,
			    (struct sockaddr *)&sa,
			    (unsigned int)sizeof(sa)) :
			connect(fd, (struct sockaddr *)&sa,
				sizeof(sa));

		if (crc != 0) {
			breadcrumb("connect_failed", NULL);
			if (retrace_real_impls.rc_close != NULL)
				retrace_real_impls.rc_close(fd);
			else
				close(fd);
			return -1;
		}
	}
	g_agent.fd = fd;

	{
		char hello[512];
		const char *sess = retrace_real_impls.getenv(
			"RETRACE_SESSION");

		snprintf(hello, sizeof(hello),
			"{\"session_token\":\"%s\",\"pid\":%ld,"
			"\"ppid\":%ld,\"boot_id\":\"proc\","
			"\"cmdline\":\"\",\"retrace_version\":\"agent\"}",
			sess != NULL ? sess : "",
			(long)getpid(), (long)getppid());
		if (send_frame(RETRACE_RPC_MSG_HELLO, hello) != 0) {
			breadcrumb("hello_send_failed", NULL);
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

/* lifecycle breadcrumbs: ride the EVENT queue to the daemon's
 * journal (the E2E dumps it) -- zero new I/O paths, and the
 * pre-connect events drain right after HELLO, so their presence
 * or absence tells exactly how far the thread got
 */
static void breadcrumb(const char *stage, const char *detail)
{
	/* FILE trace first: the journal breadcrumbs only drain
	 * after HELLO, so a never-connecting thread is invisible
	 * there. The thread's permanent guard makes the plain
	 * open/write/close dispatch-bounce to the real ones.
	 */
	{
		char path[64];
		char line[160];
		int fd;

		snprintf(path, sizeof(path), "/tmp/agent-bc-%ld.log",
			(long)getpid());
		fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0600);
		if (fd >= 0) {
			ssize_t n = snprintf(line, sizeof(line),
				"%s %s\n", stage,
				detail != NULL ? detail : "");

			(void)write(fd, line, (size_t)n);
			close(fd);
		}
	}

	{
		const char *kv[4];

		kv[0] = "retrace.agent.stage";
		kv[1] = stage;
		kv[2] = "retrace.agent.detail";
		kv[3] = detail != NULL ? detail : "";
		(void)retrace_agent_emit_event(
			"retrace.agent.lifecycle", kv, 2);
	}
}

static void *agent_thread_main(void *arg)
{
	struct timespec beat = {.tv_sec = 0, .tv_nsec = 100000000};
	long backoff_ms = AGENT_BACKOFF_MIN_MS;

	(void)arg;
	breadcrumb("thread_start", NULL);

	/*
	 * Settle past the process's init-adjacent phase (the gdb
	 * backtrace of the Linux CI SEGV: this thread's very first
	 * interposed dispatch crashed at engine entry while the
	 * main thread sat on the loader lock). poll() is NOT in the
	 * wrapped inventory -- a direct syscall, no dispatch. This
	 * mirrors how the otlp tick thread always survived (its
	 * first connect sleeps ~1s).
	 */
	poll(NULL, 0, 250);

	/*
	 * Guard FIRST, then logging off: the TSS registration uses
	 * the REAL rc_tss_* calls (no dispatch), and the guard must
	 * own every libc call this thread makes -- the previous
	 * order let logger-disable's internals dispatch free()
	 * unguarded.
	 */
	{
		struct ThreadContext *ctx = retrace_thread_context_get();

		if (ctx != NULL)
			retrace_reentrance_guard_enter_permanent(ctx,
				NULL);
	}
	retrace_logger_disable_for_this_thread();

	/* registration only (resolution happened on the main
	 * thread, inside the guard, at kick); a fork before the
	 * agent's first spawn misses it and degrades to the
	 * pre-slice-5 behavior
	 */
	if (g_real_atfork != NULL)
		g_real_atfork(agent_atfork_prepare,
			agent_atfork_parent, agent_atfork_child);

	while (!atomic_load(&g_agent.stop)) {
		if (!g_agent.connected) {
			if (try_connect() == 0) {
				g_agent.connected = 1;
				backoff_ms = AGENT_BACKOFF_MIN_MS;
			} else {
				/* FAIL-OPEN LIVENESS: back off and
				 * retry -- never die, never unhook
				 */
				nap(backoff_ms);
				backoff_ms *= 2;
				if (backoff_ms > AGENT_BACKOFF_MAX_MS)
					backoff_ms = AGENT_BACKOFF_MAX_MS;
				continue;
			}
		}

		drain_queue();

		{
			/* poll, not select: an fd at or beyond
			 * FD_SETSIZE (1024) makes FD_SET a fortified
			 * abort (__fdelt_chk -- found on glibc 2.35
			 * arm CI) and silently corrupts the fd_set
			 * everywhere else. poll has no ceiling; the
			 * daemon loop uses it for the same reason.
			 */
			struct pollfd pfd = {.fd = g_agent.fd,
				.events = POLLIN};

			if (poll(&pfd, 1, 200) > 0 &&
			    (pfd.revents & (POLLIN | POLLHUP))) {
				ssize_t n = retrace_real_impls.rc_read(
					g_agent.fd,
					g_agent.rbuf + g_agent.rfill,
					sizeof(g_agent.rbuf) -
						g_agent.rfill);

				if (n <= 0) {
					drop_connection();
					continue;
				}
				g_agent.rfill += (size_t)n;
				/* stream framing: a frame may arrive in
				 * pieces (the daemon mirrors this)
				 */
				while (g_agent.rfill >=
				       RETRACE_RPC_HEADER_SZ) {
					struct retrace_rpc_frame fr;

					if (retrace_rpc_frame_decode(
						    g_agent.rbuf,
						    g_agent.rfill,
						    &fr) != 0) {
						g_agent.rfill = 0;
						break;
					}
					if (g_agent.rfill <
					    RETRACE_RPC_HEADER_SZ +
						    fr.length)
						break;
					handle_frame(g_agent.rbuf,
						g_agent.rfill);
					memmove(g_agent.rbuf,
						g_agent.rbuf +
							RETRACE_RPC_HEADER_SZ +
							fr.length,
						g_agent.rfill -
						(RETRACE_RPC_HEADER_SZ +
							fr.length));
					g_agent.rfill -=
						RETRACE_RPC_HEADER_SZ +
						fr.length;
				}
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

	/* Bounded final flush: short-lived targets must deliver
	 * (the daemon is alive; the queue is bounded; the socket
	 * flushes synchronously -- the budget guards a stuck peer).
	 * Stop may arrive BEFORE the first loop iteration ever ran
	 * (the target exits before the thread is scheduled -- the
	 * linux-arm64 CI race), so the drain may have to open the
	 * connection itself. A UDS connect is immediate, absent or
	 * refused peers fail instantly: the attempt is inherently
	 * bounded.
	 */
	if (!g_agent.connected && try_connect() == 0)
		g_agent.connected = 1;
	if (g_agent.connected)
		drain_queue();
	if (g_agent.connected) {
		char bye[128];

		snprintf(bye, sizeof(bye), "{\"agent_id\":\"%s\"}",
			g_agent.agent_id);
		send_frame(RETRACE_RPC_MSG_BYE, bye);
	}
	if (g_agent.fd >= 0)
		retrace_real_impls.rc_close(g_agent.fd);
	atomic_store(&g_agent.thread_done, 1);
	return NULL;
}

/*
 * Fork children (TODO.supervisor/04): the child inherits HALF an
 * agent -- no agent thread (only the forking thread survives),
 * a socket shared with the parent's connection, and the parent's
 * in-flight queue. Reset to a bootable state; the next emit (a
 * jail denial is the natural beacon) respawns and re-HELLOs.
 * policy_epoch SURVIVES: the engine's active config rode in the
 * image, so a re-pushed equal epoch ACKs idempotently.
 */
static void agent_atfork_prepare(void)
{
	pthread_mutex_lock(&g_agent.mu);
}

static void agent_atfork_parent(void)
{
	pthread_mutex_unlock(&g_agent.mu);
}

static void agent_atfork_child(void)
{
	if (g_agent.fd >= 0) {
		retrace_real_impls.rc_close(g_agent.fd);
		g_agent.fd = -1;
	}
	g_agent.connected = 0;
	g_agent.rfill = 0;
	/* NO malloc/free here: this runs in a fork child of a
	 * multithreaded process, where the allocator can abort
	 * (locks held by now-missing threads). Drop the pointers
	 * and leak -- bounded, fork-children only.
	 */
	g_agent.q_count = 0;
	atomic_store(&g_agent.thread_spawned, 0);
	atomic_store(&g_agent.thread_done, 0);
	atomic_store(&g_agent.stop, 0);
	/* memcpy, not snprintf: no dispatch, no allocator -- this
	 * runs between fork() and the child's first safe breath
	 */
	memcpy(g_agent.agent_id, "pending", sizeof("pending"));
	pthread_mutex_unlock(&g_agent.mu);
}

static int g_eager_wanted;

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
	/*
	 * EAGER mode (TODO.supervisor/05): connect at boot instead
	 * of on the first event -- a policy agent must be reachable
	 * for PUSHES before any denial. The spawn is DEFERRED to
	 * retrace_agent_post_boot(): a thread spawned mid-constructor
	 * dispatches interposed calls (socket on Linux) through a
	 * half-initialized engine and crashes the boot.
	 */
	env = retrace_real_impls.getenv("RETRACE_SUPERVISOR_EAGER");
	g_eager_wanted = env != NULL && env[0] == '1';
	log_info("retrace_agent: armed, supervisor %s",
		g_agent.sock_path);
	return 0;
}

void retrace_agent_kick(void)
{
	if (!g_agent.armed || !g_eager_wanted)
		return;
	/*
	 * Resolution on the MAIN thread, post-boot, WITH THE GUARD
	 * ACTIVE (kick runs after guard-enter in the engine entry):
	 * thread-side dlsym raced the loader on Linux (glibc dlsym
	 * dispatches free() through the PLT), and unguarded dlsym
	 * cycled the dispatch path to a stack overflow. Bounded
	 * here: nested dispatches bail at the guard.
	 */
	resolve_reals();
	(void)spawn_agent_thread();
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

void retrace_agent_kick(void)
{
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

int retrace_agent_policy_apply(const char *payload_json,
	char *reason_out, size_t reason_cap)
{
	(void)payload_json;
	if (reason_out != NULL && reason_cap > 0)
		reason_out[0] = '\0';
	return -1;
}

#endif
