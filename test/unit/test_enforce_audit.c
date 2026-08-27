/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Audited enforcement artifacts (TODO.beyond-libc/01 P2): the
 * chain that answers "which filter was in force when". Append
 * builds the chain; verify replays it; any tamper (reordered
 * line, edited field, torn tail) fails verification; a broken
 * chain refuses further appends (fail-closed).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "artifact_audit.h"

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

static char trail[256];

static void fresh_trail(void)
{
	unlink(trail);
}

static void test_append_and_verify(void)
{
	char digest[ENFORCE_DIGEST_HEX_MAX];
	const char *alg = NULL;
	char head[ENFORCE_DIGEST_HEX_MAX];
	long n;
	int i;

	fresh_trail();
	CHECK(enforce_spec_digest("{\"spec\":1}", 10, digest, &alg) == 0);
	CHECK(alg != NULL && alg[0] != '\0');
	for (i = 0; i < 3; i++)
		CHECK(enforce_audit_append(trail, 1000 + i, 42 + i,
			digest, alg, "landlock+seccomp",
			(char *[]){"/bin/true", NULL}) == 0);
	n = enforce_audit_verify(trail, head);
	CHECK(n == 3);
	CHECK(head[0] != '\0');
}

static void test_tampered_field_fails(void)
{
	FILE *f;
	char buf[8192];
	size_t n;
	char digest[ENFORCE_DIGEST_HEX_MAX];

	fresh_trail();
	CHECK(enforce_spec_digest("x", 1, digest, NULL) == 0);
	CHECK(enforce_audit_append(trail, 1, 1, digest, "sha256",
		"landlock", (char *[]){"/bin/ls", NULL}) == 0);
	/* edit one field of the committed record */
	f = fopen(trail, "r");
	CHECK(f != NULL);
	n = fread(buf, 1, sizeof(buf) - 1, f);
	fclose(f);
	buf[n] = '\0';
	{
		char *pid = strstr(buf, "\"pid\":1");

		CHECK(pid != NULL);
		pid[6] = '9';		/* pid 1 -> 9 */
	}
	f = fopen(trail, "w");
	CHECK(f != NULL);
	fwrite(buf, 1, n, f);
	fclose(f);
	CHECK(enforce_audit_verify(trail, NULL) == -2);
}

static void test_torn_tail_fails(void)
{
	FILE *f;
	char buf[8192];
	size_t n;
	char digest[ENFORCE_DIGEST_HEX_MAX];

	fresh_trail();
	CHECK(enforce_spec_digest("y", 1, digest, NULL) == 0);
	CHECK(enforce_audit_append(trail, 1, 7, digest, "fnv1a64",
		"seccomp", (char *[]){"/bin/true", NULL}) == 0);
	f = fopen(trail, "r");
	CHECK(f != NULL);
	n = fread(buf, 1, sizeof(buf) - 1, f);
	fclose(f);
	f = fopen(trail, "w");
	CHECK(f != NULL);
	fwrite(buf, 1, n - 5, f);	/* drop the last 5 bytes */
	fclose(f);
	CHECK(enforce_audit_verify(trail, NULL) == -2);
}

static void test_append_refuses_after_tamper(void)
{
	FILE *f;
	char digest[ENFORCE_DIGEST_HEX_MAX];

	fresh_trail();
	CHECK(enforce_spec_digest("z", 1, digest, NULL) == 0);
	CHECK(enforce_audit_append(trail, 1, 1, digest, NULL,
		"landlock", NULL) == 0);
	/* break the sole line */
	f = fopen(trail, "w");
	CHECK(f != NULL);
	fputs("{\"garbage\":1}\n", f);
	fclose(f);
	CHECK(enforce_audit_append(trail, 2, 2, digest, NULL,
		"landlock", NULL) == -1);
}

static void test_missing_file_verifies_empty(void)
{
	fresh_trail();
	CHECK(enforce_audit_verify(trail, NULL) == 0);
}

static void test_digest_stable_and_sensitive(void)
{
	char a[ENFORCE_DIGEST_HEX_MAX], b[ENFORCE_DIGEST_HEX_MAX];

	CHECK(enforce_spec_digest("same", 4, a, NULL) == 0);
	CHECK(enforce_spec_digest("same", 4, b, NULL) == 0);
	CHECK(strcmp(a, b) == 0);
	CHECK(enforce_spec_digest("same2", 5, b, NULL) == 0);
	CHECK(strcmp(a, b) != 0);
}

static void test_signed_records(void)
{
	char digest[ENFORCE_DIGEST_HEX_MAX];
	char head[ENFORCE_DIGEST_HEX_MAX];

	fresh_trail();
	CHECK(enforce_spec_digest("s", 1, digest, NULL) == 0);
	if (!enforce_audit_signing() &&
	    enforce_audit_set_key("test/fixtures/audit_ed25519_key.pem") != 0) {
		printf("SKIP (built without OpenSSL) ");
		return;
	}
	CHECK(enforce_audit_append(trail, 1, 11, digest, NULL,
		"landlock", (char *[]){"/bin/true", NULL}) == 0);
	/* the record carries a signature */
	{
		FILE *f = fopen(trail, "r");
		char buf[4096];
		size_t n;

		CHECK(f != NULL);
		n = fread(buf, 1, sizeof(buf) - 1, f);
		fclose(f);
		buf[n] = '\0';
		CHECK(strstr(buf, "\"sig\":\"") != NULL);
	}
	/* verify WITHOUT the pubkey: chain still ok */
	CHECK(enforce_audit_verify(trail, head) == 1);
	/* verify WITH the pubkey: signature must hold */
	CHECK(enforce_audit_set_pubkey(
		"test/fixtures/audit_ed25519_pub.pem") == 0);
	CHECK(enforce_audit_verify(trail, NULL) == 1);
	/* flip one signed byte: signature must fail */
	{
		FILE *f = fopen(trail, "r");
		char buf[4096];
		size_t n;
		char *pid;

		CHECK(f != NULL);
		n = fread(buf, 1, sizeof(buf) - 1, f);
		fclose(f);
		buf[n] = '\0';
		pid = strstr(buf, "\"pid\":11");
		CHECK(pid != NULL);
		pid[6] = '2';
		f = fopen(trail, "w");
		CHECK(f != NULL);
		fwrite(buf, 1, n, f);
		fclose(f);
	}
	CHECK(enforce_audit_verify(trail, NULL) == -2);
}

int main(void)
{
	snprintf(trail, sizeof(trail), "%s",
		"/tmp/.retrace-audit-test.jsonl");

	printf("enforce artifact audit tests:\n");
	TEST(append_and_verify);
	TEST(tampered_field_fails);
	TEST(torn_tail_fails);
	TEST(append_refuses_after_tamper);
	TEST(missing_file_verifies_empty);
	TEST(digest_stable_and_sensitive);
	TEST(signed_records);

	printf("%d tests: %d pass, %d fail\n", tests_run, tests_pass,
		tests_fail);
	return tests_fail == 0 ? 0 : 1;
}
