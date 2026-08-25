/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * retraced -- the supervisor daemon (TODO.supervisor/02, P0).
 *
 * One process, one thread, one poll loop: the agent socket,
 * every accepted agent connection, and SIGTERM/SIGINT. Control
 * is low-rate; a total event order falls out of the single
 * loop for free (the journal's chain assumes it).
 *
 * Crash-only state: the registry lives in RAM; the journal is
 * append-only and replays on boot (a torn tail is normal and
 * loses at most the trailing partial record).
 *
 * P0 scope (this slice): accept agents, frame parsing with the
 * receiver rules from protocol.h, HELLO/HEARTBEAT/EVENT/BYE/
 * POLICY_ACK handling, PING responder, journal + registry,
 * snapshot on exit. Controller socket, policy store, remote
 * TLS, and commands ride the later slices (05/07/08).
 */

#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#include "protocol.h"
#include "journal.h"

#include "parson.h"

#define MAX_AGENTS 128
#define HB_TIMEOUT_MS 15000
#define SWEEP_INTERVAL_MS 1000

static volatile sig_atomic_t g_stop;

static void on_signal(int sig)
{
	(void)sig;
	g_stop = 1;
}

static long now_ms(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (long)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

struct conn {
	int fd;
	uint8_t buf[RETRACE_RPC_PAYLOAD_MAX + RETRACE_RPC_HEADER_SZ];
	size_t fill;
	char agent_id[RETRACED_AGENT_ID_MAX];
	int helloed;
};

static int write_frame(int fd, uint16_t type, const char *payload)
{
	uint8_t out[RETRACE_RPC_HEADER_SZ + 1024];
	size_t plen = payload != NULL ? strlen(payload) : 0;

	if (plen > 1024)
		plen = 1024;
	if (retrace_rpc_frame_encode(out, sizeof(out),
		RETRACE_RPC_VERSION, type, payload,
		(uint32_t)plen) != 0)
		return -1;
	return (int)write(fd, out, RETRACE_RPC_HEADER_SZ + plen);
}

/* Extract a string field ("" when absent -- schema-optional). */
static void jstr(JSON_Object *o, const char *key, char *dst,
	size_t cap)
{
	const char *v = json_object_get_string(o, key);

	snprintf(dst, cap, "%s", v != NULL ? v : "");
}

static void handle_agent_frame(struct conn *c,
	struct retrace_rpc_frame *fr,
	struct retraced_registry *reg,
	struct retraced_journal *jr)
{
	JSON_Value *v;
	JSON_Object *o;
	char payload[RETRACE_RPC_PAYLOAD_MAX];
	size_t plen = fr->length < sizeof(payload) - 1 ?
		fr->length : sizeof(payload) - 1;

	memcpy(payload, c->buf + RETRACE_RPC_HEADER_SZ, plen);
	payload[plen] = '\0';

	switch (fr->type) {
	case RETRACE_RPC_MSG_HELLO: {
		char session[RETRACED_SESSION_MAX];
		char cmdline[RETRACED_CMDLINE_MAX];
		struct agent_entry *e;
		char welcome[512];
		long pid, ppid;

		v = json_parse_string(payload);
		if (v == NULL)
			return;
		o = json_value_get_object(v);
		jstr(o, "session_token", session, sizeof(session));
		jstr(o, "cmdline", cmdline, sizeof(cmdline));
		pid = (long)json_object_get_number(o, "pid");
		ppid = (long)json_object_get_number(o, "ppid");
		/* agent-supplied ids are honored when the schema
		 * grows re-HELLO (plan 04); P0 mints from pid
		 */
		e = retraced_registry_hello(reg, NULL, pid, ppid,
			session, cmdline);
		json_value_free(v);
		if (e == NULL) {
			fprintf(stderr,
				"retraced: registry full; refusing agent\n");
			return;
		}
		snprintf(c->agent_id, sizeof(c->agent_id), "%s",
			e->id);
		c->helloed = 1;
		e->last_hb_ms = now_ms();
		snprintf(welcome, sizeof(welcome),
			"{\"agent_id\":\"%s\",\"policy_epoch\":0,\"heartbeat_ms\":1000}",
			e->id);
		write_frame(c->fd, RETRACE_RPC_MSG_WELCOME, welcome);
		printf("retraced: agent %s (pid %ld) registered\n",
			e->id, pid);
		break;
	}
	case RETRACE_RPC_MSG_HEARTBEAT:
		if (!c->helloed)
			return;
		v = json_parse_string(payload);
		if (v == NULL)
			return;
		o = json_value_get_object(v);
		retraced_registry_heartbeat(reg, c->agent_id,
			(uint64_t)json_object_get_number(o, "seq"),
			now_ms());
		json_value_free(v);
		break;
	case RETRACE_RPC_MSG_EVENT:
		if (!c->helloed)
			return;
		v = json_parse_string(payload);
		if (v == NULL)
			return;
		o = json_value_get_object(v);
		retraced_journal_event(jr, (long)time(NULL),
			c->agent_id,
			(uint64_t)json_object_get_number(o, "seq"),
			payload);
		json_value_free(v);
		break;
	case RETRACE_RPC_MSG_POLICY_ACK: {
		struct agent_entry *e;

		if (!c->helloed)
			return;
		v = json_parse_string(payload);
		if (v == NULL)
			return;
		o = json_value_get_object(v);
		e = retraced_registry_find(reg, c->agent_id);
		if (e != NULL)
			e->policy_epoch = (uint64_t)
				json_object_get_number(o,
					"policy_epoch");
		json_value_free(v);
		retraced_journal_event(jr, (long)time(NULL),
			c->agent_id, 0, payload);
		break;
	}
	case RETRACE_RPC_MSG_BYE:
		retraced_registry_bye(reg, c->agent_id);
		c->helloed = 0;
		break;
	case RETRACE_RPC_MSG_PING:
		write_frame(c->fd, RETRACE_RPC_MSG_PING, "{}");
		break;
	default:
		/* unknown types skip by length (compatibility) */
		break;
	}
}

int main(int argc, char **argv)
{
	const char *sock_path = "/tmp/retraced.agent.sock";
	const char *journal_path = "retraced-journal.jsonl";
	struct retraced_registry reg;
	struct retraced_journal jr;
	struct conn *conns;
	struct sockaddr_un sa;
	struct pollfd pfds[MAX_AGENTS + 1];
	int listen_fd;
	long last_sweep;
	int i;

	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--sock") == 0 && i + 1 < argc)
			sock_path = argv[++i];
		else if (strcmp(argv[i], "--journal") == 0 &&
			 i + 1 < argc)
			journal_path = argv[++i];
		else {
			fprintf(stderr,
				"Usage: retraced [--sock <path>] [--journal <path>]\n");
			return 2;
		}
	}

	signal(SIGINT, on_signal);
	signal(SIGTERM, on_signal);
	signal(SIGPIPE, SIG_IGN);

	retraced_registry_init(&reg);
	retraced_journal_open(&jr, journal_path);
	{
		int rc = retraced_journal_replay(&jr, &reg);

		/* replay POPULATES chain_broken_at -- the check must
		 * follow it; the early return precedes any else
		 * (checkpatch) without changing the order
		 */
		if (jr.chain_broken_at >= 0) {
			fprintf(stderr,
				"retraced: journal chain broken at line %d; refusing to start (fail-closed)\n",
				jr.chain_broken_at);
			return 1;
		}
		if (rc == 0)
			printf("retraced: journal replayed: %llu events, chain ok\n",
				(unsigned long long)jr.replay_events);
		else
			printf("retraced: no prior journal (fresh boot)\n");
	}

	/* 1 MiB frame buffer per agent -- the heap, not the
	 * stack: 128 x (cap + header) overflows any main frame
	 * (found the hard way: ___chkstk_darwin in the prologue)
	 */
	conns = calloc(MAX_AGENTS, sizeof(*conns));
	if (conns == NULL) {
		perror("calloc");
		return 1;
	}
	for (i = 0; i < MAX_AGENTS; i++)
		conns[i].fd = -1;

	unlink(sock_path);
	listen_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (listen_fd < 0) {
		perror("socket");
		return 1;
	}
	memset(&sa, 0, sizeof(sa));
	sa.sun_family = AF_UNIX;
	snprintf(sa.sun_path, sizeof(sa.sun_path), "%s", sock_path);
	if (bind(listen_fd, (struct sockaddr *)&sa, sizeof(sa)) != 0) {
		perror("bind");
		return 1;
	}
	if (listen(listen_fd, 16) != 0) {
		perror("listen");
		return 1;
	}
	printf("retraced: listening on %s (P0: agent socket only)\n",
		sock_path);

	last_sweep = now_ms();
	while (!g_stop) {
		int nfd = 0;
		int r;

		pfds[nfd].fd = listen_fd;
		pfds[nfd].events = POLLIN;
		nfd++;
		for (i = 0; i < MAX_AGENTS; i++) {
			if (conns[i].fd >= 0) {
				pfds[nfd].fd = conns[i].fd;
				pfds[nfd].events = POLLIN;
				nfd++;
			}
		}
		r = poll(pfds, (nfds_t)nfd, 500);
		if (r <= 0)
			continue;

		if (pfds[0].revents & POLLIN) {
			int fd = accept(listen_fd, NULL, NULL);

			if (fd >= 0) {
				int slot = -1;

				for (i = 0; i < MAX_AGENTS; i++) {
					if (conns[i].fd < 0) {
						slot = i;
						break;
					}
				}
				if (slot >= 0) {
					memset(&conns[slot], 0,
						sizeof(conns[slot]));
					conns[slot].fd = fd;
				} else {
					close(fd);
				}
			}
		}
		for (i = 0; i < MAX_AGENTS; i++) {
			if (conns[i].fd < 0)
				continue;
			if (!(pfds[i + 1].revents & (POLLIN | POLLHUP)))
				continue;
			{
				ssize_t n = read(conns[i].fd,
					conns[i].buf + conns[i].fill,
					sizeof(conns[i].buf) -
						conns[i].fill);

				if (n <= 0) {
					close(conns[i].fd);
					conns[i].fd = -1;
					continue;
				}
				conns[i].fill += (size_t)n;
				while (conns[i].fill >=
				       RETRACE_RPC_HEADER_SZ) {
					struct retrace_rpc_frame fr;

					if (retrace_rpc_frame_decode(
						    conns[i].buf,
						    conns[i].fill,
						    &fr) != 0)
						break;
					if (conns[i].fill <
					    RETRACE_RPC_HEADER_SZ +
						    fr.length)
						break;
					handle_agent_frame(&conns[i],
						&fr, &reg, &jr);
					memmove(conns[i].buf,
						conns[i].buf +
							RETRACE_RPC_HEADER_SZ +
							fr.length,
						conns[i].fill -
							(RETRACE_RPC_HEADER_SZ +
							 fr.length));
					conns[i].fill -=
						RETRACE_RPC_HEADER_SZ +
						fr.length;
				}
			}
		}

		if (now_ms() - last_sweep > SWEEP_INTERVAL_MS) {
			size_t stale = retraced_registry_sweep(&reg,
				now_ms(), HB_TIMEOUT_MS);

			if (stale > 0)
				printf("retraced: %zu agent(s) stale\n",
					stale);
	last_sweep = now_ms();
		}
	}

	{
		JSON_Value *snap = retraced_registry_to_json(&reg);
		char *s = json_serialize_to_string_pretty(snap);

		printf("retraced: final registry:\n%s\n", s);
		json_free_serialized_string(s);
		json_value_free(snap);
	}
	retraced_journal_close(&jr);
	unlink(sock_path);
	free(conns);
	return 0;
}
