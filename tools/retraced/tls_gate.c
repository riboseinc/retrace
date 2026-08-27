/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

#include "tls_gate.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef RETRACED_HAVE_OPENSSL
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#endif

uint32_t retraced_tls_parse_scopes(const char *csv)
{
	uint32_t s = 0;
	const char *p;
	char tok[32];
	size_t n;

	if (csv == NULL || *csv == '\0')
		return 0;
	p = csv;
	while (*p != '\0') {
		n = 0;
		while (*p != '\0' && *p != ',' && *p != '+' && *p != ' ' &&
		       n + 1 < sizeof(tok))
			tok[n++] = *p++;
		tok[n] = '\0';
		while (*p == ',' || *p == '+' || *p == ' ')
			p++;
		if (strcmp(tok, "status") == 0)
			s |= RETRACED_SCOPE_STATUS;
		else if (strcmp(tok, "ps") == 0)
			s |= RETRACED_SCOPE_PS;
		else if (strcmp(tok, "policy") == 0 ||
			 strcmp(tok, "policy_push") == 0 ||
			 strcmp(tok, "freeze") == 0 ||
			 strcmp(tok, "thaw") == 0)
			s |= RETRACED_SCOPE_POLICY;
		else if (strcmp(tok, "kill") == 0)
			s |= RETRACED_SCOPE_KILL;
		else if (strcmp(tok, "spawn") == 0)
			s |= RETRACED_SCOPE_SPAWN;
		else if (strcmp(tok, "all") == 0)
			s |= RETRACED_SCOPE_ALL;
	}
	return s;
}

uint32_t retraced_tls_scope_for_cmd(const char *cmd)
{
	if (cmd == NULL)
		return 0;
	if (strcmp(cmd, "status") == 0)
		return RETRACED_SCOPE_STATUS;
	if (strcmp(cmd, "ps") == 0)
		return RETRACED_SCOPE_PS;
	if (strcmp(cmd, "policy_push") == 0 ||
	    strcmp(cmd, "freeze") == 0 ||
	    strcmp(cmd, "thaw") == 0)
		return RETRACED_SCOPE_POLICY;
	if (strcmp(cmd, "kill") == 0)
		return RETRACED_SCOPE_KILL;
	if (strcmp(cmd, "spawn") == 0)
		return RETRACED_SCOPE_SPAWN;
	return 0;
}

int retraced_tls_listen(const char *hostport)
{
	char host[128];
	char port[16];
	const char *colon;
	struct addrinfo hints, *res = NULL, *rp;
	int fd = -1, on = 1, rc;

	if (hostport == NULL || *hostport == '\0')
		return -1;
	colon = strrchr(hostport, ':');
	if (colon == NULL) {
		snprintf(host, sizeof(host), "%s", "127.0.0.1");
		snprintf(port, sizeof(port), "%s", hostport);
	} else {
		size_t hn = (size_t)(colon - hostport);

		if (hn >= sizeof(host) || colon[1] == '\0')
			return -1;
		memcpy(host, hostport, hn);
		host[hn] = '\0';
		if (host[0] == '\0')
			snprintf(host, sizeof(host), "%s", "127.0.0.1");
		snprintf(port, sizeof(port), "%s", colon + 1);
	}

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	rc = getaddrinfo(host[0] ? host : NULL, port, &hints, &res);
	if (rc != 0)
		return -1;
	for (rp = res; rp != NULL; rp = rp->ai_next) {
		fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (fd < 0)
			continue;
		setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
		if (bind(fd, rp->ai_addr, rp->ai_addrlen) == 0 &&
		    listen(fd, 4) == 0)
			break;
		close(fd);
		fd = -1;
	}
	freeaddrinfo(res);
	return fd;
}

#ifndef RETRACED_HAVE_OPENSSL

struct retraced_tls_ctx {
	int dummy;
};

int retraced_tls_available(void)
{
	return 0;
}

struct retraced_tls_ctx *retraced_tls_server_new(const char *cert,
	const char *key, const char *ca)
{
	(void)cert;
	(void)key;
	(void)ca;
	return NULL;
}

struct retraced_tls_ctx *retraced_tls_client_new(const char *cert,
	const char *key, const char *ca)
{
	(void)cert;
	(void)key;
	(void)ca;
	return NULL;
}

void retraced_tls_free(struct retraced_tls_ctx *ctx)
{
	(void)ctx;
}

void *retraced_tls_accept(struct retraced_tls_ctx *ctx, int fd,
	struct retraced_tls_peer *peer)
{
	(void)ctx;
	(void)fd;
	(void)peer;
	return NULL;
}

void *retraced_tls_connect(struct retraced_tls_ctx *ctx, int fd,
	struct retraced_tls_peer *peer)
{
	(void)ctx;
	(void)fd;
	(void)peer;
	return NULL;
}

void retraced_tls_ssl_free(void *ssl)
{
	(void)ssl;
}

int retraced_tls_read(void *ssl, void *buf, int n)
{
	(void)ssl;
	(void)buf;
	(void)n;
	return -1;
}

int retraced_tls_write(void *ssl, const void *buf, int n)
{
	(void)ssl;
	(void)buf;
	(void)n;
	return -1;
}

#else /* RETRACED_HAVE_OPENSSL */

struct retraced_tls_ctx {
	SSL_CTX *ctx;
	int is_server;
};

int retraced_tls_available(void)
{
	return 1;
}

static int load_identity(SSL_CTX *ctx, const char *cert,
	const char *key, const char *ca)
{
	if (SSL_CTX_use_certificate_chain_file(ctx, cert) != 1)
		return -1;
	if (SSL_CTX_use_PrivateKey_file(ctx, key, SSL_FILETYPE_PEM) != 1)
		return -1;
	if (SSL_CTX_check_private_key(ctx) != 1)
		return -1;
	if (SSL_CTX_load_verify_locations(ctx, ca, NULL) != 1)
		return -1;
	SSL_CTX_set_verify(ctx,
		SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, NULL);
	SSL_CTX_set_verify_depth(ctx, 2);
	/* TLS 1.3 only -- the doctrine: no plaintext, no legacy */
	if (SSL_CTX_set_min_proto_version(ctx, TLS1_3_VERSION) != 1)
		return -1;
	if (SSL_CTX_set_max_proto_version(ctx, TLS1_3_VERSION) != 1)
		return -1;
	return 0;
}

struct retraced_tls_ctx *retraced_tls_server_new(const char *cert,
	const char *key, const char *ca)
{
	struct retraced_tls_ctx *t;
	SSL_CTX *ctx;

	if (cert == NULL || key == NULL || ca == NULL)
		return NULL;
	t = calloc(1, sizeof(*t));
	if (t == NULL)
		return NULL;
	ctx = SSL_CTX_new(TLS_server_method());
	if (ctx == NULL) {
		free(t);
		return NULL;
	}
	if (load_identity(ctx, cert, key, ca) != 0) {
		SSL_CTX_free(ctx);
		free(t);
		return NULL;
	}
	t->ctx = ctx;
	t->is_server = 1;
	return t;
}

struct retraced_tls_ctx *retraced_tls_client_new(const char *cert,
	const char *key, const char *ca)
{
	struct retraced_tls_ctx *t;
	SSL_CTX *ctx;

	if (cert == NULL || key == NULL || ca == NULL)
		return NULL;
	t = calloc(1, sizeof(*t));
	if (t == NULL)
		return NULL;
	ctx = SSL_CTX_new(TLS_client_method());
	if (ctx == NULL) {
		free(t);
		return NULL;
	}
	if (load_identity(ctx, cert, key, ca) != 0) {
		SSL_CTX_free(ctx);
		free(t);
		return NULL;
	}
	t->ctx = ctx;
	t->is_server = 0;
	return t;
}

void retraced_tls_free(struct retraced_tls_ctx *ctx)
{
	if (ctx == NULL)
		return;
	if (ctx->ctx != NULL)
		SSL_CTX_free(ctx->ctx);
	free(ctx);
}

/*
 * Claim extraction: walk URI SANs looking for
 *   retrace:scope:<csv>
 * First match wins. Empty/missing = scopes 0 (caller refuses).
 */
static uint32_t peer_scopes_from_cert(X509 *cert, char *cn, size_t cn_cap)
{
	X509_NAME *subj;
	GENERAL_NAMES *sans;
	int i, n;
	uint32_t scopes = 0;

	if (cn != NULL && cn_cap > 0)
		cn[0] = '\0';
	if (cert == NULL)
		return 0;

	subj = X509_get_subject_name(cert);
	if (subj != NULL && cn != NULL && cn_cap > 0) {
		X509_NAME_get_text_by_NID(subj, NID_commonName, cn,
			(int)cn_cap);
	}

	sans = X509_get_ext_d2i(cert, NID_subject_alt_name, NULL, NULL);
	if (sans == NULL)
		return 0;
	n = sk_GENERAL_NAME_num(sans);
	for (i = 0; i < n; i++) {
		GENERAL_NAME *gn = sk_GENERAL_NAME_value(sans, i);
		ASN1_STRING *uri;
		const char *s;
		const char *prefix = "retrace:scope:";

		if (gn == NULL || gn->type != GEN_URI)
			continue;
		uri = gn->d.uniformResourceIdentifier;
		if (uri == NULL)
			continue;
		s = (const char *)ASN1_STRING_get0_data(uri);
		if (s == NULL)
			continue;
		if (strncmp(s, prefix, strlen(prefix)) != 0)
			continue;
		scopes = retraced_tls_parse_scopes(s + strlen(prefix));
		break;
	}
	GENERAL_NAMES_free(sans);
	return scopes;
}

static void *handshake(struct retraced_tls_ctx *ctx, int fd, int server,
	struct retraced_tls_peer *peer)
{
	SSL *ssl;
	X509 *cert;
	int rc;

	if (ctx == NULL || ctx->ctx == NULL)
		return NULL;
	ssl = SSL_new(ctx->ctx);
	if (ssl == NULL)
		return NULL;
	if (SSL_set_fd(ssl, fd) != 1) {
		SSL_free(ssl);
		return NULL;
	}
	rc = server ? SSL_accept(ssl) : SSL_connect(ssl);
	if (rc != 1) {
		SSL_free(ssl);
		return NULL;
	}
	cert = SSL_get_peer_certificate(ssl);
	if (cert == NULL) {
		SSL_free(ssl);
		return NULL;
	}
	if (peer != NULL) {
		memset(peer, 0, sizeof(*peer));
		peer->scopes = peer_scopes_from_cert(cert, peer->cn,
			sizeof(peer->cn));
	}
	X509_free(cert);
	/* fail-closed: a cert without claim scopes is refused */
	if (peer != NULL && peer->scopes == 0) {
		SSL_free(ssl);
		return NULL;
	}
	return ssl;
}

void *retraced_tls_accept(struct retraced_tls_ctx *ctx, int fd,
	struct retraced_tls_peer *peer)
{
	return handshake(ctx, fd, 1, peer);
}

void *retraced_tls_connect(struct retraced_tls_ctx *ctx, int fd,
	struct retraced_tls_peer *peer)
{
	return handshake(ctx, fd, 0, peer);
}

void retraced_tls_ssl_free(void *ssl)
{
	if (ssl != NULL)
		SSL_free((SSL *)ssl);
}

int retraced_tls_read(void *ssl, void *buf, int n)
{
	int r;

	if (ssl == NULL)
		return -1;
	r = SSL_read((SSL *)ssl, buf, n);
	return r;
}

int retraced_tls_write(void *ssl, const void *buf, int n)
{
	int r;

	if (ssl == NULL)
		return -1;
	r = SSL_write((SSL *)ssl, buf, n);
	return r;
}

#endif /* RETRACED_HAVE_OPENSSL */
