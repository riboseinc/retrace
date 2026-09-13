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
#include <shellapi.h>
#include <winsvc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "journal.h"
#include "inject.h"
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
	struct conn *ctl;	/* the broadcast registration */
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
static const char *g_ctl_name = "\\\\.\\pipe\\retraced-ctl";

/*
 * Spawned workloads (TODO.impl/11): the reaped-children table.
 * POSIX has SIGCHLD + waitpid; Windows' analogue is holding
 * each child's PROCESS handle and polling it on the sweep --
 * the journal record is the same either way (the reap
 * doctrine). The daemon's only spawn site is the ctl seam, so
 * every handle here is a workload this daemon launched.
 */
#define WIN_CHILDREN_MAX 128
static struct {
	HANDLE h;
	DWORD pid;
} g_win_children[WIN_CHILDREN_MAX];
static const char *g_agent_pipe_for_spawn;
/*
 * RETRACED_TRACE=1: the sweep narrates (loop liveness, table,
 * raw wait results) -- the E2E's forensics switch. Narration
 * rides the JOURNAL (durable, flushed, cross-thread): stdout
 * proved unreliable as a forensic channel.
 */
static int g_trace;

static void trace_event(const char *detail)
{
	char ev[256];

	if (!g_trace)
		return;
	snprintf(ev, sizeof(ev),
		"{\"name\":\"retrace.trace\",\"detail\":\"%s\"}",
		detail);
	EnterCriticalSection(&g_lock);
	retraced_journal_event(&g_jr, (long)time(NULL), "daemon",
		0, ev);
	LeaveCriticalSection(&g_lock);
}

static int pipe_write_frame(HANDLE h, uint16_t type,
	const char *payload);

/* the ctl broadcast's pipe arm */
static int ctl_conn_send_pipe(void *io, uint16_t type,
	const char *payload)
{
	return pipe_write_frame((HANDLE)io, type, payload);
}

/* the transport seam for the shared frame module: pipe-backed */
static int df_write_pipe(void *io, uint16_t type, const char *payload)
{
	return pipe_write_frame((HANDLE)io, type, payload);
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

static int pipe_write_frame(HANDLE h, uint16_t type,
	const char *payload)
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
		if (c->ctl != NULL) {
			c->ctl->helloed = c->helloed;
			c->ctl->spectator = c->spectator;
			snprintf(c->ctl->agent_id,
				sizeof(c->ctl->agent_id), "%s",
				c->agent_id);
		}
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
		if (c->ctl != NULL)
			c->ctl->helloed = c->helloed;
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
	if (c->ctl != NULL) {
		/* a concurrent push holds the same lock */
		EnterCriticalSection(&g_lock);
		c->ctl->io = NULL;
		c->ctl->helloed = 0;
		c->ctl = NULL;
		LeaveCriticalSection(&g_lock);
	}
	/* connection-scoped durability (same as the POSIX conn
	 * end): observations are buffered-class records, and a
	 * force-killed daemon must not swallow a finished
	 * conversation
	 */
	EnterCriticalSection(&g_lock);
	daemon_frame_drift_summaries(&g_reg, &g_jr);
	retraced_journal_flush(&g_jr);
	LeaveCriticalSection(&g_lock);
	return 0;
}

/* ---- spawn + reap (TODO.impl/11): the audited plane ----- */

/*
 * Quote one argv word for a Windows command line so the
 * target's CommandLineToArgvW argv round-trips: a run of n
 * backslashes doubles only when a quote or the closing quote
 * follows, and an embedded quote is escaped. The whole word
 * is wrapped so paths with spaces survive.
 */
static size_t
argv_quote(const char *arg, char *out, size_t cap)
{
	const char *p = arg;
	size_t o = 0;

	if (cap < 3)
		return 0;
	out[o++] = '"';
	while (*p != '\0') {
		size_t bs = 0;
		size_t k;
		size_t emit;

		while (p[bs] == '\\')
			bs++;
		p += bs;
		if (*p == '"') {
			/* 2n backslashes, then an escaped quote */
			for (k = 0; k < 2 * bs; k++) {
				if (o + 2 >= cap)
					return 0;
				out[o++] = '\\';
			}
			if (o + 2 >= cap)
				return 0;
			out[o++] = '\\';
			out[o++] = '"';
			p++;
			continue;
		}
		/* trailing backslashes double (closing quote next) */
		emit = (*p == '\0') ? 2 * bs : bs;
		for (k = 0; k < emit; k++) {
			if (o + 2 >= cap)
				return 0;
			out[o++] = '\\';
		}
		if (*p != '\0') {
			if (o + 2 >= cap)
				return 0;
			out[o++] = *p++;
		}
	}
	if (o + 1 >= cap)
		return 0;
	out[o++] = '"';
	out[o] = '\0';
	return o;
}

static long ctl_spawn_win(const char *const *argv,
	const char *preload, char *err_out, size_t err_cap)
{
	char cmdline[1024];
	char env[32768];
	char dll_buf[MAX_PATH];
	const char *dll_path;
	int i;
	size_t o;
	DWORD pid;
	HANDLE child = NULL;

	if (argv == NULL || argv[0] == NULL) {
		snprintf(err_out, err_cap, "empty argv");
		return -1;
	}

	/* the command line: quoted words, space-separated */
	o = 0;
	for (i = 0; argv[i] != NULL; i++) {
		size_t w = argv_quote(argv[i], cmdline + o,
			sizeof(cmdline) - o);

		if (w == 0) {
			snprintf(err_out, err_cap, "argv too long");
			return -1;
		}
		o += w;
		if (argv[i + 1] != NULL && o + 1 < sizeof(cmdline))
			cmdline[o++] = ' ';
	}
	cmdline[o] = '\0';

	dll_path = preload != NULL && preload[0] != '\0' ?
		preload : NULL;
	if (dll_path == NULL) {
		/* default: the retrace.dll beside this daemon */
		char *slash;

		GetModuleFileNameA(NULL, dll_buf, sizeof(dll_buf));
		slash = strrchr(dll_buf, '\\');
		if (slash != NULL) {
			snprintf(slash + 1,
				sizeof(dll_buf) -
					(size_t)(slash + 1 - dll_buf),
				"retrace.dll");
			dll_path = dll_buf;
		} else {
			dll_path = "retrace.dll";
		}
	}

	/*
	 * The environment: arm the workload exactly as the POSIX
	 * seam's fork+exec arms it -- INHERIT the parent's
	 * environment and override the arming keys (supervisor
	 * role, the agent pipe (its connection target), the nonce
	 * (the threat model's "handed to spawners"), EAGER (join
	 * without waiting for a queued event)). A replaced block
	 * would strip PATH/SystemRoot from a real workload;
	 * inheritance is the fork+exec semantic. Injection
	 * replaces LD_PRELOAD.
	 *
	 * The blob is NUL-separated NAME=VALUE strings with a
	 * double-NUL terminator -- built one segment at a time
	 * because *printf stops at the first NUL of its format.
	 */
	{
		static const char *const names[] = {
			"RETRACE_SUPERVISOR",
			"RETRACE_SUPERVISOR_SOCK",
			"RETRACE_SUPERVISOR_NONCE",
			"RETRACE_SUPERVISOR_EAGER",
		};
		const char *vals[4];
		char *env_all;
		size_t n = 0;
		size_t dll_dir_len;
		int saw_path = 0;
		int k;

		vals[0] = "1";
		vals[1] = g_agent_pipe_for_spawn != NULL ?
			g_agent_pipe_for_spawn : "";
		vals[2] = g_nonce;
		vals[3] = "1";

		/*
		 * PATH gets the dll's directory PREPENDED: the
		 * library's imports (libcrypto on the vcpkg build)
		 * are not system DLLs and sit beside the dll -- the
		 * loader's dependency search must reach them or
		 * the injected LoadLibraryA fails (round 1's
		 * ERROR_MOD_NOT_FOUND).
		 */
		dll_dir_len = 0;
		{
			const char *slash = strrchr(dll_path, '\\');

			if (slash != NULL)
				dll_dir_len = (size_t)(slash - dll_path);
		}
		env_all = GetEnvironmentStringsA();
		if (env_all != NULL) {
			const char *e = env_all;

			while (*e != '\0') {
				size_t l = strlen(e);
				int skip = 0;

				for (k = 0; k < 4; k++) {
					size_t nl = strlen(names[k]);

					if (strncmp(e, names[k], nl) ==
					    0 && e[nl] == '=') {
						skip = 1;
						break;
					}
				}
				if (!skip && dll_dir_len > 0 &&
				    _strnicmp(e, "PATH=", 5) == 0) {
					saw_path = 1;
					/* override in place: PATH=dll_dir;old */
					{
						size_t need = 5 + dll_dir_len +
							l + 3;

						if (n + need >= sizeof(env)) {
							FreeEnvironmentStringsA(
								env_all);
							snprintf(err_out,
								err_cap,
								"env too long");
							return -1;
						}
					}
					memcpy(env + n, "PATH=", 5);
					n += 5;
					memcpy(env + n, dll_path, dll_dir_len);
					n += dll_dir_len;
					env[n++] = ';';
					memcpy(env + n, e, l + 1);
					n += l + 1;
					skip = 1;
				}
				if (!skip) {
					if (n + l + 1 + 1 >= sizeof(env)) {
						FreeEnvironmentStringsA(
							env_all);
						snprintf(err_out, err_cap,
							"env too long");
						return -1;
					}
					memcpy(env + n, e, l + 1);
					n += l + 1;
				}
				e += l + 1;
			}
			FreeEnvironmentStringsA(env_all);
		}
		if (dll_dir_len > 0 && !saw_path) {
			/* a parent without PATH still resolves imports */
			if (n + 5 + dll_dir_len + 2 >= sizeof(env)) {
				snprintf(err_out, err_cap, "env too long");
				return -1;
			}
			memcpy(env + n, "PATH=", 5);
			n += 5;
			memcpy(env + n, dll_path, dll_dir_len);
			n += dll_dir_len;
			env[n++] = '\0';
		}
		for (k = 0; k < 4; k++) {
			if (n + strlen(names[k]) + strlen(vals[k]) + 3 >=
			    sizeof(env)) {
				snprintf(err_out, err_cap, "env too long");
				return -1;
			}
			n += (size_t)snprintf(env + n, sizeof(env) - n,
				"%s=%s", names[k], vals[k]);
			env[n++] = '\0';
		}
		env[n] = '\0';		/* the terminator */
	}

	pid = retrace_win_inject_spawn(cmdline, dll_path, env,
		&child);
	if (pid == 0) {
		snprintf(err_out, err_cap,
			"launch/inject failed (GetLastError %lu)",
			(unsigned long)GetLastError());
		return -1;
	}

	/* register for the reap sweep */
	for (i = 0; i < WIN_CHILDREN_MAX; i++) {
		if (g_win_children[i].h == NULL) {
			g_win_children[i].h = child;
			g_win_children[i].pid = pid;
			return (long)pid;
		}
	}
	/* table full: close and refuse honestly */
	CloseHandle(child);
	snprintf(err_out, err_cap, "child table full");
	return -1;
}

/*
 * SIGCHLD's analogue: poll every held child; journal each
 * departure (how/code). Windows exit codes carry NTSTATUS for
 * fatal exits -- recorded verbatim ("exited"), the honest
 * mapping; the verdict layer reads the code.
 */
static void win_reap_sweep(void)
{
	int i;

	for (i = 0; i < WIN_CHILDREN_MAX; i++) {
		DWORD code = 0;

		if (g_win_children[i].h == NULL)
			continue;
		{
			DWORD w = WaitForSingleObject(
				g_win_children[i].h, 0);

			if (w != WAIT_OBJECT_0) {
				if (g_trace) {
					char line[80];

					snprintf(line, sizeof(line),
						"wait pid=%lu w=%lu",
						(unsigned long)
							g_win_children[i].pid,
						(unsigned long)w);
					trace_event(line);
				}
				continue;	/* not signaled */
			}
		}
		if (GetExitCodeProcess(g_win_children[i].h, &code)) {
			char ev[160];

			snprintf(ev, sizeof(ev),
				"{\"name\":\"retrace.ctl.exit\","
				"\"pid\":%lu,\"how\":\"exited\","
				"\"code\":%lu}",
				(unsigned long)g_win_children[i].pid,
				(unsigned long)code);
			retraced_journal_event(&g_jr,
				(long)time(NULL), "daemon", 0, ev);
			printf("retraced: reaped pid %lu code %lu\n",
				(unsigned long)g_win_children[i].pid,
				(unsigned long)code);
			fflush(stdout);
		}
		CloseHandle(g_win_children[i].h);
		g_win_children[i].h = NULL;
		g_win_children[i].pid = 0;
	}
}

/* ---- drift summaries (the same heartbeat-grade) ---- */

static void emit_drift_summaries(void)
{
	daemon_frame_drift_summaries(&g_reg, &g_jr);
}
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
	(void)arg;

	/*
	 * One pipe instance per controller, until g_stop: the
	 * single-client shape closed the ctl plane with the first
	 * disconnect -- the fleet CLI's second command could not
	 * even connect. Blocking ConnectNamedPipe in this thread
	 * is fine: daemon exit tears the process down anyway.
	 */
	while (!g_stop) {
		char buf[CTL_LINE_MAX];
		size_t fill = 0;
		HANDLE c = make_pipe(g_ctl_name);

		if (c == INVALID_HANDLE_VALUE)
			break;
		ConnectNamedPipe(c, NULL);
		if (g_stop) {
			CloseHandle(c);
			break;
		}
		g_ctl_pipe = c;
		for (;;) {
			DWORD got = 0;

			if (!ReadFile(c, buf + fill,
				    (DWORD)(sizeof(buf) - 1 - fill),
				    &got, NULL) || got == 0)
				break;
			fill += got;
			buf[fill] = '\0';
			{
				char *nl;

				while ((nl = strchr(buf, '\n')) != NULL) {
					struct ctl_reply_ctx ctx;

					*nl = '\0';
					ctx.pipe = c;
					EnterCriticalSection(&g_lock);
					g_ctl.reply_sink = ctl_reply_pipe;
					g_ctl.reply_user = &ctx;
					retraced_ctl_handle_line(&g_ctl,
						buf);
					LeaveCriticalSection(&g_lock);
					memmove(buf, nl + 1,
						fill - (size_t)(nl + 1 - buf));
					fill -= (size_t)(nl + 1 - buf);
				}
			}
			if (fill >= sizeof(buf) - 1)
				fill = 0;
		}
		FlushFileBuffers(c);
		DisconnectNamedPipe(c);
		CloseHandle(c);
		g_ctl_pipe = NULL;
	}
	return 0;
}

/* ---- accept loop ---- */

/*
 * Arm one pending accept. ConnectNamedPipe returning FALSE
 * without ERROR_IO_PENDING means a client's CreateFile beat the
 * arm (ERROR_PIPE_CONNECTED) or the instance is unusable: in
 * both cases the event NEVER fires, the loop would sleep on it
 * forever, and every later connect reads ERROR_PIPE_BUSY -- the
 * slow-leg jretrace refusal. Take the accept door immediately
 * instead (a broken handle simply fails fast in its thread).
 */
static void arm_accept(HANDLE h, OVERLAPPED *ov)
{
	if (h == INVALID_HANDLE_VALUE)
		return;
	if (!ConnectNamedPipe(h, ov) &&
	    GetLastError() != ERROR_IO_PENDING)
		SetEvent(ov->hEvent);
}


static BOOL WINAPI on_console_ctrl(DWORD type)
{
	(void)type;
	InterlockedExchange(&g_stop, 1);
	return TRUE;
}

/* ---- the SCM service lane (TODO.supervisor/12 P1) ---- */

/*
 * One binary, two launches: `sc start` lands here through the
 * dispatcher (which blocks for the service lifetime); a console
 * launch gets ERROR_FAILED_SERVICE_STARTUP back from the
 * dispatcher and runs the accept loop itself. STOP maps to the
 * same g_stop the console Ctrl handler flips -- one graceful
 * exit path (journal flush included) for both.
 */
static SERVICE_STATUS_HANDLE g_svc;
static SERVICE_STATUS g_svc_st;
static DWORD g_svc_ret;

static void svc_report(DWORD state, DWORD wait_ms)
{
	g_svc_st.dwCurrentState = state;
	g_svc_st.dwWaitHint = wait_ms;
	SetServiceStatus(g_svc, &g_svc_st);
}

static VOID WINAPI svc_handler(DWORD ctrl)
{
	if (ctrl == SERVICE_CONTROL_STOP) {
		svc_report(SERVICE_STOP_PENDING, 5000);
		InterlockedExchange(&g_stop, 1);
	}
}

static VOID WINAPI service_main(DWORD argc, char **argv)
{
	wchar_t **wargv;
	char **nargv = NULL;
	int nargs = 0, k;

	g_svc = RegisterServiceCtrlHandlerA(argv[0], svc_handler);
	if (g_svc == NULL)
		return;
	memset(&g_svc_st, 0, sizeof(g_svc_st));
	g_svc_st.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	g_svc_st.dwControlsAccepted = SERVICE_ACCEPT_STOP;
	svc_report(SERVICE_START_PENDING, 1000);
	/*
	 * ServiceMain's argv is the SCM's space-split of binPath --
	 * quotes left embedded, and on some hosts the flags never
	 * arrive at all (the daemon then serves its DEFAULT pipe
	 * names and the registered --sock is silently ignored).
	 * The process's own command line is the truth: parse it
	 * quote-aware and run the same shape a console launch
	 * parses.
	 */
	wargv = CommandLineToArgvW(GetCommandLineW(), &nargs);
	if (wargv != NULL && nargs > 1) {
		nargv = (char **)HeapAlloc(GetProcessHeap(), 0,
			(size_t)nargs * sizeof(*nargv));
		if (nargv != NULL) {
			for (k = 0; k < nargs; k++) {
				int need = WideCharToMultiByte(CP_UTF8, 0,
					wargv[k], -1, NULL, 0, NULL,
					NULL);

				nargv[k] = (char *)HeapAlloc(
					GetProcessHeap(), 0,
					(size_t)(need > 0 ? need : 1));
				if (nargv[k] != NULL && need > 0)
					WideCharToMultiByte(CP_UTF8, 0,
						wargv[k], -1, nargv[k],
						need, NULL, NULL);
				else if (nargv[k] != NULL)
					nargv[k][0] = '\0';
			}
			argc = (DWORD)nargs;
			argv = nargv;
		}
	}
	LocalFree(wargv);
	/* RUNNING before the loop: the loop blocks for the
	 * service lifetime, and the SCM kills a service that
	 * never leaves START_PENDING
	 */
	svc_report(SERVICE_RUNNING, 0);
	g_svc_ret = (DWORD)retraced_pipe_main((int)argc, argv);
	svc_report(SERVICE_STOPPED, 0);
	if (nargv != NULL) {
		for (k = 0; k < nargs; k++) {
			if (nargv[k] != NULL)
				HeapFree(GetProcessHeap(), 0, nargv[k]);
		}
		HeapFree(GetProcessHeap(), 0, nargv);
	}
}

int retraced_pipe_main(int argc, char **argv)
{
	const char *pipe_name = "\\\\.\\pipe\\retraced-agent";
	const char *ctl_name = "\\\\.\\pipe\\retraced-ctl";
	const char *journal_path = "retraced-journal.jsonl";
	const char *policy_path = NULL;
	const char *nonce_arg = NULL;
	const char *nonce_file = NULL;
	int exit_after_s = 0;
	long deadline;
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
		else if (strcmp(argv[i], "--exit-after") == 0 &&
			 i + 1 < argc)
			exit_after_s = atoi(argv[++i]);
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
	{
		const char *tr = getenv("RETRACED_TRACE");

		g_trace = tr != NULL && tr[0] == '1';
	}
	SetConsoleCtrlHandler(on_console_ctrl, TRUE);
	/* the harness guard: an orphaned test/CI daemon self-ends
	 * (deadline checked in the accept loop -- pump-free)
	 */
	deadline = exit_after_s > 0 ?
		now_ms() + (long)exit_after_s * 1000 : 0;

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
	g_agent_pipe_for_spawn = pipe_name;
	g_ctl.spawn_cb = ctl_spawn_win;
	g_ctl.conns = g_ctl_conns;
	g_ctl.conn_send = ctl_conn_send_pipe;
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
	g_ctl_name = ctl_name;
	g_ctl_thread = CreateThread(NULL, 0, ctl_thread,
		NULL, 0, NULL);

	printf("retraced: listening on %s (agents)\n", pipe_name);
	{
		long last_sweep = now_ms();
		long deadline_local = deadline;
		HANDLE evt = CreateEvent(NULL, TRUE, FALSE, NULL);
		HANDLE h = make_pipe(pipe_name);
		OVERLAPPED ov;

		memset(&ov, 0, sizeof(ov));
		ov.hEvent = evt;
		arm_accept(h, &ov);
		while (!g_stop &&
		       (deadline_local == 0 ||
			now_ms() < deadline_local)) {
			/*
			 * The sweep check runs BEFORE the wait: a busy
			 * accept (client churn) must not starve the reap
			 * cadence (round 4's missing exit record).
			 */
			if (g_trace)
				trace_event("iter");
			if (now_ms() - last_sweep > SWEEP_INTERVAL_MS) {
				if (g_trace)
					trace_event("sweep-pass");
				EnterCriticalSection(&g_lock);
				emit_drift_summaries();
				retraced_registry_sweep(&g_reg, now_ms(),
					15000);
				win_reap_sweep();
				LeaveCriticalSection(&g_lock);
				if (g_trace) {
					int kids = 0;
					int k;
					char line[80];

					for (k = 0; k < WIN_CHILDREN_MAX;
					     k++)
						if (g_win_children[k].h !=
						    NULL)
							kids++;
					snprintf(line, sizeof(line),
						"sweep agents=%zu kids=%d",
						g_reg.count, kids);
					trace_event(line);
				}
				last_sweep = now_ms();
			}
			/*
			 * ONE pending accept at a time (classic pattern):
			 * the event fires when a client lands; the 250ms
			 * cadence lets sweeps and stop run regardless.
			 */
			if (WaitForSingleObject(evt, 250) == WAIT_OBJECT_0) {
				int slot = -1;
				int k;

				trace_event("connect");

				for (k = 0; k < PIPE_AGENTS_MAX; k++) {
					if (!conns[k].live) {
						slot = k;
						break;
					}
				}
				if (slot >= 0) {
					HANDLE th;
					int cs;

					conns[slot].pipe = h;
					conns[slot].live = 1;
					conns[slot].helloed = 0;
					conns[slot].spectator = 0;
					conns[slot].ctl = NULL;
					/* the broadcast
					 * registration: policy
					 * reaches pipe agents
					 */
					EnterCriticalSection(&g_lock);
					for (cs = 0; cs < MAX_AGENTS;
					     cs++) {
						if (g_ctl_conns[cs].io == NULL) {
							g_ctl_conns[cs].io = (void *)h;
							g_ctl_conns[cs].helloed = 0;
							g_ctl_conns[cs].spectator = 0;
							g_ctl_conns[cs].agent_id[0] = '\0';
							conns[slot].ctl = &g_ctl_conns[cs];
							break;
						}
					}
					LeaveCriticalSection(&g_lock);
					trace_event("registered");
					th = CreateThread(NULL, 0, agent_thread,
						&conns[slot], 0, NULL);
					if (th != NULL)
						CloseHandle(th);
					trace_event("thread");
				} else {
					/* full: drop the connection */
					DisconnectNamedPipe(h);
					CloseHandle(h);
				}
				h = make_pipe(pipe_name);
				trace_event("remade");
				ResetEvent(evt);
				memset(&ov, 0, sizeof(ov));
				ov.hEvent = evt;
				arm_accept(h, &ov);
				trace_event("rearmed");
				continue;
			}
		}
		DisconnectNamedPipe(h);
		CloseHandle(h);
	}
	{
		char line[48];

		snprintf(line, sizeof(line),
			"loop-done stop=%d", (int)g_stop);
		trace_event(line);
	}

	/* graceful stop: flush the routine tail + the final reap
	 * (a fast-exiting workload's record must not wait on a
	 * sweep that will never come)
	 */
	EnterCriticalSection(&g_lock);
	emit_drift_summaries();
	win_reap_sweep();
	LeaveCriticalSection(&g_lock);
	retraced_journal_close(&g_jr);
	DeleteCriticalSection(&g_lock);
	return 0;
}

int main(int argc, char **argv)
{
	SERVICE_TABLE_ENTRYA table[2];

	table[0].lpServiceName = "";
	table[0].lpServiceProc = service_main;
	table[1].lpServiceName = NULL;
	table[1].lpServiceProc = NULL;
	if (StartServiceCtrlDispatcherA(table))
		return (int)g_svc_ret;
	return retraced_pipe_main(argc, argv);
}

#endif /* _WIN32 */
