/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

#include "artifact_audit.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifdef RETRACE_HAVE_OPENSSL
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/sha.h>
#endif

#define AUDIT_LINE_MAX 4200
#define AUDIT_SIG_B64_MAX 128	/* Ed25519 64B -> ~88 base64 */

#ifdef RETRACE_HAVE_OPENSSL
static EVP_PKEY *g_sign_key;
static EVP_PKEY *g_verify_key;
#endif

int enforce_audit_signing(void)
{
#ifdef RETRACE_HAVE_OPENSSL
	return g_sign_key != NULL;
#else
	return 0;
#endif
}

#ifdef RETRACE_HAVE_OPENSSL
/* sign prev_hex||payload; out holds base64 (no newline) */
static int audit_sign(const char *prev_hex, const char *payload,
	char out[AUDIT_SIG_B64_MAX])
{
	EVP_MD_CTX *ctx = NULL;
	unsigned char input[AUDIT_LINE_MAX];
	unsigned char sig[128];
	size_t inlen, siglen = sizeof(sig);
	size_t prevlen = strlen(prev_hex), plen = strlen(payload);
	int rc = -1;

	if (g_sign_key == NULL)
		return -1;
	if (prevlen + plen + 1 > sizeof(input))
		return -1;
	memcpy(input, prev_hex, prevlen);
	memcpy(input + prevlen, payload, plen + 1);
	inlen = prevlen + plen;
	ctx = EVP_MD_CTX_new();
	if (ctx == NULL)
		return -1;
	if (EVP_DigestSignInit(ctx, NULL, NULL, NULL, g_sign_key) == 1 &&
	    EVP_DigestSign(ctx, sig, &siglen, input, inlen) == 1) {
		EVP_EncodeBlock((unsigned char *)out, sig, (int)siglen);
		rc = 0;
	}
	EVP_MD_CTX_free(ctx);
	return rc;
}

static int audit_verify_sig(const char *prev_hex, const char *payload,
	const char *b64)
{
	EVP_MD_CTX *ctx = NULL;
	unsigned char input[AUDIT_LINE_MAX];
	unsigned char sig[128];
	unsigned char expected[AUDIT_SIG_B64_MAX];
	size_t prevlen = strlen(prev_hex), plen = strlen(payload);
	int rc = -1;

	if (g_verify_key == NULL || b64 == NULL)
		return -1;
	if (prevlen + plen + 1 > sizeof(input))
		return -1;
	memcpy(input, prev_hex, prevlen);
	memcpy(input + prevlen, payload, plen + 1);
	{
		size_t blen = strlen(b64);
		size_t pad = 0;
		int declen;

		if (blen == 0 || blen % 4 != 0 || blen >= sizeof(expected))
			return -1;	/* canonical base64 is quartets */
		while (pad < 2 && b64[blen - 1 - pad] == '=')
			pad++;
		declen = EVP_DecodeBlock(sig, (const unsigned char *)b64,
			(int)blen);
		if (declen < 0)
			return -1;
		/* EVP_DecodeBlock decodes '=' as NUL bytes: strip them */
		declen -= (int)pad;
		EVP_EncodeBlock(expected, sig, declen);
		if (strcmp((const char *)expected, b64) != 0)
			return -1;	/* non-canonical encoding */
		ctx = EVP_MD_CTX_new();
		if (ctx != NULL) {
			if (EVP_DigestVerifyInit(ctx, NULL, NULL, NULL,
				    g_verify_key) == 1 &&
			    EVP_DigestVerify(ctx, sig, (size_t)declen,
				    input, prevlen + plen) == 1)
				rc = 0;
			EVP_MD_CTX_free(ctx);
		}
	}
	return rc;
}
#endif

int enforce_audit_set_key(const char *priv_pem_path)
{
#ifdef RETRACE_HAVE_OPENSSL
	FILE *f = fopen(priv_pem_path, "r");

	if (f == NULL)
		return -1;
	g_sign_key = PEM_read_PrivateKey(f, NULL, NULL, NULL);
	fclose(f);
	return g_sign_key != NULL ? 0 : -1;
#else
	(void)priv_pem_path;
	return -1;
#endif
}

int enforce_audit_set_pubkey(const char *pub_pem_path)
{
#ifdef RETRACE_HAVE_OPENSSL
	FILE *f = fopen(pub_pem_path, "r");

	if (f == NULL)
		return -1;
	g_verify_key = PEM_read_PUBKEY(f, NULL, NULL, NULL);
	fclose(f);
	return g_verify_key != NULL ? 0 : -1;
#else
	(void)pub_pem_path;
	return -1;
#endif
}

/*
 * FNV-1a 64 -- the fallback digest when the build has no
 * OpenSSL (same discipline as the supervisor journal: tamper
 * EVIDENCE now, signatures with the policy-signing plan).
 */
static uint64_t fnv1a64(const char *s, uint64_t h)
{
	for (; *s != '\0'; s++) {
		h ^= (unsigned char)*s;
		h *= 0x01000193ULL;
	}
	return h;
}

static void hex32(const unsigned char *md, char out[ENFORCE_DIGEST_HEX_MAX])
{
	size_t i;

	for (i = 0; i < 32; i++)
		snprintf(out + i * 2, 3, "%02x", md[i]);
	out[64] = '\0';
}

int enforce_audit_hash(const char *prev_hex, const char *payload,
	char out[ENFORCE_DIGEST_HEX_MAX])
{
#ifdef RETRACE_HAVE_OPENSSL
	unsigned char md[32];
	char input[AUDIT_LINE_MAX];
	size_t plen = strlen(payload);
	size_t prevlen = strlen(prev_hex);

	if (prevlen + plen + 1 > sizeof(input))
		return -1;
	memcpy(input, prev_hex, prevlen);
	memcpy(input + prevlen, payload, plen + 1);
	if (SHA256((const unsigned char *)input, prevlen + plen, md)
	    == NULL)
		return -1;
	hex32(md, out);
	return 0;
#else
	uint64_t h = fnv1a64(payload,
		fnv1a64(prev_hex, 0xcbf29ce484222325ULL));

	snprintf(out, ENFORCE_DIGEST_HEX_MAX, "%016llx",
		(unsigned long long)h);
	return 0;
#endif
}

static const char *digest_alg(void)
{
#ifdef RETRACE_HAVE_OPENSSL
	return "sha256";
#else
	return "fnv1a64";
#endif
}

int enforce_spec_digest(const char *spec_json, size_t len,
	char out[ENFORCE_DIGEST_HEX_MAX], const char **alg_out)
{
	if (alg_out != NULL)
		*alg_out = digest_alg();
#ifdef RETRACE_HAVE_OPENSSL
	{
		unsigned char md[32];

		if (SHA256((const unsigned char *)spec_json, len, md)
		    == NULL)
			return -1;
		hex32(md, out);
	}
#else
	{
		uint64_t h = 0xcbf29ce484222325ULL;
		size_t i;

		for (i = 0; i < len; i++) {
			h ^= (unsigned char)spec_json[i];
			h *= 0x01000193ULL;
		}
		snprintf(out, ENFORCE_DIGEST_HEX_MAX, "%016llx",
			(unsigned long long)h);
	}
#endif
	return 0;
}

/* JSON string escape for argv elements (bounded) */
static int jesc(char *dst, size_t cap, const char *s)
{
	size_t o = 0;

	if (cap < 3)
		return -1;
	dst[o++] = '"';
	if (s != NULL) {
		for (; *s != '\0'; s++) {
			if (o + 7 >= cap)
				return -1;
			if (*s == '"' || *s == '\\') {
				dst[o++] = '\\';
				dst[o++] = *s;
			} else if ((unsigned char)*s < 0x20) {
				o += (size_t)snprintf(dst + o, 7,
					"\\u%04x",
					(unsigned int)(unsigned char)*s);
			} else {
				dst[o++] = *s;
			}
		}
	}
	dst[o++] = '"';
	dst[o] = '\0';
	return 0;
}

static void extract_hash(const char *line, char out[ENFORCE_DIGEST_HEX_MAX])
{
	const char *h = strstr(line, "{\"hash\":\"");
	size_t n = 0;

	if (h == NULL) {
		out[0] = '\0';
		return;
	}
	h += 9;
	while (h[n] != '\0' && h[n] != '"' &&
	       n + 1 < ENFORCE_DIGEST_HEX_MAX)
		n++;
	memcpy(out, h, n);
	out[n] = '\0';
}

static void extract_sig(const char *line, char out[AUDIT_SIG_B64_MAX])
{
	const char *h = strstr(line, ",\"sig\":\"");
	size_t n = 0;

	if (h == NULL) {
		out[0] = '\0';
		return;
	}
	h += 8;
	while (h[n] != '\0' && h[n] != '"' &&
	       n + 1 < AUDIT_SIG_B64_MAX)
		n++;
	memcpy(out, h, n);
	out[n] = '\0';
}

static long verify_chain(const char *path,
	char last_hash[ENFORCE_DIGEST_HEX_MAX])
{
	FILE *f = fopen(path, "r");
	char line[AUDIT_LINE_MAX];
	char prev[ENFORCE_DIGEST_HEX_MAX] = "0";
	long count = 0;

	if (last_hash != NULL)
		last_hash[0] = '\0';
	if (f == NULL)
		return 0;		/* no trail yet: zero records */
	while (fgets(line, sizeof(line), f) != NULL) {
		size_t len = strlen(line);
		const char *hp;
		char want[ENFORCE_DIGEST_HEX_MAX];
		char stored[ENFORCE_DIGEST_HEX_MAX];
		size_t plen;

		if (len == 0 || line[len - 1] != '\n')
			return -2;	/* torn tail */
		hp = strstr(line, "{\"hash\":\"");
		if (hp == NULL || hp == line)
			return -2;	/* no payload or no hash */
		plen = (size_t)(hp - line);
		if (plen >= sizeof(line))	/* keeps gcc quiet */
			return -2;
		{
			char payload[AUDIT_LINE_MAX];

			memcpy(payload, line, plen);
			payload[plen] = '\0';
			extract_hash(line, stored);
			if (stored[0] == '\0' ||
			    enforce_audit_hash(prev, payload, want) != 0 ||
			    strcmp(want, stored) != 0)
				return -2;
#ifdef RETRACE_HAVE_OPENSSL
			if (g_verify_key != NULL) {
				char sig[AUDIT_SIG_B64_MAX];

				extract_sig(line, sig);
				if (sig[0] == '\0' ||
				    audit_verify_sig(prev, payload, sig) != 0)
					return -2;
			}
#endif
		}
		snprintf(prev, sizeof(prev), "%s", stored);
		count++;
	}
	fclose(f);
	if (last_hash != NULL)
		snprintf(last_hash, ENFORCE_DIGEST_HEX_MAX, "%s", prev);
	return count;
}

int enforce_audit_append(const char *path, long ts, long pid,
	const char digest[ENFORCE_DIGEST_HEX_MAX], const char *alg,
	const char *backends, char *const argv[])
{
	char prev[ENFORCE_DIGEST_HEX_MAX];
	char hash[ENFORCE_DIGEST_HEX_MAX];
	char payload[2048];
	char av[1024];
	char line[AUDIT_LINE_MAX];
	char tail[ENFORCE_DIGEST_HEX_MAX] = "";
	FILE *f;
	int i;

	/* fail-closed: a broken existing trail refuses new records;
	 * the same replay yields the chain head
	 */
	{
		long rc = verify_chain(path, tail);

		if (rc == -2) {
			fprintf(stderr,
				"retrace-enforce: audit trail tampered; refusing\n");
			return -1;
		}
		if (rc < 0)
			return -1;
	}

	av[0] = '\0';
	for (i = 0; argv != NULL && argv[i] != NULL; i++) {
		char one[512];

		if (jesc(one, sizeof(one), argv[i]) != 0)
			return -1;
		if (av[0] != '\0')
			strncat(av, ",", sizeof(av) - strlen(av) - 1);
		strncat(av, one, sizeof(av) - strlen(av) - 1);
	}
	{
		int n = snprintf(payload, sizeof(payload),
			"{\"ts\":%ld,\"pid\":%ld,\"digest\":\"%s\","
			"\"alg\":\"%s\",\"backends\":\"%s\","
			"\"argv\":[%s]}",
			ts, pid, digest, alg != NULL ? alg : digest_alg(),
			backends != NULL ? backends : "", av);

		if (n <= 0 || (size_t)n >= sizeof(payload))
			return -1;
	}

	/* chain head: the last record's hash, "0" when fresh */
	if (tail[0] != '\0')
		snprintf(prev, sizeof(prev), "%s", tail);
	else
		snprintf(prev, sizeof(prev), "0");
	if (enforce_audit_hash(prev, payload, hash) != 0)
		return -1;
#ifdef RETRACE_HAVE_OPENSSL
	if (g_sign_key != NULL) {
		char sig[AUDIT_SIG_B64_MAX];
		int n;

		if (audit_sign(prev, payload, sig) != 0)
			return -1;
		n = snprintf(line, sizeof(line),
			"%s{\"hash\":\"%s\",\"sig\":\"%s\"}\n",
			payload, hash, sig);
		if (n <= 0 || (size_t)n >= sizeof(line))
			return -1;
	} else
#endif
	{
		int n = snprintf(line, sizeof(line), "%s{\"hash\":\"%s\"}\n",
			payload, hash);

		if (n <= 0 || (size_t)n >= sizeof(line))
			return -1;
	}
	f = fopen(path, "a");
	if (f == NULL)
		return -1;
	if (fputs(line, f) == EOF || fflush(f) != 0) {
		fclose(f);
		return -1;
	}
	fclose(f);
	return 0;
}

long enforce_audit_verify(const char *path,
	char last_hash[ENFORCE_DIGEST_HEX_MAX])
{
	return verify_chain(path, last_hash);
}
