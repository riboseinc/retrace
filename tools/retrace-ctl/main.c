/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * retrace-ctl -- the fleet CLI (TODO.supervisor/07, P0).
 *
 * One command per invocation against a local retraced's
 * controller socket (newline-JSON both ways). JSON out by
 * default; pipe through jq for humans, -t tables land in P1.
 *
 * Exit codes: 0 ok, 1 daemon reported an error, 2 usage/conn.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netdb.h>
#endif

#include "../retraced/tls_gate.h"
#include "../retraced/journal_sign.h"
#include "../retraced/journal.h"
#include "journal_query.h"
#include "journal_query.h"
#include "../retraced/ctl_verbs.h"

#ifdef RETRACE_HAVE_OPENSSL
#include <openssl/evp.h>
#include <openssl/pem.h>
#endif

#ifdef _WIN32
#define CTL_DEFAULT "\\\\.\\pipe\\retraced-ctl"
#else
#define CTL_DEFAULT "/tmp/retraced.ctl.sock"
#endif

/* JSON string escape (policy blobs are JSON-in-JSON); caller frees */
static char *jesc(const char *s)
{
	size_t n = 0, o = 0;
	const char *p;
	char *out;

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

static char *read_file(const char *path)
{
	FILE *f = fopen(path, "rb");
	long sz;
	char *buf;

	if (f == NULL)
		return NULL;
	fseek(f, 0, SEEK_END);
	sz = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (sz <= 0 || sz > 60000) {
		fclose(f);
		return NULL;
	}
	buf = malloc((size_t)sz + 1);
	if (buf == NULL) {
		fclose(f);
		return NULL;
	}
	if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
		free(buf);
		fclose(f);
		return NULL;
	}
	fclose(f);
	buf[sz] = '\0';
	return buf;
}

static int print_reply(const char *reply)
{
	fputs(reply, stdout);
	if (strstr(reply, "\"ok\":1") == NULL &&
	    strstr(reply, "\"ok\": true") == NULL)
		return 1;
	return 0;
}

#ifdef _WIN32
/* one round trip over the ctl named pipe (supervisor/12): the
 * same newline-JSON line protocol the UDS ctl speaks.
 */
static int ctl_roundtrip_pipe(const char *pipe_name,
	const char *request)
{
	char reply[8192];
	HANDLE h = CreateFileA(pipe_name, GENERIC_READ | GENERIC_WRITE,
		0, NULL, OPEN_EXISTING, 0, NULL);
	DWORD put = 0, got = 0;

	if (h == INVALID_HANDLE_VALUE) {
		fprintf(stderr, "retrace-ctl: cannot reach %s "
			"(is retraced running with --ctl?)\n", pipe_name);
		return 2;
	}
	if (!WriteFile(h, request, (DWORD)strlen(request), &put,
		    NULL) || put == 0) {
		CloseHandle(h);
		return 2;
	}
	if (!ReadFile(h, reply, sizeof(reply) - 1, &got, NULL) ||
	    got == 0) {
		CloseHandle(h);
		fprintf(stderr, "retrace-ctl: no reply\n");
		return 2;
	}
	CloseHandle(h);
	reply[got] = '\0';
	fputs(reply, stdout);
	if (strstr(reply, "\"ok\":1") == NULL &&
	    strstr(reply, "\"ok\": true") == NULL)
		return 1;
	return 0;
}
#endif

#ifndef _WIN32
/* one round trip over local UDS */
static int ctl_roundtrip_uds(const char *sock_path, const char *request)
{
	struct sockaddr_un sa;
	char reply[8192];
	int fd = socket(AF_UNIX, SOCK_STREAM, 0);
	ssize_t n;

	if (fd < 0) {
		perror("socket");
		return 2;
	}
	memset(&sa, 0, sizeof(sa));
	sa.sun_family = AF_UNIX;
	snprintf(sa.sun_path, sizeof(sa.sun_path), "%s", sock_path);
	if (connect(fd, (struct sockaddr *)&sa, sizeof(sa)) != 0) {
		fprintf(stderr, "retrace-ctl: cannot reach %s "
			"(is retraced running with --ctl?)\n", sock_path);
		close(fd);
		return 2;
	}
	if (write(fd, request, strlen(request)) < 0) {
		perror("write");
		close(fd);
		return 2;
	}
	n = read(fd, reply, sizeof(reply) - 1);
	close(fd);
	if (n <= 0) {
		fprintf(stderr, "retrace-ctl: no reply\n");
		return 2;
	}
	reply[n] = '\0';
	return print_reply(reply);
}

/* one round trip over TLS 1.3 mTLS (TODO.supervisor/08 P1) */
static int ctl_roundtrip_tls(const char *hostport, const char *cert,
	const char *key, const char *ca, const char *request)
{
	struct retraced_tls_ctx *ctx;
	struct retraced_tls_peer peer;
	struct addrinfo hints, *res = NULL, *rp;
	char host[128], port[16];
	const char *colon;
	void *ssl = NULL;
	char reply[8192];
	int fd = -1, n, rc = 2;

	if (!retraced_tls_available()) {
		fprintf(stderr, "retrace-ctl: TLS requested but OpenSSL "
			"was not linked\n");
		return 2;
	}
	colon = strrchr(hostport, ':');
	if (colon == NULL) {
		fprintf(stderr, "retrace-ctl: --tls-host wants host:port\n");
		return 2;
	}
	{
		size_t hn = (size_t)(colon - hostport);

		if (hn >= sizeof(host))
			return 2;
		memcpy(host, hostport, hn);
		host[hn] = '\0';
		snprintf(port, sizeof(port), "%s", colon + 1);
	}
	ctx = retraced_tls_client_new(cert, key, ca);
	if (ctx == NULL) {
		fprintf(stderr, "retrace-ctl: TLS client context failed\n");
		return 2;
	}
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	if (getaddrinfo(host, port, &hints, &res) != 0) {
		retraced_tls_free(ctx);
		fprintf(stderr, "retrace-ctl: resolve %s failed\n", hostport);
		return 2;
	}
	for (rp = res; rp != NULL; rp = rp->ai_next) {
		fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (fd < 0)
			continue;
		if (connect(fd, rp->ai_addr, rp->ai_addrlen) == 0)
			break;
		close(fd);
		fd = -1;
	}
	freeaddrinfo(res);
	if (fd < 0) {
		retraced_tls_free(ctx);
		fprintf(stderr, "retrace-ctl: connect %s failed\n", hostport);
		return 2;
	}
	ssl = retraced_tls_connect(ctx, fd, &peer);
	if (ssl == NULL) {
		fprintf(stderr, "retrace-ctl: TLS handshake failed "
			"(mutual auth / claim scopes)\n");
		close(fd);
		retraced_tls_free(ctx);
		return 2;
	}
	if (retraced_tls_write(ssl, request, (int)strlen(request)) <= 0) {
		fprintf(stderr, "retrace-ctl: TLS write failed\n");
		goto out;
	}
	n = retraced_tls_read(ssl, reply, (int)sizeof(reply) - 1);
	if (n <= 0) {
		fprintf(stderr, "retrace-ctl: no TLS reply\n");
		goto out;
	}
	reply[n] = '\0';
	rc = print_reply(reply);
out:
	retraced_tls_ssl_free(ssl);
	close(fd);
	retraced_tls_free(ctx);
	return rc;
}
#endif /* !_WIN32 */

/* sign-policy: wrapper {sig:{alg,key_id,sig},blob} over the file
 * bytes with Ed25519; the blob string is the exact file text.
 */
static int cmd_sign_policy(const char *file, const char *key_path)
{
#ifdef RETRACE_HAVE_OPENSSL
	char *blob = read_file(file);
	FILE *kf;
	EVP_PKEY *key;
	EVP_MD_CTX *ctx;
	unsigned char sig[128];
	size_t siglen = sizeof(sig);
	char enc[256];
	char *esc;
	int n;

	if (blob == NULL) {
		fprintf(stderr, "retrace-ctl: cannot read %s\n", file);
		return 2;
	}
	kf = fopen(key_path, "r");
	if (kf == NULL) {
		fprintf(stderr, "retrace-ctl: cannot read %s\n", key_path);
		return 2;
	}
	key = PEM_read_PrivateKey(kf, NULL, NULL, NULL);
	fclose(kf);
	if (key == NULL) {
		fprintf(stderr, "retrace-ctl: bad key %s\n", key_path);
		return 2;
	}
	ctx = EVP_MD_CTX_new();
	if (ctx == NULL ||
	    EVP_DigestSignInit(ctx, NULL, NULL, NULL, key) != 1 ||
	    EVP_DigestSign(ctx, sig, &siglen,
		    (const unsigned char *)blob, strlen(blob)) != 1) {
		fprintf(stderr, "retrace-ctl: sign failed\n");
		return 2;
	}
	EVP_MD_CTX_free(ctx);
	EVP_EncodeBlock((unsigned char *)enc, sig, (int)siglen);
	esc = jesc(blob);
	free(blob);
	if (esc == NULL)
		return 2;
	n = printf("{\"sig\":{\"alg\":\"ed25519\",\"key_id\":\"%02x%02x\",\"sig\":\"%s\"},\"blob\":\"%s\"}\n",
		sig[0], sig[1], enc, esc);
	free(esc);
	if (n <= 0)
		return 2;
	return 0;
#else
	(void)file;
	(void)key_path;
	fprintf(stderr,
		"retrace-ctl: sign-policy needs an OpenSSL build\n");
	return 2;
#endif
}

/* verify-journal (TODO.impl/07): local, like sign-policy -- the
 * auditor's hand: chain + ed25519 seal in one verdict
 */
/* events --journal BASE [--query EXPR] (TODO.impl/10): the
 * offline read arm -- stream matching records from the segment
 * series, chain-verified on the way through
 */
static void event_sink(const char *line, void *user)
{
	(void) user;
	fputs(line, stdout);
}

static int cmd_events_local(const char *base, const char *query)
{
	struct retrace_filter_ast *ast = NULL;
	long n;

	if (query != NULL) {
		char err[128];

		ast = retrace_filter_compile(query, err, sizeof(err));
		if (ast == NULL) {
			fprintf(stderr, "retrace-ctl: bad query: %s\n",
				err);
			return 2;
		}
	}

	n = retraced_journal_query(base, ast, event_sink, NULL);
	retrace_filter_free(ast);
	if (n < 0) {
		fprintf(stderr,
			"retrace-ctl: journal chain broken or unreadable\n");
		return 1;
	}
	fprintf(stderr, "%ld record%s\n", n, n == 1 ? "" : "s");
	return 0;
}

/* the declared genesis link of a segment: its first line's
 * "prev" (what the segment says it continues from)
 */
static uint64_t segment_genesis_prev(const char *path)
{
	FILE *f = fopen(path, "r");
	char line[2048];
	const char *p;

	if (f == NULL)
		return 0;
	if (fgets(line, sizeof(line), f) == NULL) {
		fclose(f);
		return 0;
	}
	fclose(f);
	p = strstr(line, "\"prev\":\"");
	if (p == NULL)
		return 0;
	return (uint64_t) strtoull(p + 8, NULL, 16);
}

static int verify_one(const char *path, const char *pubkey_path,
	int *rc)
{
	char verdict[192];

	/* without a pubkey: the chain walk verdict only (unsigned
	 * journals are legitimate; the seal path needs a key)
	 */
	if (pubkey_path == NULL) {
		uint64_t head;
		size_t lines;
		size_t broken;

		if (retraced_journal_chain_file(path, 0, &head, &lines,
			    &broken) != 0) {
			fprintf(stderr,
				"retrace-ctl: %s: chain broken at line %zu\n",
				path, broken);
			*rc = 1;
			return 1;
		}
		printf("%s: chain ok, %zu lines, head %016llx\n",
			path, lines, (unsigned long long) head);
		return 1;
	}

	if (retraced_journal_verify_file(path, pubkey_path, verdict,
		    sizeof(verdict)) != 0) {
		fprintf(stderr, "retrace-ctl: %s\n", verdict);
		*rc = 1;
		return 1;
	}
	printf("%s\n", verdict);
	return 1;
}

static int cmd_verify_journal(const char *file,
	const char *pubkey_path)
{
	int rc = 0;
	int lo, hi, seq;
	uint64_t carry = 0;
	int have_carry = 0;

	/* the segment series (TODO.impl/10): each segment's chain
	 * verifies INTERNALLY from its own declared genesis link;
	 * the declared link is then checked against the
	 * predecessor's final head -- a match is the live
	 * cross-segment chain, a mismatch on the OLDEST surviving
	 * segment is retention (a chained prune record lives in
	 * the successor), a mismatch mid-series is corruption.
	 */
	if (retraced_journal_segment_range(file, &lo, &hi) == 0) {
		int segments = 0;

		for (seq = lo; seq <= hi; seq++) {
			char p[600];
			uint64_t declared;
			uint64_t head;
			size_t lines, broken;

			retraced_journal_segment_path(file, seq, p,
				sizeof(p));
			if (!retraced_journal_segment_exists(p, NULL))
				continue;

			declared = segment_genesis_prev(p);
			if (retraced_journal_chain_file_from(p,
				    declared, 0, &head, &lines, &broken)
				    != 0) {
				fprintf(stderr,
					"retrace-ctl: %s: chain broken at line %zu\n",
					p, broken);
				rc = 1;
			}
			if (have_carry && declared != carry) {
				if (seq == lo) {
					printf("%s: chain ok, %zu lines"
					       " (pruned predecessor)\n",
						p, lines);
				} else {
					fprintf(stderr,
						"retrace-ctl: %s: genesis link mismatch (corruption)\n",
						p);
					rc = 1;
				}
			} else {
				printf("%s: chain ok, %zu lines, "
				       "head %016llx\n", p, lines,
					(unsigned long long) head);
			}
			carry = head;
			have_carry = 1;
			segments++;
		}

		if (segments > 1)
			printf("%d segments verified\n", segments);
		if (segments == 0) {
			fprintf(stderr,
				"retrace-ctl: no journal at %s\n", file);
			return 1;
		}
		return rc;
	}

	/* no numbered segments: the plain base file IS the journal */
	if (retraced_journal_segment_exists(file, NULL))
		return verify_one(file, pubkey_path, &rc) ? rc : 1;

	fprintf(stderr, "retrace-ctl: no journal at %s\n", file);
	return 1;
}

static int ctl_usage(void)
{
	/* the verb lines derive from the same SSOT list the
	 * daemon dispatches on (ctl_verbs.h) -- the usage can no
	 * longer drift behind the verb surface
	 */
#define USAGE_LINE(n, c, s, a, h)					      \
	fprintf(stderr, "  %-14s%-26s%s\n", #c, a, h);

	fprintf(stderr,
		"usage: retrace-ctl [--sock PATH] COMMAND\n"
		"       retrace-ctl --tls-host H:P --tls-cert C --tls-key K\n"
		"                   --tls-ca CA COMMAND\n");
	RETRACED_CTL_VERBS(USAGE_LINE)
#undef USAGE_LINE
	fprintf(stderr,
		"  sign-policy FILE KEY   emit a signed wrapper to stdout\n"
		"  verify-journal FILE [--pubkey PEM]  chain (+ ed25519 seal) verdict\n"
		"  events --journal BASE --query EXPR  offline journal query (filter DSL)\n"
		"  --tls-*: fleet mTLS (all four required together)\n");
	return 2;
}

int main(int argc, char **argv)
{
	const char *sock_path = CTL_DEFAULT;
	const char *tls_host = NULL;
	const char *tls_cert = NULL;
	const char *tls_key = NULL;
	const char *tls_ca = NULL;
	char req[64000];
	int i;

	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--sock") == 0 && i + 1 < argc)
			sock_path = argv[++i];
		else if (strcmp(argv[i], "--tls-host") == 0 && i + 1 < argc)
			tls_host = argv[++i];
		else if (strcmp(argv[i], "--tls-cert") == 0 && i + 1 < argc)
			tls_cert = argv[++i];
		else if (strcmp(argv[i], "--tls-key") == 0 && i + 1 < argc)
			tls_key = argv[++i];
		else if (strcmp(argv[i], "--tls-ca") == 0 && i + 1 < argc)
			tls_ca = argv[++i];
		else
			break;
	}
	if (i >= argc)
		return ctl_usage();
	{
		int tls_n = (tls_host != NULL) + (tls_cert != NULL) +
			(tls_key != NULL) + (tls_ca != NULL);

		if (tls_n != 0 && tls_n != 4) {
			fprintf(stderr,
				"retrace-ctl: --tls-host/cert/key/ca are all-or-nothing\n");
			return 2;
		}
	}

	if (strcmp(argv[i], "status") == 0) {
		snprintf(req, sizeof(req),
			"{\"cmd\":\"status\"}\n");
	} else if (strcmp(argv[i], "ps") == 0) {
		snprintf(req, sizeof(req), "{\"cmd\":\"ps\"}\n");
	} else if (strcmp(argv[i], "policy-push") == 0 &&
		   i + 1 < argc) {
		char *blob = read_file(argv[i + 1]);
		char *esc;
		int rc;

		if (blob == NULL) {
			fprintf(stderr, "retrace-ctl: cannot read %s\n",
				argv[i + 1]);
			return 2;
		}
		esc = jesc(blob);
		free(blob);
		if (esc == NULL)
			return 2;
		rc = snprintf(req, sizeof(req),
			"{\"cmd\":\"policy_push\",\"blob\":\"%s\"}\n", esc);
		free(esc);
		if (rc < 0 || (size_t)rc >= sizeof(req)) {
			fprintf(stderr, "retrace-ctl: policy too large\n");
			return 2;
		}
	} else if (strcmp(argv[i], "freeze") == 0) {
		snprintf(req, sizeof(req), "{\"cmd\":\"freeze\"}\n");
	} else if (strcmp(argv[i], "thaw") == 0) {
		snprintf(req, sizeof(req), "{\"cmd\":\"thaw\"}\n");
	} else if (strcmp(argv[i], "sign-policy") == 0 &&
		   i + 2 < argc) {
		return cmd_sign_policy(argv[i + 1], argv[i + 2]);
	} else if (strcmp(argv[i], "verify-journal") == 0 &&
		   i + 1 < argc) {
		const char *pub = NULL;
		int j;

		for (j = i + 2; j + 1 < argc; j += 2) {
			if (strcmp(argv[j], "--pubkey") == 0)
				pub = argv[j + 1];
		}
		/* --pubkey optional: without it the verdict is the
		 * chain walk only (the sealed variant needs a key)
		 */
		return cmd_verify_journal(argv[i + 1], pub);
	} else if (strcmp(argv[i], "sessions") == 0) {
		snprintf(req, sizeof(req), "{\"cmd\":\"sessions\"}\n");
	} else if (strcmp(argv[i], "spawn") == 0 && i + 1 < argc) {
		/* spawn --preload <path> -- argv0 argv1 ...
		 * everything after -- is the workload
		 */
		const char *preload = NULL;
		int j = i + 1;


		while (j < argc && strcmp(argv[j], "--preload") == 0 &&
		       j + 2 < argc) {
			preload = argv[j + 1];
			j += 2;
		}
		if (j >= argc || strcmp(argv[j], "--") != 0) {
			return ctl_usage();
		}
		{
			char blob[1024];
			size_t o = 0;
			int k;

			o += (size_t)snprintf(blob + o,
				sizeof(blob) - o,
				"{\"cmd\":\"spawn\",\"argv\":[");
			for (k = j + 1; k < argc && o < sizeof(blob) - 64;
			     k++) {
				/* json-escape conservative: the
				 * workload argv in these flows is
				 * plain paths
				 */
				o += (size_t)snprintf(blob + o,
					sizeof(blob) - o, "%s\"%s\"",
					k > j + 1 ? "," : "", argv[k]);
			}
			o += (size_t)snprintf(blob + o, sizeof(blob) - o,
				"]");
			if (preload != NULL)
				o += (size_t)snprintf(blob + o,
					sizeof(blob) - o,
					",\"preload\":\"%s\"", preload);
			if (o < sizeof(blob) - 2) {
				blob[o++] = '}';
				blob[o] = '\0';
			}
			snprintf(req, sizeof(req), "%s\n", blob);
		}
		i = argc;
	} else if (strcmp(argv[i], "drift") == 0) {
		snprintf(req, sizeof(req), "{\"cmd\":\"drift\"}\n");
	} else if (strcmp(argv[i], "events") == 0) {
		long last = 20;
		const char *qjournal = NULL;
		const char *query = NULL;

		while (i + 2 < argc) {
			if (strcmp(argv[i + 1], "--last") == 0) {
				last = strtol(argv[i + 2], NULL, 10);
				if (last <= 0)
					last = 20;
				if (last > 128)
					last = 128;
				i += 2;
			} else if (strcmp(argv[i + 1], "--journal") == 0) {
				qjournal = argv[i + 2];
				i += 2;
			} else if (strcmp(argv[i + 1], "--query") == 0) {
				query = argv[i + 2];
				i += 2;
			} else {
				break;
			}
		}

		/* the offline arm (TODO.impl/10): query the journal
		 * segments directly -- the same expression language
		 * the config's filters speak
		 */
		if (qjournal != NULL)
			return cmd_events_local(qjournal, query);
		snprintf(req, sizeof(req),
			"{\"cmd\":\"events\",\"last\":%ld}\n", last);
	} else if (strcmp(argv[i], "kill") == 0 && i + 1 < argc) {
		long pid = strtol(argv[i + 1], NULL, 10);

		if (pid <= 0)
			return ctl_usage();
		snprintf(req, sizeof(req),
			"{\"cmd\":\"kill\",\"pid\":%ld}\n", pid);
	} else {
		return ctl_usage();
	}
#ifdef _WIN32
	return ctl_roundtrip_pipe(sock_path, req);
#else
	if (tls_host != NULL)
		return ctl_roundtrip_tls(tls_host, tls_cert, tls_key,
			tls_ca, req);
	return ctl_roundtrip_uds(sock_path, req);
#endif
}
