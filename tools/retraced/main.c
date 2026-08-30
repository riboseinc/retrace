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
#include <fcntl.h>
#include <poll.h>
#include <grp.h>
#include <pwd.h>
#include <stdarg.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "protocol.h"
#include "journal.h"
#include "peer_gate.h"
#include "retraced_ctl.h"
#include "tls_gate.h"
#include "daemon_frame.h"

#include "parson.h"

#define HB_TIMEOUT_MS 15000
#define SWEEP_INTERVAL_MS 1000
#define POLICY_MAX_BYTES 4000	/* the agent's inbound budget */

static volatile sig_atomic_t g_stop;

/* The active policy (TODO.supervisor/05): the file contents
 * verbatim (header + a full retrace config) plus its epoch.
 * Pushed to every agent on registration; POLICY_ACKs update
 * the registry's per-agent epoch.
 */
static struct retraced_ctl_ctx g_ctl;

static void *g_ctl_ssl; /* non-NULL when the active ctl is TLS */

int write_frame(int fd, uint16_t type, const char *payload);

/* the transport seam for the shared frame module: fd-backed */
static int df_write_fd(void *io, uint16_t type, const char *payload)
{
	return write_frame((int)(intptr_t)io, type, payload);
}

static void ctl_reply_fd(const char *line, void *user)
{
	int fd = *(int *)user;

	if (g_ctl_ssl != NULL)
		(void)retraced_tls_write(g_ctl_ssl, line, (int)strlen(line));
	else if (fd >= 0)
		(void)write(fd, line, strlen(line));
}


/* Agent-plane nonce (TODO.supervisor/08 P0): minted at startup
 * (or injected via --nonce for reproducible runs), delivered to
 * spawners out-of-band (--nonce-file), presented by agents in
 * HELLO. No/wrong nonce => spectator: events still flow
 * (evidence), commands and policy never do.
 */
static char g_agent_nonce[65];

static int load_policy(const char *path)
{
	FILE *f;
	long sz;
	JSON_Value *v;
	JSON_Object *root, *pol;
	double epoch;

	f = fopen(path, "rb");
	if (f == NULL) {
		fprintf(stderr, "retraced: cannot open policy path\n",
			path);
		return -1;
	}
	fseek(f, 0, SEEK_END);
	sz = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (sz <= 0 || sz > POLICY_MAX_BYTES) {
		fprintf(stderr,
			"retraced: policy path bad size %ld (max %d)\n",
			path, sz, POLICY_MAX_BYTES);
		fclose(f);
		return -1;
	}
	g_ctl.policy_blob = malloc((size_t)sz + 1);
	if (g_ctl.policy_blob == NULL) {
		fclose(f);
		return -1;
	}
	if (fread(g_ctl.policy_blob, 1, (size_t)sz, f) != (size_t)sz) {
		fprintf(stderr, "retraced: policy path read failed\n",
			path);
		free(g_ctl.policy_blob);
		g_ctl.policy_blob = NULL;
		fclose(f);
		return -1;
	}
	fclose(f);
	g_ctl.policy_blob[sz] = '\0';

	{
		char *blob = NULL;
		long epoch = 0;

		if (retraced_policy_load(g_ctl.policy_blob, &blob,
			    &epoch) != 0) {
			fprintf(stderr,
				"retraced: policy path: policy.epoch >= 1 + intercept_scripts required\n",
				path);
			free(blob);
			return -1;
		}
		free(g_ctl.policy_blob);
		g_ctl.policy_blob = blob;
		g_ctl.policy_epoch = epoch;
	}
	printf("retraced: policy loaded: epoch %ld\n", g_ctl.policy_epoch);
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

int write_frame(int fd, uint16_t type, const char *payload)
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

	struct daemon_conn_state st;
	int frame_rc;

	st.helloed = c->helloed;
	st.spectator = c->spectator;
	snprintf(st.agent_id, sizeof(st.agent_id), "%s",
		c->agent_id);

	switch (fr->type) {
	case RETRACE_RPC_MSG_HELLO: {
		char session[RETRACED_SESSION_MAX];
		char cmdline[RETRACED_CMDLINE_MAX];
		char nonce[80];
		struct agent_entry *e;
		char welcome[512];
		long pid, ppid;

		v = json_parse_string(payload);
		if (v == NULL)
			return;
		o = json_value_get_object(v);
		jstr(o, "session_token", session, sizeof(session));
		jstr(o, "cmdline", cmdline, sizeof(cmdline));
		jstr(o, "nonce", nonce, sizeof(nonce));
		pid = (long)json_object_get_number(o, "pid");
		ppid = (long)json_object_get_number(o, "ppid");
		/* P0 auth (TODO.supervisor/08): a HELLO without the
		 * channel nonce is a spectator -- evidence flows,
		 * policy and commands never reach it
		 */
		c->spectator = !retraced_nonce_matches(nonce,
			g_agent_nonce);
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
		e->spectator = c->spectator;
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
			"{\"agent_id\":\"%s\",\"session_token\":\"%s\",\"policy_epoch\":%ld,\"heartbeat_ms\":1000,\"role\":\"%s\"}",
			e->id, e->session, g_ctl.policy_epoch,
			c->spectator ? "spectator" : "full");
		write_frame(c->fd, RETRACE_RPC_MSG_WELCOME, welcome);
		/* the policy rides every registration: an agent
		 * that re-HELLOs after a daemon restart gets the
		 * current epoch again (and refuses it if it
		 * already holds a newer one). Spectators never
		 * receive policy or commands.
		 */
		if (g_ctl.policy_blob != NULL && !c->spectator)
			write_frame(c->fd, RETRACE_RPC_MSG_POLICY_SET,
				g_ctl.policy_blob);
		{
			char ev[224];

			snprintf(ev, sizeof(ev),
				"{\"name\":\"retrace.auth.agent\",\"agent\":\"%s\",\"role\":\"%s\"}",
				e->id, c->spectator ? "spectator" : "full");
			retraced_journal_event(jr,
				(long)time(NULL), "daemon", 0, ev);
		}
		printf("retraced: agent %s (pid %ld) registered%s\n",
			e->id, pid, c->spectator ? " (spectator)" : "");
		st.helloed = c->helloed;
		st.spectator = c->spectator;
		snprintf(st.agent_id, sizeof(st.agent_id), "%s",
			c->agent_id);
		break;
	}
	case RETRACE_RPC_MSG_HEARTBEAT:
		(void)v;
		(void)o;
		frame_rc = daemon_frame_handle(&st,
			RETRACE_RPC_MSG_HEARTBEAT, payload, now_ms(),
			reg, jr, &g_ctl, g_agent_nonce, df_write_fd,
			(void *)(intptr_t)c->fd);
		break;
	case RETRACE_RPC_MSG_EVENT:
		(void)v;
		(void)o;
		frame_rc = daemon_frame_handle(&st,
			RETRACE_RPC_MSG_EVENT, payload, now_ms(),
			reg, jr, &g_ctl, g_agent_nonce, df_write_fd,
			(void *)(intptr_t)c->fd);
		break;
	case RETRACE_RPC_MSG_POLICY_ACK:
		(void)v;
		(void)o;
		frame_rc = daemon_frame_handle(&st,
			RETRACE_RPC_MSG_POLICY_ACK, payload, now_ms(),
			reg, jr, &g_ctl, g_agent_nonce, df_write_fd,
			(void *)(intptr_t)c->fd);
		break;
	case RETRACE_RPC_MSG_BYE:
	case RETRACE_RPC_MSG_PING:
		(void)v;
		(void)o;
		frame_rc = daemon_frame_handle(&st, fr->type,
			payload, now_ms(), reg, jr, &g_ctl,
			g_agent_nonce, df_write_fd,
			(void *)(intptr_t)c->fd);
		c->helloed = st.helloed;
		break;
	default:
		/* unknown types skip by length (compatibility) */
		break;
	}
}

/*
 * The CONTROLLER socket (TODO.supervisor/07): newline-delimited
 * JSON, one request per line, one reply per line -- deliberately
 * not RTRD-framed: this plane is local-only in P0 and every
 * consumer is a script. Commands: status, ps, policy_push,
 * freeze, thaw, kill.
 */
static int g_ctl_listen = -1;
static int g_ctl_fd = -1;
static int g_ctl_pfd_slot = -1;
static char g_ctl_buf[8192];
static size_t g_ctl_fill;
/* TLS fleet listener (TODO.supervisor/08 P1 / beyond-libc/05) */
static int g_tls_listen = -1;
static struct retraced_tls_ctx *g_tls_ctx;
static int g_tls_pfd_slot = -1;
static void ctl_drop(void)
{
	if (g_ctl_ssl != NULL) {
		retraced_tls_ssl_free(g_ctl_ssl);
		g_ctl_ssl = NULL;
	}
	if (g_ctl_fd >= 0) {
		close(g_ctl_fd);
		g_ctl_fd = -1;
	}
	g_ctl_fill = 0;
	/* local UDS peers regain full scope on next accept */
	g_ctl.scopes = RETRACED_SCOPE_ALL;
}

static void handle_ctl_readable(struct conn *conns,
	struct retraced_registry *reg, struct retraced_journal *jr)
{
	ssize_t n;
	char *nl;

	(void)conns;
	(void)reg;
	(void)jr;
	if (g_ctl_ssl != NULL)
		n = (ssize_t)retraced_tls_read(g_ctl_ssl,
			g_ctl_buf + g_ctl_fill,
			(int)(sizeof(g_ctl_buf) - 1 - g_ctl_fill));
	else
		n = read(g_ctl_fd, g_ctl_buf + g_ctl_fill,
			sizeof(g_ctl_buf) - 1 - g_ctl_fill);
	if (n <= 0) {
		ctl_drop();
		return;
	}
	g_ctl_fill += (size_t)n;
	g_ctl_buf[g_ctl_fill] = '\0';
	while ((nl = strchr(g_ctl_buf, '\n')) != NULL) {
		*nl = '\0';
		retraced_ctl_handle_line(&g_ctl, g_ctl_buf);
		memmove(g_ctl_buf, nl + 1,
			g_ctl_fill - (size_t)(nl + 1 - g_ctl_buf));
		g_ctl_fill -= (size_t)(nl + 1 - g_ctl_buf);
	}
	if (g_ctl_fill >= sizeof(g_ctl_buf) - 1) {
		/* oversized line: drop the connection, not the daemon */
		ctl_drop();
	}
}

/*
 * Accept gate (TODO.supervisor/08 P0): refuse and journal any
 * peer that is not the daemon's euid or root. Returns the
 * accepted fd, or -1 (already closed + journaled).
 */
static int accept_gated(int listen_fd,
	struct retraced_journal *jr)
{
	int fd = accept(listen_fd, NULL, NULL);
	long puid = -1;

	if (fd < 0)
		return -1;
	if (retraced_peer_uid(fd, &puid) != 0)
		return fd;	/* no query on this platform */
	if (retraced_peer_allowed(puid, geteuid()))
		return fd;
	{
		char ev[160];

		snprintf(ev, sizeof(ev),
			"{\"name\":\"retrace.auth.refused\",\"peer_uid\":%ld}",
			puid);
		retraced_journal_event(jr, (long)time(NULL),
			"daemon", 0, ev);
	}
	close(fd);
	return -1;
}


/*
 * Live drift grading (TODO.beyond-libc/03 P1): journal the
 * kernel-observation count delta per agent -- the correlate
 * oracle's summary, live. The offline correlate tool still
 * does the full path-level grading; this is the daemon's own
 * heartbeat-grade. Called from the sweep and once more on
 * clean shutdown so a short-lived session never loses its
 * final delta.
 */
static void emit_drift_summaries(struct retraced_registry *reg,
	struct retraced_journal *jr)
{
	daemon_frame_drift_summaries(reg, jr);
}

/*
 * Loop guards (the spin-incident doctrine): any condition that
 * could make poll() return instantly forever is FATAL, journaled
 * first -- a daemon that cannot make progress must die loudly,
 * never spin silently.
 */
static struct retraced_journal *g_jr_fatal;
static volatile sig_atomic_t g_fatal_done;

static void daemon_fatal(const char *why)
{
	char ev[192];

	if (g_fatal_done)
		_exit(2);
	g_fatal_done = 1;
	snprintf(ev, sizeof(ev),
		"{\"name\":\"retrace.daemon.fatal\",\"why\":\"%s\"}",
		why);
	if (g_jr_fatal != NULL)
		retraced_journal_event(g_jr_fatal, (long)time(NULL),
			"daemon", 0, ev);
	fprintf(stderr, "retraced: fatal: %s\n", why);
	_exit(2);
}

int main(int argc, char **argv)
{
	const char *sock_path = "/tmp/retraced.agent.sock";
	const char *journal_path = "retraced-journal.jsonl";
	const char *policy_path = NULL;
	const char *ctl_path = NULL;
	const char *nonce_arg = NULL;
	const char *nonce_file = NULL;
	const char *tls_listen = NULL;
	const char *tls_cert = NULL;
	const char *tls_key = NULL;
	const char *tls_ca = NULL;
	const char *drop_user = NULL;
	const char *drop_group = NULL;
	int inherit_fd = -1;
	int unlink_sock = 1;
	int exit_after_s = 0;
	struct retraced_registry reg;
	struct retraced_journal jr;
	struct conn *conns;
	struct sockaddr_un sa;
	struct pollfd pfds[MAX_AGENTS + 5];
	/* conn slot -> pollfd index (packed: holes skipped); -1
	 * when the slot is empty. The read loop used to index
	 * pfds[slot + 1], which only holds while NO earlier slot
	 * ever disconnected -- after the first disconnect, every
	 * later agent's readiness was read from a stranger's
	 * entry: missed events, false stale sweeps.
	 */
	int pfd_of[MAX_AGENTS];
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
		else if (strcmp(argv[i], "--ctl") == 0 && i + 1 < argc)
			ctl_path = argv[++i];
		else if (strcmp(argv[i], "--nonce") == 0 && i + 1 < argc)
			nonce_arg = argv[++i];
		else if (strcmp(argv[i], "--nonce-file") == 0 &&
			 i + 1 < argc)
			nonce_file = argv[++i];
		else if (strcmp(argv[i], "--tls-listen") == 0 &&
			 i + 1 < argc)
			tls_listen = argv[++i];
		else if (strcmp(argv[i], "--tls-cert") == 0 &&
			 i + 1 < argc)
			tls_cert = argv[++i];
		else if (strcmp(argv[i], "--tls-key") == 0 &&
			 i + 1 < argc)
			tls_key = argv[++i];
		else if (strcmp(argv[i], "--tls-ca") == 0 &&
			 i + 1 < argc)
			tls_ca = argv[++i];
		else if (strcmp(argv[i], "--user") == 0 &&
			 i + 1 < argc)
			drop_user = argv[++i];
		else if (strcmp(argv[i], "--group") == 0 &&
			 i + 1 < argc)
			drop_group = argv[++i];
		else if (strcmp(argv[i], "--fd") == 0 &&
			 i + 1 < argc)
			inherit_fd = atoi(argv[++i]);
		else if (strcmp(argv[i], "--exit-after") == 0 &&
			 i + 1 < argc)
			exit_after_s = atoi(argv[++i]);
		else {
			fprintf(stderr,
				"Usage: retraced [--sock <path>] [--journal <path>]\n"
				"                 [--policy <file>] [--ctl <path>]\n"
				"                 [--nonce <hex32>] [--nonce-file <path>]\n"
				"                 [--tls-listen host:port --tls-cert c\n"
				"                  --tls-key k --tls-ca ca]\n"
				"                 [--user u] [--group g] [--fd N]\n"
			"                 [--exit-after SECONDS]\n"
				"  --policy: a policy file (header + full\n"
				"    retrace config) pushed to every agent\n"
				"    at registration; POLICY_ACKs are\n"
				"    journaled\n"
				"  --nonce: fix the agent nonce (32 hex; tests)\n"
				"  --nonce-file: write the minted nonce here\n"
				"    (0600) for spawners to inject\n"
				"  --tls-*: fleet TLS 1.3 mutual-auth ctl\n"
				"    (all four flags required together; no\n"
				"    plaintext remote mode exists)\n"
				"  --user/--group: drop privileges after every\n"
				"    socket is bound (fail-closed: a failed\n"
				"    drop exits, never continues elevated)\n"
				"  --fd N: inherit an already-bound, listening\n"
				"    agent socket on fd N (socket activation;\n"
				"    --sock names it for clients only)\n"
				"  --exit-after SECONDS: self-terminate (tests\n"
				"    and CI -- an orphaned daemon must never\n"
				"    outlive its purpose)\n");
			return 2;
		}
	}

	/*
	 * Socket activation (05 P2): an explicit --fd, else the
	 * systemd convention (LISTEN_FDS=1 -> fd 3 when LISTEN_PID
	 * is ours). An inherited listener is never bound or
	 * unlinked by this process.
	 */
	if (inherit_fd < 0) {
		const char *lf = getenv("LISTEN_FDS");
		const char *lp = getenv("LISTEN_PID");

		if (lf != NULL && strcmp(lf, "1") == 0 && lp != NULL &&
		    (long)strtol(lp, NULL, 10) == (long)getpid())
			inherit_fd = 3;		/* SD_LISTEN_FDS_START */
	}

	/* stdout may be a file or pipe (service managers): the
	 * readiness/diagnostic lines must not sit in a buffer
	 */
	setvbuf(stdout, NULL, _IOLBF, 0);
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
		if (rc == 0) {
			printf("retraced: journal replayed: %llu events, chain ok\n",
				(unsigned long long)jr.replay_events);
			if (jr.lines > 0 && !jr.clean_close) {
				/* the buffered tail was lost to an
				 * unclean shutdown: the gap gets its
				 * own record -- never silent
				 */
				retraced_journal_event(&jr,
					(long)time(NULL), "daemon", 0,
					"{\"name\":\"retrace.journal.unclean\"}");
				printf("retraced: prior shutdown was unclean; gap journaled\n");
			}
		} else {
			printf("retraced: no prior journal (fresh boot)\n");
		}
	}

	if (policy_path != NULL && load_policy(policy_path) != 0)
		return 1;

	/* P0 transport auth (TODO.supervisor/08): mint or adopt the
	 * agent nonce; publish it for spawners when asked
	 */
	if (nonce_arg != NULL) {
		size_t j;

		if (strlen(nonce_arg) != 32) {
			fprintf(stderr,
				"retraced: --nonce wants 32 hex chars\n");
			return 2;
		}
		for (j = 0; nonce_arg[j] != '\0'; j++) {
			if (strchr("0123456789abcdef", nonce_arg[j]) == NULL) {
				fprintf(stderr,
					"retraced: --nonce wants hex chars only\n");
				return 2;
			}
		}
		snprintf(g_agent_nonce, sizeof(g_agent_nonce), "%s",
			nonce_arg);
	} else {
		retraced_registry_mint_session(g_agent_nonce);
	}
	if (!retraced_peer_query_supported()) {
		char ev[128];

		snprintf(ev, sizeof(ev),
			"{\"name\":\"retrace.auth.gate_unavailable\"}");
		retraced_journal_event(&jr, (long)time(NULL),
			"daemon", 0, ev);
	}
	if (nonce_file != NULL) {
		FILE *nf = fopen(nonce_file, "w");

		if (nf == NULL) {
			perror("nonce-file");
			return 1;
		}
		fprintf(nf, "%s\n", g_agent_nonce);
		fclose(nf);
		chmod(nonce_file, 0600);
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
	g_ctl.reg = &reg;
	g_ctl.jr = &jr;
	g_ctl.conns = conns;
	g_ctl.reply_sink = ctl_reply_fd;
	g_ctl.reply_user = &g_ctl_fd;
	g_ctl.scopes = RETRACED_SCOPE_ALL; /* local UDS default */

	if (inherit_fd >= 0) {
		/* socket activation: the listener is ours to serve,
		 * not to bind or unlink. Fail-closed on a bad fd --
		 * poll would silently spin on POLLNVAL.
		 */
		if (fcntl(inherit_fd, F_GETFD) == -1) {
			fprintf(stderr,
				"retraced: --fd %d is not an open descriptor\n",
				inherit_fd);
			return 2;
		}
		listen_fd = inherit_fd;
		unlink_sock = 0;
		printf("retraced: agent socket inherited on fd %d (%s)\n",
			inherit_fd, sock_path);
	} else {
		unlink(sock_path);
		listen_fd = socket(AF_UNIX, SOCK_STREAM, 0);
		if (listen_fd < 0) {
			perror("socket");
			return 1;
		}
		memset(&sa, 0, sizeof(sa));
		sa.sun_family = AF_UNIX;
		snprintf(sa.sun_path, sizeof(sa.sun_path), "%s",
			sock_path);
		if (bind(listen_fd, (struct sockaddr *)&sa,
			    sizeof(sa)) != 0) {
			perror("bind");
			return 1;
		}
		if (listen(listen_fd, 16) != 0) {
			perror("listen");
			return 1;
		}
		chmod(sock_path, 0660);
		printf("retraced: listening on %s (agents)\n", sock_path);
	}

	if (ctl_path != NULL) {
		unlink(ctl_path);
		g_ctl_listen = socket(AF_UNIX, SOCK_STREAM, 0);
		if (g_ctl_listen >= 0) {
			memset(&sa, 0, sizeof(sa));
			sa.sun_family = AF_UNIX;
			snprintf(sa.sun_path, sizeof(sa.sun_path), "%s",
				ctl_path);
			if (bind(g_ctl_listen,
				    (struct sockaddr *)&sa, sizeof(sa)) != 0 ||
			    listen(g_ctl_listen, 4) != 0) {
				perror("ctl bind/listen");
				close(g_ctl_listen);
				g_ctl_listen = -1;
			} else {
				chmod(ctl_path, 0600);
				printf("retraced: controller on %s\n",
					ctl_path);
			}
		}
	}

	/*
	 * TLS fleet listener (TODO.supervisor/08 P1 /
	 * TODO.beyond-libc/05): ALL four flags or none. No
	 * plaintext remote mode exists -- a partial set is a
	 * hard error, never a silent degradation.
	 */
	{
		int tls_n = (tls_listen != NULL) + (tls_cert != NULL) +
			(tls_key != NULL) + (tls_ca != NULL);

		if (tls_n != 0 && tls_n != 4) {
			fprintf(stderr,
				"retraced: --tls-listen/cert/key/ca are all-or-nothing\n");
			return 2;
		}
		if (tls_n == 4) {
			if (!retraced_tls_available()) {
				fprintf(stderr,
					"retraced: TLS requested but OpenSSL was not linked\n");
				return 2;
			}
			g_tls_ctx = retraced_tls_server_new(tls_cert,
				tls_key, tls_ca);
			if (g_tls_ctx == NULL) {
				fprintf(stderr,
					"retraced: TLS context failed (cert/key/ca)\n");
				return 1;
			}
			g_tls_listen = retraced_tls_listen(tls_listen);
			if (g_tls_listen < 0) {
				fprintf(stderr,
					"retraced: TLS listen failed on %s\n",
					tls_listen);
				return 1;
			}
			printf("retraced: TLS controller on %s (TLS 1.3 mTLS)\n",
				tls_listen);
		}
	}

	/*
	 * Privilege drop (05 P2): every privileged operation is
	 * done -- sockets bound, nonce file written, journal
	 * open. A requested drop that cannot complete exits
	 * (fail-closed: the daemon never continues elevated).
	 * Without --user the daemon keeps its caller's rights,
	 * which for a systemd unit means whatever User= says.
	 */
	if (drop_user != NULL || drop_group != NULL) {
		uid_t uid = getuid();
		gid_t gid = getgid();

		if (drop_group != NULL) {
			struct group *gr = getgrnam(drop_group);

			if (gr == NULL) {
				fprintf(stderr,
					"retraced: unknown group %s\n",
					drop_group);
				return 2;
			}
			gid = gr->gr_gid;
		}
		if (drop_user != NULL) {
			struct passwd *pw = getpwnam(drop_user);

			if (pw == NULL) {
				fprintf(stderr,
					"retraced: unknown user %s\n",
					drop_user);
				return 2;
			}
			if (drop_group == NULL)
				gid = pw->pw_gid;
			uid = pw->pw_uid;
		}
		if (setgid(gid) != 0 || setuid(uid) != 0 ||
		    getuid() != uid || getgid() != gid) {
			fprintf(stderr,
				"retraced: privilege drop failed: %s\n",
				strerror(errno));
			return 2;
		}
		printf("retraced: running as uid=%ld gid=%ld\n",
			(long)uid, (long)gid);
	}

	g_jr_fatal = &jr;
	/* the harness guard: an orphaned test/CI daemon self-ends */
	if (exit_after_s > 0)
		alarm(exit_after_s);
	last_sweep = now_ms();
	while (!g_stop) {
		int nfd = 0;
		int r;
		static int spin_hits;

		pfds[nfd].fd = listen_fd;
		pfds[nfd].events = POLLIN;
		nfd++;
		{
			int ctl_idx = -1;
			int tls_idx = -1;

			if (g_ctl_listen >= 0) {
				pfds[nfd].fd = g_ctl_listen;
				pfds[nfd].events = POLLIN;
				ctl_idx = nfd;
				nfd++;
			}
			if (g_tls_listen >= 0) {
				pfds[nfd].fd = g_tls_listen;
				pfds[nfd].events = POLLIN;
				tls_idx = nfd;
				nfd++;
			}
			if (g_ctl_fd >= 0) {
				pfds[nfd].fd = g_ctl_fd;
				pfds[nfd].events = POLLIN;
				nfd++;
			}
			g_ctl_pfd_slot = ctl_idx;
			g_tls_pfd_slot = tls_idx;
		}
		for (i = 0; i < MAX_AGENTS; i++) {
			pfd_of[i] = -1;
			if (conns[i].fd >= 0) {
				pfds[nfd].fd = conns[i].fd;
				pfds[nfd].events = POLLIN;
				pfd_of[i] = nfd;
				nfd++;
			}
		}
		r = poll(pfds, (nfds_t)nfd, 500);
		if (r <= 0) {
			spin_hits = 0;	/* timed out: healthy */
			continue;
		}

		/*
		 * Poll-set hygiene (the spin-incident guards): a
		 * POLLNVAL is a descriptor error poll reports
		 * INSTANTLY and forever -- left in the set it is a
		 * 100%-CPU spin. Listeners: fatal (configuration
		 * error; the startup --fd check should have caught
		 * it, this is the backstop). Connections: drop.
		 * POLLERR|POLLHUP without POLLIN route through the
		 * read path, whose EOF handling drops them.
		 */
		{
			int acted = 0;

			for (i = 0; i < nfd; i++) {
				if (pfds[i].revents & POLLNVAL) {
					if (pfds[i].fd == listen_fd ||
					    pfds[i].fd == g_ctl_listen ||
					    pfds[i].fd == g_tls_listen)
						daemon_fatal(
							"listener POLLNVAL (bad descriptor)");
					/* connection: find + drop */
					{
						int k;

						for (k = 0; k < MAX_AGENTS;
						     k++) {
							if (conns[k].fd ==
							    pfds[i].fd) {
								close(conns[k].fd);
								conns[k].fd = -1;
								conns[k].helloed = 0;
								break;
							}
						}
						if (pfds[i].fd ==
						    g_ctl_fd)
							ctl_drop();
					}
					acted = 1;
				}
				if (pfds[i].revents & (POLLIN | POLLHUP |
						       POLLERR))
					acted = 1;
			}
			if (acted)
				spin_hits = 0;
			else if (++spin_hits > 10000) {
				/*
				 * Backstop: poll reported activity but
				 * nothing consumed it, ten thousand
				 * times in a row -- every iteration
				 * returned instantly. Die loudly.
				 */
				daemon_fatal("poll spin backstop");
			}
		}

		if (g_ctl_pfd_slot >= 0 &&
		    (pfds[g_ctl_pfd_slot].revents & POLLIN) &&
		    g_ctl_fd < 0) {
			g_ctl_fd = accept_gated(g_ctl_listen, &jr);
			g_ctl_fill = 0;
			g_ctl_ssl = NULL;
			g_ctl.scopes = RETRACED_SCOPE_ALL;
		}
		/* TLS fleet accept (08 P1): mutual auth + claim scopes */
		if (g_tls_pfd_slot >= 0 &&
		    (pfds[g_tls_pfd_slot].revents & POLLIN) &&
		    g_ctl_fd < 0) {
			int tfd = accept(g_tls_listen, NULL, NULL);
			struct retraced_tls_peer peer;
			void *ssl;
			char ev[256];

			if (tfd >= 0) {
				ssl = retraced_tls_accept(g_tls_ctx, tfd,
					&peer);
				if (ssl == NULL) {
					snprintf(ev, sizeof(ev),
						"{\"name\":\"retrace.auth.tls_refused\"}");
					retraced_journal_event(&jr,
						(long)time(NULL), "daemon",
						0, ev);
					close(tfd);
				} else {
					g_ctl_fd = tfd;
					g_ctl_ssl = ssl;
					g_ctl_fill = 0;
					g_ctl.scopes = peer.scopes;
					snprintf(ev, sizeof(ev),
						"{\"name\":\"retrace.auth.tls\",\"cn\":\"%s\",\"scopes\":%u}",
						peer.cn,
						(unsigned int)peer.scopes);
					retraced_journal_event(&jr,
						(long)time(NULL), "daemon",
						0, ev);
					printf("retraced: TLS peer %s scopes=0x%x\n",
						peer.cn,
						(unsigned int)peer.scopes);
				}
			}
		}
		if (g_ctl_fd >= 0) {
			struct pollfd *cfd = NULL;

			for (i = 0; i < nfd; i++) {
				if (pfds[i].fd == g_ctl_fd)
					cfd = &pfds[i];
			}
			if (cfd != NULL &&
			    (cfd->revents & (POLLIN | POLLHUP)))
				handle_ctl_readable(conns, &reg, &jr);
		}

		if (pfds[0].revents & POLLIN) {
			int fd = accept_gated(listen_fd, &jr);

			if (fd >= 0) {
				int slot = -1;

				for (i = 0; i < MAX_AGENTS; i++) {
					if (conns[i].fd < 0) {
						slot = i;
						break;
					}
				}
				if (slot >= 0) {
					/*
					 * Field init only -- NOT memset of
					 * the whole struct: each conn
					 * carries a 1 MiB frame buffer,
					 * and a full memset COMMITS that
					 * MiB of RSS per accepted agent
					 * (128 agents = 128 MiB for zero
					 * reason). The buffer's contents
					 * are governed by `fill`; zero
					 * pages stay untouched until the
					 * peer actually sends.
					 */
					conns[slot].fd = fd;
					conns[slot].fill = 0;
					conns[slot].agent_id[0] = '\0';
					conns[slot].helloed = 0;
					conns[slot].spectator = 0;
				} else {
					close(fd);
				}
			}
		}
		for (i = 0; i < MAX_AGENTS; i++) {
			if (conns[i].fd < 0)
				continue;
			if (pfd_of[i] < 0 ||
			    !(pfds[pfd_of[i]].revents &
			      (POLLIN | POLLHUP)))
				continue;
			{
				ssize_t n = read(conns[i].fd,
					conns[i].buf + conns[i].fill,
					sizeof(conns[i].buf) -
						conns[i].fill);

				if (n <= 0) {
					close(conns[i].fd);
					conns[i].fd = -1;
					/* connection-scoped
					 * durability: a finished
					 * conversation is on disk
					 * even if the daemon is
					 * killed before its graceful
					 * close. The leaver's final
					 * drift summary rides the
					 * same flush.
					 */
					daemon_frame_drift_summaries(
						&reg, &jr);
					retraced_journal_flush(&jr);
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
			emit_drift_summaries(&reg, &jr);
			{
				size_t stale = retraced_registry_sweep(&reg,
					now_ms(), HB_TIMEOUT_MS);

				if (stale > 0)
					printf("retraced: %zu agent(s) stale\n",
						stale);
			}
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
	emit_drift_summaries(&reg, &jr);
	retraced_journal_close(&jr);
	ctl_drop();
	if (g_tls_listen >= 0)
		close(g_tls_listen);
	retraced_tls_free(g_tls_ctx);
	if (unlink_sock)
		unlink(sock_path);
	free(conns);
	return 0;
}
