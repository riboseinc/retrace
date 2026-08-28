/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

#include "policy_sig.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parson.h"

#ifdef RETRACE_HAVE_OPENSSL
#include <openssl/evp.h>
#include <openssl/pem.h>
#endif

#define SIG_B64_MAX 128

#define REASON(reason) \
	do { \
		if (reason_out != NULL && reason_cap > 0) \
			snprintf(reason_out, reason_cap, "%s", reason); \
	} while (0)

#ifdef RETRACE_HAVE_OPENSSL
static EVP_PKEY *g_key;
#endif
static int g_pinned;

static void copy_str_field(JSON_Object *root, const char *key,
	char *dst, size_t cap)
{
	const char *v = json_object_get_string(root, key);

	dst[0] = '\0';
	if (v != NULL)
		snprintf(dst, cap, "%s", v);
}

/*
 * The pinned key from RETRACE_SUPERVISOR_PUBKEY: a PEM file path
 * or an inline PEM with literal \n escapes.
 */
#ifdef RETRACE_HAVE_OPENSSL
static EVP_PKEY *load_pubkey(const char *spec)
{
	FILE *f = fopen(spec, "r");
	EVP_PKEY *k = NULL;

	if (f != NULL) {
		k = PEM_read_PUBKEY(f, NULL, NULL, NULL);
		fclose(f);
		return k;
	}
	{
		char buf[4096];
		size_t o = 0;

		for (; *spec != '\0' && o + 1 < sizeof(buf); spec++) {
			if (spec[0] == '\\' && spec[1] == 'n') {
				buf[o++] = '\n';
				spec++;
			} else {
				buf[o++] = *spec;
			}
		}
		buf[o] = '\0';
		{
			BIO *b = BIO_new_mem_buf(buf, (int)o);

			if (b != NULL) {
				k = PEM_read_bio_PUBKEY(b, NULL, NULL,
					NULL);
				BIO_free(b);
			}
		}
	}
	return k;
}

/* Ed25519 verify: base64 (canonical quartets) over the message */
static int verify_b64(const char *sig_b64, const char *msg)
{
	unsigned char raw[SIG_B64_MAX];
	size_t blen = strlen(sig_b64);
	size_t pad = 0;
	int declen;
	EVP_MD_CTX *ctx;
	int ok = 0;

	if (blen == 0 || blen % 4 != 0 || blen >= sizeof(raw) + 8)
		return 0;
	while (pad < 2 && sig_b64[blen - 1 - pad] == '=')
		pad++;
	declen = EVP_DecodeBlock(raw, (const unsigned char *)sig_b64,
		(int)blen);
	if (declen < 0)
		return 0;
	declen -= (int)pad;	/* EVP_DecodeBlock decodes '=' as NULs */
	ctx = EVP_MD_CTX_new();
	if (ctx != NULL) {
		if (EVP_DigestVerifyInit(ctx, NULL, NULL, NULL,
			    g_key) == 1 &&
		    EVP_DigestVerify(ctx, raw, (size_t)declen,
			    (const unsigned char *)msg,
			    strlen(msg)) == 1)
			ok = 1;
		EVP_MD_CTX_free(ctx);
	}
	return ok;
}
#endif

int retrace_policy_sig_init(void)
{
	if (g_pinned)
		return 0;
#ifdef RETRACE_HAVE_OPENSSL
	{
		const char *spec = getenv("RETRACE_SUPERVISOR_PUBKEY");

		if (spec == NULL || spec[0] == '\0')
			return 0;
		g_key = load_pubkey(spec);
		if (g_key == NULL)
			return -1;
		g_pinned = 1;
	}
#else
	/*
	 * No OpenSSL in the build: the pin still arms fail-closed
	 * for WRAPPED policies (they cannot be verified); bare
	 * policies keep working (the local PEERCRED+nonce model).
	 */
	if (getenv("RETRACE_SUPERVISOR_PUBKEY") != NULL)
		g_pinned = 1;
#endif
	return 0;
}

int retrace_policy_sig_pinned(void)
{
	return g_pinned;
}

int retrace_policy_sig_check(const char *payload, char *blob_out,
	size_t blob_cap, char *reason_out, size_t reason_cap)
{
	JSON_Value *v;
	JSON_Object *root, *sig;
	const char *blob;
	int rc = -1;

	if (reason_out != NULL && reason_cap > 0)
		reason_out[0] = '\0';

	v = json_parse_string(payload);
	if (v == NULL) {
		REASON("malformed json");
		return -1;
	}
	root = json_value_get_object(v);
	sig = root != NULL ? json_object_get_object(root, "sig") : NULL;
	blob = root != NULL ? json_object_get_string(root, "blob")
		: NULL;

	if (sig == NULL && blob == NULL) {
		/* bare policy: the caller applies payload as-is */
		snprintf(blob_out, blob_cap, "%s", payload);
		json_value_free(v);
		return 0;
	}
	if (sig == NULL || blob == NULL) {
		REASON("partial signature wrapper");
		goto out;
	}

#ifdef RETRACE_HAVE_OPENSSL
	{
		char alg[32], sig_b64[SIG_B64_MAX];

		copy_str_field(sig, "alg", alg, sizeof(alg));
		copy_str_field(sig, "sig", sig_b64, sizeof(sig_b64));
		if (strcmp(alg, "ed25519") != 0 || sig_b64[0] == '\0') {
			REASON("unsupported signature");
			goto out;
		}
		if (!g_pinned) {
			/* nobody armed verification: pass through */
			snprintf(blob_out, blob_cap, "%s", blob);
			json_value_free(v);
			return 1;
		}
		if (!verify_b64(sig_b64, blob)) {
			REASON("signature invalid");
			goto out;
		}
		snprintf(blob_out, blob_cap, "%s", blob);
		json_value_free(v);
		return 1;
	}
#else
	(void)blob_out;
	(void)blob_cap;
	REASON("build cannot verify signatures");
	goto out;
#endif
out:
	json_value_free(v);
	return rc;
}
