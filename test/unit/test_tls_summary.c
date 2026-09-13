/*
 * Unit: the pure TLS plaintext summarizer (TODO.impl/13).
 * Standalone by the card-06 law: this TU compiles ONLY
 * tls_summary.c -- no engine linkage, no interposition.
 *
 *   ctest -R unit-test-tls-summary
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "tls_summary.h"

/* Always-on check. assert() alone is compiled out by NDEBUG.
 * See feedback_assert_with_side_effects.md.
 */
static int tests_fail;

#define CHECK(cond) do { \
	if (!(cond)) { \
		printf("FAIL [%s:%d] %s\n", __FILE__, __LINE__, #cond); \
		tests_fail++; \
		return; \
	} \
} while (0)

#define CHECK_STR_EQ(got, want) do { \
	if (strcmp((got), (want)) != 0) { \
		printf("FAIL [%s:%d] \"%s\" != \"%s\"\n", \
			__FILE__, __LINE__, (got), (want)); \
		tests_fail++; \
		return; \
	} \
} while (0)

static void test_request_line(void)
{
	char out[256];
	size_t n;

	n = retrace_tls_first_line("GET /a/b?x=1 HTTP/1.1\r\nHost: h\r\n",
		29, out, sizeof(out));
	CHECK(n > 0);
	CHECK_STR_EQ(out, "GET /a/b?x=1 HTTP/1.1");
}

static void test_lf_only(void)
{
	char out[64];

	CHECK(retrace_tls_first_line("POST /submit HTTP/1.1\nbody", 26,
		out, sizeof(out)) > 0);
	CHECK_STR_EQ(out, "POST /submit HTTP/1.1");
}

static void test_binary_is_silent(void)
{
	char out[64];

	/* TLS-compressed/encrypted-looking payload: no printable
	 * line, no bytes out (the length rides as its own attr)
	 */
	CHECK(retrace_tls_first_line("\x16\x03\x01\x00\x05hello", 10,
		out, sizeof(out)) == 0);
	CHECK(out[0] == '\0');
}

static void test_bounded_by_caller_len(void)
{
	char out[64];
	char big[128];

	memset(big, 'A', sizeof(big));
	/* the scan must never read past the caller-declared length
	 * (TLS buffers are not NUL-terminated)
	 */
	CHECK(retrace_tls_first_line(big, 10, out, sizeof(out)) == 10);
	CHECK(strlen(out) == 10);
}

static void test_cap_respected(void)
{
	char out[16];

	CHECK(retrace_tls_first_line(
		"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA", 32, out,
		sizeof(out)) == 15);
	CHECK(strlen(out) == 15);
}

static void test_edge_args(void)
{
	char out[8];

	CHECK(retrace_tls_first_line(NULL, 5, out, sizeof(out)) == 0);
	CHECK(retrace_tls_first_line("abc", 3, NULL, 8) == 0);
	CHECK(retrace_tls_first_line("abc", 0, out, sizeof(out)) == 0);
}

int main(void)
{
	test_request_line();
	test_lf_only();
	test_binary_is_silent();
	test_bounded_by_caller_len();
	test_cap_respected();
	test_edge_args();
	printf("tls_summary: all checks passed\n");
	return tests_fail != 0;
}
