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
#define POLICY_MAX_BYTES 4000	/* the agent's inbound budget */

static volatile sig_atomic_t g_stop;

/* The active policy (TODO.supervisor/05): the file contents
 * verbatim (header + a full retrace config) plus its epoch.
 * Pushed to every agent on registration; POLICY_ACKs update
 * the registry's per-agent epoch.
 */
static char *g_policy_blob;
static long g_policy_epoch;

static int load_policy(const char *path)
{
	FILE *f;
	long sz;
	JSON_Value *v;
	JSON_Object *root, *pol;
	double epoch;

	f = fopen(path, "rb");
	if (f == NULL) {
		fprintf(stderr, "retraced: cannot open policy %s\n",
			path);
		return -1;
	}
	fseek(f, 0, SEEK_END);
	sz = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (sz <= 0 || sz > POLICY_MAX_BYTES) {
		fprintf(stderr,
			"retraced: policy %s bad size %ld (max %d)\n",
			path, sz, POLICY_MAX_BYTES);
		fclose(f);
		return -1;
	}
	g_policy_blob = malloc((size_t)sz + 1);
	if (g_policy_blob == NULL) {
		fclose(f);
		return -1;
	}
	if (fread(g_policy_blob, 1, (size_t)sz, f) != (size_t)sz) {
		fprintf(stderr, "retraced: policy %s read failed\n",
			path);
		free(g_policy_blob);
		g_policy_blob = NULL;
		fclose(f);
		return -1;
	}
	fclose(f);
	g_policy_blob[sz] = '\0';

	v = json_parse_string(g_policy_blob);
	root = v != NULL ? json_value_get_object(v) : NULL;
	pol = root != NULL ? json_object_get_object(root, "policy") : NULL;
	epoch = pol != NULL ? json_object_get_number(pol, "epoch") : 0.0;
	if (epoch < 1.0) {
		fprintf(stderr,
			"retraced: policy %s: policy.epoch >= 1 required\n",
			path);
		json_value_free(v);
		free(g_policy_blob);
		g_policy_blob = NULL;
		return -1;
	}
	g_policy_epoch = (long)epoch;
	json_value_free(v);
	printf("retraced: policy loaded: epoch %ld\n", g_policy_epoch);
	return 0;
}

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
	size_t plen = payload != NULL ? strlen(payload) : 0;
	uint8_t *out = malloc(RETRACE_RPC_HEADER_SZ + plen);
	int rc;

	if (out == NULL)
		return -1;
	rc = retrace_rpc_frame_encode(out, RETRACE_RPC_HEADER_SZ + plen,
		RETRACE_RPC_VERSION, type, payload, (uint32_t)plen);
	if (rc == 0)
		rc = (int)write(fd, out, RETRACE_RPC_HEADER_SZ + plen);
	free(out);
	return rc;
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

		/*
		 * Sessions (TODO.supervisor/04): stitch the tree
		 * edge, then resolve the session. A tokenless
		 * child of a session'd agent lost the token in
		 * transit (an env scrub) -- inherit the parent's
		 * session and SAY so; otherwise mint (the first
		 * agent of a detonation) or accept the inherited
		 * token verbatim.
		 */
		{
			struct agent_entry *parent =
				retraced_registry_link_parent(reg, e);
			char ev[256];

			if (e->session[0] == '\0' && parent != NULL &&
			    parent->session[0] != '\0') {
				snprintf(e->session,
					sizeof(e->session), "%s",
					parent->session);
				snprintf(ev, sizeof(ev),
					"{\"name\":\"retrace.session.scrubbed\",\"agent\":\"%s\",\"session\":\"%s\"}",
					e->id, e->session);
				retraced_journal_event(jr,
					(long)time(NULL), "daemon", 0, ev);
			} else if (e->session[0] == '\0') {
				retraced_registry_mint_session(
					e->session);
				snprintf(ev, sizeof(ev),
					"{\"name\":\"retrace.session.minted\",\"agent\":\"%s\",\"session\":\"%s\"}",
					e->id, e->session);
				retraced_journal_event(jr,
					(long)time(NULL), "daemon", 0, ev);
			}
		}

		snprintf(welcome, sizeof(welcome),
			"{\"agent_id\":\"%s\",\"session_token\":\"%s\",\"policy_epoch\":%ld,\"heartbeat_ms\":1000}",
			e->id, e->session, g_policy_epoch);
		write_frame(c->fd, RETRACE_RPC_MSG_WELCOME, welcome);
		/* the policy rides every registration: an agent
		 * that re-HELLOs after a daemon restart gets the
		 * current epoch again (and refuses it if it
		 * already holds a newer one)
		 */
		if (g_policy_blob != NULL)
			write_frame(c->fd, RETRACE_RPC_MSG_POLICY_SET,
				g_policy_blob);
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
	const char *policy_path = NULL;
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
		else if (strcmp(argv[i], "--policy") == 0 &&
			 i + 1 < argc)
			policy_path = argv[++i];
		else {
			fprintf(stderr,
				"Usage: retraced [--sock <path>] [--journal <path>]\n"
				"                 [--policy <file>]\n"
				"  --policy: a policy file (header + full\n"
				"    retrace config) pushed to every agent\n"
				"    at registration; POLICY_ACKs are\n"
				"    journaled\n");
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

	if (policy_path != NULL && load_policy(policy_path) != 0)
		return 1;

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
