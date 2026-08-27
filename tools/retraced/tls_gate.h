/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * TLS fleet transport (TODO.supervisor/08 P1 / TODO.beyond-libc/05):
 * TLS 1.3 only, mutual auth, claim scopes from the peer cert.
 * No plaintext remote mode, ever. Local UDS ctl stays PEERCRED.
 *
 * Claim encoding (the peer cert URI SAN):
 *   URI:retrace:scope:status+ps+policy+kill
 * ('+' not ',' -- OpenSSL's SAN grammar splits on comma.)
 * Missing/empty URI SAN = refuse (fail-closed remote).
 */

#ifndef RETRACE_TOOLS_RETRACED_TLS_GATE_H_
#define RETRACE_TOOLS_RETRACED_TLS_GATE_H_

#include <stddef.h>
#include <stdint.h>

/* command scopes -- least privilege by construction */
#define RETRACED_SCOPE_STATUS  (1u << 0)  /* status */
#define RETRACED_SCOPE_PS      (1u << 1)  /* ps */
#define RETRACED_SCOPE_POLICY  (1u << 2)  /* policy_push, freeze, thaw */
#define RETRACED_SCOPE_KILL    (1u << 3)  /* kill */
#define RETRACED_SCOPE_SPAWN   (1u << 4)  /* spawn (future) */
#define RETRACED_SCOPE_ALL \
	(RETRACED_SCOPE_STATUS | RETRACED_SCOPE_PS | \
	 RETRACED_SCOPE_POLICY | RETRACED_SCOPE_KILL | \
	 RETRACED_SCOPE_SPAWN)

struct retraced_tls_peer {
	char cn[256];
	uint32_t scopes;
};

/* opaque; NULL when OpenSSL is unavailable or not configured */
struct retraced_tls_ctx;

/*
 * Build a server context. cert/key are the daemon's identity;
 * ca is the trust anchor that must sign controller certs.
 * Returns NULL on any load/config failure (caller must not
 * open a plaintext TCP fallback -- that is the doctrine).
 */
struct retraced_tls_ctx *retraced_tls_server_new(const char *cert,
	const char *key, const char *ca);

/*
 * Build a client context (retrace-ctl --tls). cert/key are the
 * controller identity; ca trusts the daemon.
 */
struct retraced_tls_ctx *retraced_tls_client_new(const char *cert,
	const char *key, const char *ca);

void retraced_tls_free(struct retraced_tls_ctx *ctx);

/* 1 if this build was linked with OpenSSL and ctx is live */
int retraced_tls_available(void);

/*
 * Accept + handshake on an already-accepted TCP fd. On success
 * returns an opaque SSL* handle (cast to void*) and fills peer;
 * the caller owns the fd and must close it after ssl_free.
 * On failure returns NULL (fd still open; caller closes).
 */
void *retraced_tls_accept(struct retraced_tls_ctx *ctx, int fd,
	struct retraced_tls_peer *peer);

void *retraced_tls_connect(struct retraced_tls_ctx *ctx, int fd,
	struct retraced_tls_peer *peer);

void retraced_tls_ssl_free(void *ssl);

/* BIO-style I/O over the SSL; returns bytes or <=0 on error/EOF */
int retraced_tls_read(void *ssl, void *buf, int n);
int retraced_tls_write(void *ssl, const void *buf, int n);

/* parse "status,ps,policy,kill" into a bitmask (test helper too) */
uint32_t retraced_tls_parse_scopes(const char *csv);

/* map a ctl cmd name onto the required scope bit; 0 = unknown */
uint32_t retraced_tls_scope_for_cmd(const char *cmd);

/* bind+listen a TCP socket on host:port (host may be NULL = any) */
int retraced_tls_listen(const char *hostport);

#endif /* RETRACE_TOOLS_RETRACED_TLS_GATE_H_ */
