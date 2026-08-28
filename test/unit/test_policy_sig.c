/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Signed POLICY_SET (TODO.supervisor/05): the wrapper format and
 * the fail-closed pin. The signature covers the blob STRING bytes
 * exactly (no canonicalization); with RETRACE_SUPERVISOR_PUBKEY
 * pinned, a wrapper without a valid Ed25519 signature is refused.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "policy_sig.h"

/* fixtures: the committed audit keypair (throwaway, tests only) */
#define PRIV RETRACE_SOURCE_DIR "/test/fixtures/audit_ed25519_key.pem"
#define PUB RETRACE_SOURCE_DIR "/test/fixtures/audit_ed25519_pub.pem"

#ifdef RETRACE_HAVE_OPENSSL
#include <openssl/evp.h>
#include <openssl/pem.h>

/* sign with the fixture key IN PROCESS: no openssl CLI (its
 * Ed25519 support varies -- LibreSSL on the mac runners lacks
 * pkeyutl Ed entirely), no shell, deterministic everywhere.
 */
static int sign_b64(const char *msg, char *out, size_t cap)
{
	FILE *kf = fopen(PRIV, "r");
	EVP_PKEY *key;
	EVP_MD_CTX *ctx;
	unsigned char sig[128];
	size_t siglen = sizeof(sig);
	char enc[256];

	if (kf == NULL)
		return -1;
	key = PEM_read_PrivateKey(kf, NULL, NULL, NULL);
	fclose(kf);
	if (key == NULL)
		return -1;
	ctx = EVP_MD_CTX_new();
	if (ctx == NULL ||
	    EVP_DigestSignInit(ctx, NULL, NULL, NULL, key) != 1 ||
	    EVP_DigestSign(ctx, sig, &siglen,
		    (const unsigned char *)msg, strlen(msg)) != 1)
		return -1;
	EVP_MD_CTX_free(ctx);
	EVP_EncodeBlock((unsigned char *)enc, sig, (int)siglen);
	if (strlen(enc) + 1 > cap)
		return -1;
	strcpy(out, enc);
	return 0;
}
#endif

static int tests_run;
static int tests_pass;
static int tests_fail;

#define TEST(name) do { \
	tests_run++; \
	printf("  TEST %s ... ", #name); \
	test_##name(); \
	tests_pass++; \
	printf("OK\n"); \
} while (0)

#define CHECK(cond) do { \
	if (!(cond)) { \
		printf("FAIL [%s:%d] %s\n", __FILE__, __LINE__, #cond); \
		tests_fail++; \
		return; \
	} \
} while (0)

static const char POLICY[] =
	"{\"policy\":{\"epoch\":9},\"intercept_scripts\":[]}";

static char wrapped[4096];

/* JSON-escape for the blob value (the blob is a STRING member) */
static void jesc_to(char *dst, size_t cap, const char *s)
{
	size_t o = 0;

	for (; *s != '\0' && o + 7 < cap; s++) {
		if (*s == '"' || *s == '\\') {
			dst[o++] = '\\';
			dst[o++] = *s;
		} else {
			dst[o++] = *s;
		}
	}
	dst[o] = '\0';
}

/* the wrapper with a given signature over a given blob */
static int build_signed(const char *sig_b64, const char *blob,
	size_t cap)
{
	char esc[1024];
	int n;

	jesc_to(esc, sizeof(esc), blob);
	n = snprintf(wrapped, cap,
		"{\"sig\":{\"alg\":\"ed25519\","
		"\"key_id\":\"test\",\"sig\":\"%s\"},"
		"\"blob\":\"%s\"}",
		sig_b64, esc);

	return n > 0 && (size_t)n < cap ? 0 : -1;
}

static void test_bare_passthrough(void)
{
	char blob[512];
	char reason[64];

	setenv("RETRACE_SUPERVISOR_PUBKEY", PUB, 1);
	(void)retrace_policy_sig_init();
	CHECK(retrace_policy_sig_check(POLICY, blob, sizeof(blob),
		reason, sizeof(reason)) == 0);
	CHECK(strcmp(blob, POLICY) == 0);
}

static void test_wrapper_without_pin_passes(void)
{
	char blob[512];
	char reason[64];

	/* fresh process state: run this before pinning tests order
	 * matters; main() runs this second with the env REMOVED
	 */
	unsetenv("RETRACE_SUPERVISOR_PUBKEY");
	CHECK(build_signed("AAAA", POLICY, sizeof(wrapped)) == 0);
	CHECK(retrace_policy_sig_check(wrapped, blob, sizeof(blob),
		reason, sizeof(reason)) == 1);
	CHECK(strcmp(blob, POLICY) == 0);
}

static void test_signed_accept(void)
{
	char b64[256];
	char blob[512];
	char reason[64];

	CHECK(sign_b64(POLICY, b64, sizeof(b64)) == 0);

	setenv("RETRACE_SUPERVISOR_PUBKEY", PUB, 1);
	CHECK(retrace_policy_sig_init() == 0);
	CHECK(retrace_policy_sig_pinned());
	CHECK(build_signed(b64, POLICY, sizeof(wrapped)) == 0);
	CHECK(retrace_policy_sig_check(wrapped, blob, sizeof(blob),
		reason, sizeof(reason)) == 1);
	CHECK(strcmp(blob, POLICY) == 0);
}

static void test_tampered_rejected(void)
{
	char blob[512];
	char reason[64];
	char bad[4096];

	/* the same signature over a DIFFERENT blob: must fail */
	{
		const char *alt =
			"{\"policy\":{\"epoch\":99},"
			"\"intercept_scripts\":[]}";
		const char *sig = strstr(wrapped, "\"sig\":\"");

		CHECK(sig != NULL);
		{
			const char *sig_end = strchr(sig + 7, '"');
			char sig_val[256];
			size_t n;

			CHECK(sig_end != NULL);
			n = (size_t)(sig_end - (sig + 7));
			CHECK(n < sizeof(sig_val));
			memcpy(sig_val, sig + 7, n);
			sig_val[n] = '\0';
			CHECK(build_signed(sig_val, alt,
				sizeof(wrapped)) == 0);
			snprintf(bad, sizeof(bad), "%s", wrapped);
		}
	}
	CHECK(retrace_policy_sig_check(bad, blob, sizeof(blob),
		reason, sizeof(reason)) == -1);
	CHECK(strstr(reason, "signature invalid") != NULL);
}

static void test_partial_wrapper_rejected(void)
{
	char blob[512];
	char reason[64];
	char w[512];

	{
		char esc[1024];

		jesc_to(esc, sizeof(esc), POLICY);
		snprintf(w, sizeof(w), "{\"blob\":\"%s\"}", esc);
	}
	CHECK(retrace_policy_sig_check(w, blob, sizeof(blob),
		reason, sizeof(reason)) == -1);
	CHECK(strstr(reason, "partial") != NULL);
}

int main(void)
{
	printf("policy signature tests:\n");
	/* order: no-pin first (state is process-global) */
	TEST(wrapper_without_pin_passes);
	TEST(bare_passthrough);
	TEST(signed_accept);
	TEST(tampered_rejected);
	TEST(partial_wrapper_rejected);

	printf("%d tests: %d pass, %d fail\n", tests_run, tests_pass,
		tests_fail);
	return tests_fail == 0 ? 0 : 1;
}
