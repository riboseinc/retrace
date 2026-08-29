/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * retraced on Windows (TODO.supervisor/12 P0): the named-pipe
 * transport. Protocol, journal, registry, nonce roles, and the
 * ctl command surface are shared with the POSIX daemon; only
 * the accept/read machinery differs (a pipe instance per
 * client, a service thread each, a global lock around the
 * shared registry/journal). The control pipe carries the same
 * newline-JSON line protocol the UDS ctl socket does.
 *
 * The protocol state machine here deliberately mirrors main.c's
 * handler (the same shape the conformance reference stub
 * carries); the conformance suite pins both to protocol.h.
 */

#include "pipe_server.h"

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <sddl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "journal.h"
#include "protocol.h"
#include "registry.h"
#include "retraced_ctl.h"
#include "tls_gate.h"
#include "daemon_frame.h"

#include "parson.h"

#define PIPE_AGENTS_MAX 32
#define CTL_LINE_MAX 8192
#define SWEEP_INTERVAL_MS 1000

struct pipe_conn {
	HANDLE pipe;
	OVERLAPPED ov;
	int live;
	char agent_id[RETRACED_AGENT_ID_MAX];
	int helloed;
	int spectator;
};

static struct retraced_registry g_reg;
static struct retraced_journal g_jr;
static CRITICAL_SECTION g_lock;
static volatile LONG g_stop;
static char g_nonce[65];
static struct retraced_ctl_ctx g_ctl;
static struct conn g_ctl_conns[MAX_AGENTS];
static HANDLE g_ctl_thread;
static HANDLE g_ctl_pipe;

/* the transport seam for the shared frame module: pipe-backed */
static int df_write_pipe(void *io, uint16_t type, const char *payload)
{
	return write_frame((HANDLE)io, type, payload);
}

static long now_ms(void)
{
	return (long)GetTickCount64();
}

/* ---- framing (byte-mode pipe; same RTRD wire format) ---- */

static int pipe_read_all(HANDLE h, void *buf, DWORD n)
{
	DWORD got = 0;
	char *p = (char *)buf;
	size_t left = n;

	while (left > 0) {
		if (!ReadFile(h, p, (DWORD)left, &got, NULL) || got == 0)
			return -1;
		p += got;
		left -= got;
	}
	return 0;
}

static int pipe_write_all(HANDLE h, const void *buf, DWORD n)
{
	DWORD put = 0;
	const char *p = (const char *)buf;
	size_t left = n;

	while (left > 0) {
		if (!WriteFile(h, p, (DWORD)left, &put, NULL) || put == 0)
			return -1;
		p += put;
		left -= put;
	}
	return 0;
}

static int write_frame(HANDLE h, uint16_t type, const char *payload)
{
	char hdr[RETRACE_RPC_HEADER_SZ];
	DWORD plen = (DWORD)strlen(payload);

	memcpy(hdr, "RTRD", 4);
	{
		uint16_t v = 1, t = type;
		uint32_t l = plen;

		memcpy(hdr + 4, &v, 2);
		memcpy(hdr + 6, &t, 2);
		memcpy(hdr + 8, &l, 4);
	}
	if (pipe_write_all(h, hdr, sizeof(hdr)) != 0)
		return -1;
	if (plen > 0 && pipe_write_all(h, payload, plen) != 0)
		return -1;
	return 0;
}

static int recv_frame(HANDLE h, struct retrace_rpc_frame *fr,
	char *payload, size_t cap)
{
	char hdr[RETRACE_RPC_HEADER_SZ];
	uint16_t v, t;
	uint32_t l;

	if (pipe_read_all(h, hdr, sizeof(hdr)) != 0)
		return -1;
	if (memcmp(hdr, "RTRD", 4) != 0)
		return -1;
	memcpy(&v, hdr + 4, 2);
	memcpy(&t, hdr + 6, 2);
	memcpy(&l, hdr + 8, 4);
	if (v != 1 || l > RETRACE_RPC_PAYLOAD_MAX || l >= cap)
		return -1;
	if (l > 0 && pipe_read_all(h, payload, l) != 0)
		return -1;
	payload[l] = '\0';
	memset(fr, 0, sizeof(*fr));
	fr->version = v;
	fr->type = t;
	fr->length = l;
	return 0;
}

/* ---- the protocol state machine (mirrors main.c) ---- */


static int handle_agent_frame(struct pipe_conn *c,
	const struct retrace_rpc_frame *fr, const char *payload)
{
	JSON_Value *v;
	JSON_Object *o;

	switch (fr->type) {
	case RETRACE_RPC_MSG_HELLO: {
		struct daemon_conn_state st;
		struct agent_entry *e;

		st.helloed = c->helloed;
		st.spectator = c->spectator;
		snprintf(st.agent_id, sizeof(st.agent_id), "%s",
			c->agent_id);
		e = daemon_frame_hello(&st, payload, &g_reg, &g_jr,
			&g_ctl, g_nonce, df_write_pipe, (void *)c->pipe);
		if (e == NULL) {
			fprintf(stderr,
				"retraced: registry full; refusing agent\n");
			return -1;
		}
		c->helloed = st.helloed;
		c->spectator = st.spectator;
		snprintf(c->agent_id, sizeof(c->agent_id), "%s",
			st.agent_id);
		return 0;
	}
	case RETRACE_RPC_MSG_HEARTBEAT:
	case RETRACE_RPC_MSG_EVENT:
	case RETRACE_RPC_MSG_POLICY_ACK:
	case RETRACE_RPC_MSG_PING:
	case RETRACE_RPC_MSG_BYE: {
		struct daemon_conn_state st;
		int rc;

		st.helloed = c->helloed;
		st.spectator = c->spectator;
		snprintf(st.agent_id, sizeof(st.agent_id), "%s",
			c->agent_id);
		rc = daemon_frame_handle(&st, fr->type, payload,
			now_ms(), &g_reg, &g_jr, &g_ctl, g_nonce,
			df_write_pipe, (void *)c->pipe);
		c->helloed = st.helloed;
		return rc;
	}
	default:
		return 0;
	}
}

static DWORD WINAPI agent_thread(LPVOID arg)
{
	struct pipe_conn *c = (struct pipe_conn *)arg;
	struct retrace_rpc_frame fr;
	char *payload = (char *)malloc(RETRACE_RPC_PAYLOAD_MAX + 1);
	int bye = 0;

	if (payload != NULL) {
		while (!g_stop && !bye) {
			if (recv_frame(c->pipe, &fr, payload,
				    RETRACE_RPC_PAYLOAD_MAX) != 0)
				break;
			EnterCriticalSection(&g_lock);
			bye = handle_agent_frame(c, &fr, payload) != 0;
			LeaveCriticalSection(&g_lock);
		}
		free(payload);
	}
	FlushFileBuffers(c->pipe);
	DisconnectNamedPipe(c->pipe);
	CloseHandle(c->pipe);
	c->pipe = NULL;
	c->live = 0;
	return 0;
}

/* ---- drift summaries (the same heartbeat-grade) ---- */

static void emit_drift_summaries(void)
{
	daemon_frame_drift_summaries(&g_reg, &g_jr);
}}

/* ---- the ctl pipe: same line protocol as the UDS ctl ---- */

struct ctl_reply_ctx {
	HANDLE pipe;
};

static void ctl_reply_pipe(const char *line, void *user)
{
	struct ctl_reply_ctx *ctx = (struct ctl_reply_ctx *)user;

	pipe_write_all(ctx->pipe, line, (DWORD)strlen(line));
}

static DWORD WINAPI ctl_thread(LPVOID arg)
{
	char buf[CTL_LINE_MAX];
	size_t fill = 0;
	(void)arg;

	while (!g_stop) {
		DWORD got = 0;

		if (!ReadFile(g_ctl_pipe, buf + fill,
			    (DWORD)(sizeof(buf) - 1 - fill), &got,
			    NULL) || got == 0)
			break;
		fill += got;
		buf[fill] = '\0';
		{
			char *nl;

			while ((nl = strchr(buf, '\n')) != NULL) {
				struct ctl_reply_ctx ctx;

				*nl = '\0';
				ctx.pipe = g_ctl_pipe;
				EnterCriticalSection(&g_lock);
				g_ctl.reply_sink = ctl_reply_pipe;
				g_ctl.reply_user = &ctx;
				retraced_ctl_handle_line(&g_ctl, buf);
				LeaveCriticalSection(&g_lock);
				memmove(buf, nl + 1,
					fill - (size_t)(nl + 1 - buf));
				fill -= (size_t)(nl + 1 - buf);
			}
		}
		if (fill >= sizeof(buf) - 1)
			fill = 0;
	}
	FlushFileBuffers(g_ctl_pipe);
	DisconnectNamedPipe(g_ctl_pipe);
	CloseHandle(g_ctl_pipe);
	g_ctl_pipe = NULL;
	return 0;
}

/* ---- accept loop ---- */

/*
 * Explicit DACL (supervisor/12 P1 hardening): the default pipe
 * DACL follows the process token's default, which broader
 * contexts can widen. We set our own: the token's OWNER gets
 * full duplex, Administrators and SYSTEM too (ops reality), and
 * NO world/Everyone grant -- the PEERCRED equivalent on Windows.
 */
static HANDLE make_pipe(const char *name)
{
	char owner[128];
	SECURITY_ATTRIBUTES sa;
	PSECURITY_DESCRIPTOR sd = NULL;
	HANDLE h;

	owner[0] = '\0';
	{
		HANDLE tok = NULL;

		if (OpenProcessToken(GetCurrentProcess(),
			    TOKEN_QUERY, &tok)) {
			DWORD need = 0;
			TOKEN_USER *tu = NULL;

			GetTokenInformation(tok, TokenUser, NULL, 0,
				&need);
			if (need > 0) {
				tu = (TOKEN_USER *)HeapAlloc(
					GetProcessHeap(), 0, need);
				if (tu != NULL &&
				    GetTokenInformation(tok, TokenUser,
					    tu, need, &need)) {
					LPSTR sid = NULL;

					if (ConvertSidToStringSidA(
						    tu->User.Sid, &sid)) {
						snprintf(owner,
							sizeof(owner),
							"%s", sid);
						LocalFree(sid);
					}
				}
				HeapFree(GetProcessHeap(), 0, tu);
			}
			CloseHandle(tok);
		}
	}
	sa.nLength = sizeof(sa);
	sa.bInheritHandle = FALSE;
	if (owner[0] != '\0') {
		char sddl[256];

		snprintf(sddl, sizeof(sddl),
			"D:P(A;;GA;;;BA)(A;;GA;;;SY)(A;;GRGW;;;%s)",
			owner);
		if (!ConvertStringSecurityDescriptorToSecurityDescriptorA(
			    sddl, SDDL_REVISION_1, &sd, NULL))
			sd = NULL;
	}
	sa.lpSecurityDescriptor = sd;

	h = CreateNamedPipeA(name,
		PIPE_ACCESS_DUPLEX,
		PIPE_TYPE_BYTE | PIPE_READMODE_BYTE |
		PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
		PIPE_UNLIMITED_INSTANCES,
		64 * 1024, 64 * 1024, 0,
		sd != NULL ? &sa : NULL);
	if (sd != NULL)
		LocalFree(sd);
	return h;
}

static BOOL WINAPI on_console_ctrl(DWORD type)
{
	(void)type;
	InterlockedExchange(&g_stop, 1);
	return TRUE;
}

int retraced_pipe_main(int argc, char **argv)
{
	const char *pipe_name = "\\\\.\\pipe\\retraced-agent";
	const char *ctl_name = "\\\\.\\pipe\\retraced-ctl";
	const char *journal_path = "retraced-journal.jsonl";
	const char *policy_path = NULL;
	const char *nonce_arg = NULL;
	const char *nonce_file = NULL;
	struct pipe_conn conns[PIPE_AGENTS_MAX];
	int i;

	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--sock") == 0 && i + 1 < argc)
			pipe_name = argv[++i];
		else if (strcmp(argv[i], "--journal") == 0 && i + 1 < argc)
			journal_path = argv[++i];
		else if (strcmp(argv[i], "--policy") == 0 && i + 1 < argc)
			policy_path = argv[++i];
		else if (strcmp(argv[i], "--ctl") == 0 && i + 1 < argc)
			ctl_name = argv[++i];
		else if (strcmp(argv[i], "--nonce") == 0 && i + 1 < argc)
			nonce_arg = argv[++i];
		else if (strcmp(argv[i], "--nonce-file") == 0 &&
			 i + 1 < argc)
			nonce_file = argv[++i];
		else {
			fprintf(stderr,
				"Usage: retraced [--sock PIPE] [--journal PATH]\n"
				"                 [--policy FILE] [--ctl PIPE]\n"
				"                 [--nonce HEX32] [--nonce-file PATH]\n"
				"  (Windows: --sock/--ctl name named pipes;\n"
				"   TLS/fd/user flags ride the POSIX daemon)\n");
			return 2;
		}
	}

	InitializeCriticalSection(&g_lock);
	SetConsoleCtrlHandler(on_console_ctrl, TRUE);

	retraced_registry_init(&g_reg);
	retraced_journal_open(&g_jr, journal_path);
	if (retraced_journal_replay(&g_jr, &g_reg) != 0)
		printf("retraced: no prior journal (fresh boot)\n");

	if (nonce_arg != NULL) {
		snprintf(g_nonce, sizeof(g_nonce), "%s", nonce_arg);
	} else {
		snprintf(g_nonce, sizeof(g_nonce), "%08x%08x%08x%08x",
			(unsigned int)(GetCurrentProcessId() ^ GetTickCount()),
			(unsigned int)rand(), (unsigned int)rand(),
			(unsigned int)rand());
	}
	if (nonce_file != NULL) {
		FILE *f = fopen(nonce_file, "w");

		if (f != NULL) {
			fprintf(f, "%s\n", g_nonce);
			fclose(f);
		}
	}

	/* policy load (read-only broadcast on Windows P0: the
	 * agent-side pipe port carries POLICY_SET delivery)
	 */
	for (i = 0; i < MAX_AGENTS; i++)
		g_ctl_conns[i].fd = -1;
	g_ctl.conns = g_ctl_conns;
	g_ctl.reg = &g_reg;
	g_ctl.jr = &g_jr;
	g_ctl.scopes = RETRACED_SCOPE_ALL;
	if (policy_path != NULL) {
		/* the shared loading seam (wrapper descent + epochs) */
		FILE *f = fopen(policy_path, "rb");
		char text[65536];
		size_t n = 0;

		if (f != NULL) {
			n = fread(text, 1, sizeof(text) - 1, f);
			fclose(f);
		}
		text[n] = '\0';
		if (n > 0) {
			char *blob = NULL;
			long epoch = 0;

			if (retraced_policy_load(text, &blob, &epoch)
			    == 0) {
				retraced_ctl_set_policy(&g_ctl, blob,
					epoch);
				free(blob);
			}
		}
	}
	memset(conns, 0, sizeof(conns));

	/* the ctl pipe: one instance, one client at a time */
	g_ctl_pipe = make_pipe(ctl_name);
	if (g_ctl_pipe != INVALID_HANDLE_VALUE)
		g_ctl_thread = CreateThread(NULL, 0, ctl_thread,
			NULL, 0, NULL);

	printf("retraced: listening on %s (agents)\n", pipe_name);
	{
		long last_sweep = now_ms();
		HANDLE evt = CreateEvent(NULL, TRUE, FALSE, NULL);
		HANDLE h = make_pipe(pipe_name);
		OVERLAPPED ov;

		memset(&ov, 0, sizeof(ov));
		ov.hEvent = evt;
		ConnectNamedPipe(h, &ov);
		while (!g_stop) {
			/*
			 * ONE pending accept at a time (classic pattern):
			 * the event fires when a client lands; the 250ms
			 * cadence lets sweeps and stop run regardless.
			 */
			if (WaitForSingleObject(evt, 250) == WAIT_OBJECT_0) {
				int slot = -1;
				int k;

				for (k = 0; k < PIPE_AGENTS_MAX; k++) {
					if (!conns[k].live) {
						slot = k;
						break;
					}
				}
				if (slot >= 0) {
					HANDLE th;

					conns[slot].pipe = h;
					conns[slot].live = 1;
					conns[slot].helloed = 0;
					conns[slot].spectator = 0;
					th = CreateThread(NULL, 0, agent_thread,
						&conns[slot], 0, NULL);
					if (th != NULL)
						CloseHandle(th);
				} else {
					/* full: drop the connection */
					DisconnectNamedPipe(h);
					CloseHandle(h);
				}
				h = make_pipe(pipe_name);
				ResetEvent(evt);
				memset(&ov, 0, sizeof(ov));
				ov.hEvent = evt;
				ConnectNamedPipe(h, &ov);
				continue;
			}
			if (now_ms() - last_sweep > SWEEP_INTERVAL_MS) {
				EnterCriticalSection(&g_lock);
				emit_drift_summaries();
				retraced_registry_sweep(&g_reg, now_ms(),
					15000);
				LeaveCriticalSection(&g_lock);
				last_sweep = now_ms();
			}
		}
		DisconnectNamedPipe(h);
		CloseHandle(h);
	}

	/* graceful stop: flush the routine tail */
	EnterCriticalSection(&g_lock);
	emit_drift_summaries();
	LeaveCriticalSection(&g_lock);
	retraced_journal_close(&g_jr);
	if (g_ctl_pipe != NULL) {
		DisconnectNamedPipe(g_ctl_pipe);
		CloseHandle(g_ctl_pipe);
	}
	DeleteCriticalSection(&g_lock);
	return 0;
}

/*
 * write_frame normally lives in main.c's TU (the POSIX daemon);
 * on Windows that file is not linked, and policy broadcast is
 * P0-off (the agent-side pipe port delivers POLICY_SET).
 */
int write_frame(int fd, uint16_t type, const char *payload);

int write_frame(int fd, uint16_t type, const char *payload)
{
	(void)fd;
	(void)type;
	(void)payload;
	return 0;
}

int main(int argc, char **argv)
{
	return retraced_pipe_main(argc, argv);
}

#endif /* _WIN32 */
