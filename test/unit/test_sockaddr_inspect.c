/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Unit tests for retrace_sockaddr_inspect and retrace_sockaddr_match.
 *
 * These do NOT load the v2 engine -- the helpers are pure C functions
 * that read sockaddr* and compare strings. We set up a minimal
 * retrace_real_impls so the helper's libc calls don't crash, then
 * exercise each family and match form.
 *
 * Part of TODO.complete/15 (network functions).
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <sys/socket.h>

#include "sockaddr_inspect.h"
#include "real_impls.h"

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

/* -- inspect: AF_INET -- */

static void test_inspect_ipv4_loopback(void)
{
	struct sockaddr_in sin = {0};
	struct retrace_sockaddr_info info;
	int rc;

	sin.sin_family = AF_INET;
	sin.sin_port = htons(443);
	inet_pton(AF_INET, "127.0.0.1", &sin.sin_addr);

	rc = retrace_sockaddr_inspect(&sin, sizeof(sin), &info);
	assert(rc == 0);
	assert(info.family == AF_INET);
	assert(info.group == RETRACE_SAF_INET);
	assert(info.expected_len == sizeof(sin));
	assert(info.port == 443);
	assert(strcmp(info.ip, "127.0.0.1") == 0);
}

static void test_inspect_ipv4_arbitrary(void)
{
	struct sockaddr_in sin = {0};
	struct retrace_sockaddr_info info;

	sin.sin_family = AF_INET;
	sin.sin_port = htons(8080);
	inet_pton(AF_INET, "10.0.0.1", &sin.sin_addr);

	assert(retrace_sockaddr_inspect(&sin, sizeof(sin), &info) == 0);
	assert(info.port == 8080);
	assert(strcmp(info.ip, "10.0.0.1") == 0);
}

/* -- inspect: AF_INET6 -- */

static void test_inspect_ipv6_loopback(void)
{
	struct sockaddr_in6 sin6 = {0};
	struct retrace_sockaddr_info info;

	sin6.sin6_family = AF_INET6;
	sin6.sin6_port = htons(443);
	inet_pton(AF_INET6, "::1", &sin6.sin6_addr);

	assert(retrace_sockaddr_inspect(&sin6, sizeof(sin6), &info) == 0);
	assert(info.family == AF_INET6);
	assert(info.group == RETRACE_SAF_INET6);
	assert(info.expected_len == sizeof(sin6));
	assert(info.port == 443);
	/* inet_pton -> ::1, our formatter produces "0:0:0:0:0:0:0:1" */
	assert(strcmp(info.ip, "0:0:0:0:0:0:0:1") == 0);
}

static void test_inspect_ipv6_link_local(void)
{
	struct sockaddr_in6 sin6 = {0};
	struct retrace_sockaddr_info info;

	sin6.sin6_family = AF_INET6;
	sin6.sin6_port = htons(1234);
	inet_pton(AF_INET6, "fe80::1", &sin6.sin6_addr);

	assert(retrace_sockaddr_inspect(&sin6, sizeof(sin6), &info) == 0);
	assert(strcmp(info.ip, "fe80:0:0:0:0:0:0:1") == 0);
	assert(info.port == 1234);
}

/* -- inspect: AF_UNIX -- */

static void test_inspect_unix_path(void)
{
	struct sockaddr_un sun = {0};
	struct retrace_sockaddr_info info;

	sun.sun_family = AF_UNIX;
	strncpy(sun.sun_path, "/var/run/test.sock", sizeof(sun.sun_path) - 1);

	assert(retrace_sockaddr_inspect(&sun, sizeof(sun), &info) == 0);
	assert(info.family == AF_UNIX);
	assert(info.group == RETRACE_SAF_UNIX);
	assert(info.expected_len == sizeof(sun));
	assert(strcmp(info.path, "/var/run/test.sock") == 0);
	assert(info.ip[0] == '\0');
}

static void test_inspect_unix_anonymous(void)
{
	struct sockaddr_un sun;
	struct retrace_sockaddr_info info;

	memset(&sun, 0, sizeof(sun));
	sun.sun_family = AF_UNIX;

	assert(retrace_sockaddr_inspect(&sun, sizeof(sun), &info) == 0);
	assert(info.group == RETRACE_SAF_UNIX);
	assert(info.path[0] == '\0');
}

/* -- inspect: edge cases -- */

static void test_inspect_null(void)
{
	struct retrace_sockaddr_info info;

	assert(retrace_sockaddr_inspect(NULL, 0, &info) == -1);
	assert(retrace_sockaddr_inspect(NULL, sizeof(struct sockaddr_in),
		&info) == -1);
}

static void test_inspect_null_out(void)
{
	struct sockaddr_in sin = {0};

	sin.sin_family = AF_INET;
	assert(retrace_sockaddr_inspect(&sin, sizeof(sin), NULL) == -1);
}

static void test_inspect_unknown_family(void)
{
	struct sockaddr_in sin = {0};
	struct retrace_sockaddr_info info;

	/* Use a family value that's unlikely to be valid. */
	sin.sin_family = (sa_family_t)0xFFFE;

	assert(retrace_sockaddr_inspect(&sin, sizeof(sin), &info) == 0);
	assert(info.group == RETRACE_SAF_UNKNOWN);
	assert(info.expected_len == 0);
}

static void test_inspect_short_buffer(void)
{
	struct sockaddr_in sin = {0};
	struct retrace_sockaddr_info info;

	sin.sin_family = AF_INET;
	/* addrlen smaller than sockaddr_in */
	assert(retrace_sockaddr_inspect(&sin, sizeof(sin) - 4, &info) == -1);
}

/* -- match: IPv4 forms -- */

static void test_match_ipv4_exact(void)
{
	struct retrace_sockaddr_info info = {0};

	info.group = RETRACE_SAF_INET;
	strcpy(info.ip, "10.0.0.1");
	info.port = 443;

	assert(retrace_sockaddr_match(&info, "10.0.0.1:443") == 1);
	assert(retrace_sockaddr_match(&info, "10.0.0.1:80") == 0);
	assert(retrace_sockaddr_match(&info, "10.0.0.2:443") == 0);
}

static void test_match_ipv4_wild_port(void)
{
	struct retrace_sockaddr_info info = {0};

	info.group = RETRACE_SAF_INET;
	strcpy(info.ip, "10.0.0.1");
	info.port = 443;

	assert(retrace_sockaddr_match(&info, "10.0.0.1:*") == 1);
	assert(retrace_sockaddr_match(&info, "10.0.0.2:*") == 0);
}

static void test_match_ipv4_wild_host(void)
{
	struct retrace_sockaddr_info info = {0};

	info.group = RETRACE_SAF_INET;
	strcpy(info.ip, "10.0.0.1");
	info.port = 443;

	assert(retrace_sockaddr_match(&info, "*:443") == 1);
	assert(retrace_sockaddr_match(&info, "*:80") == 0);
}

static void test_match_ipv4_deny_all(void)
{
	struct retrace_sockaddr_info info = {0};

	info.group = RETRACE_SAF_INET;
	strcpy(info.ip, "10.0.0.1");
	info.port = 443;

	assert(retrace_sockaddr_match(&info, "*:*") == 1);
	assert(retrace_sockaddr_match(&info, "*") == 1);
}

/* -- match: IPv6 forms -- */

static void test_match_ipv6_bracketed(void)
{
	struct retrace_sockaddr_info info = {0};

	info.group = RETRACE_SAF_INET6;
	strcpy(info.ip, "fe80:0:0:0:0:0:0:1");
	info.port = 443;

	assert(retrace_sockaddr_match(&info, "[fe80:0:0:0:0:0:0:1]:443") == 1);
	assert(retrace_sockaddr_match(&info, "[fe80:0:0:0:0:0:0:1]:*") == 1);
	assert(retrace_sockaddr_match(&info, "[fe80:0:0:0:0:0:0:2]:443") == 0);
}

static void test_match_ipv6_no_port(void)
{
	struct retrace_sockaddr_info info = {0};

	info.group = RETRACE_SAF_INET6;
	strcpy(info.ip, "0:0:0:0:0:0:0:1");
	info.port = 443;

	/* "[::1]" alone -- no port, no wildcard suffix. */
	assert(retrace_sockaddr_match(&info, "[0:0:0:0:0:0:0:1]") == 1);
}

/* -- match: AF_UNIX -- */

static void test_match_unix_path(void)
{
	struct retrace_sockaddr_info info = {0};

	info.group = RETRACE_SAF_UNIX;
	strcpy(info.path, "/var/run/x.sock");

	assert(retrace_sockaddr_match(&info, "/var/run/x.sock") == 1);
	assert(retrace_sockaddr_match(&info, "/var/run/y.sock") == 0);
	/* "host:port" spec never matches a unix sockaddr. */
	assert(retrace_sockaddr_match(&info, "*:443") == 0);
}

/* -- match: malformed -- */

static void test_match_malformed(void)
{
	struct retrace_sockaddr_info info = {0};

	info.group = RETRACE_SAF_INET;
	strcpy(info.ip, "10.0.0.1");
	info.port = 443;

	/* Missing close bracket. */
	assert(retrace_sockaddr_match(&info, "[10.0.0.1:443") == -1);
	/* Bracket but no closing colon-form. */
	assert(retrace_sockaddr_match(&info, "[10.0.0.1]extra") == -1);
	/* Non-numeric port. */
	assert(retrace_sockaddr_match(&info, "10.0.0.1:abc") == -1);
	/* Out-of-range port. */
	assert(retrace_sockaddr_match(&info, "10.0.0.1:99999") == -1);
}

static void test_match_null(void)
{
	struct retrace_sockaddr_info info = {0};

	assert(retrace_sockaddr_match(NULL, "10.0.0.1:443") == -1);
	assert(retrace_sockaddr_match(&info, NULL) == -1);
}

/* -- match: AF_INET vs AF_INET6 mismatch -- */

static void test_match_family_mismatch(void)
{
	struct retrace_sockaddr_info info = {0};

	info.group = RETRACE_SAF_INET;
	strcpy(info.ip, "10.0.0.1");
	info.port = 443;

	/* IPv6-bracketed spec should not match an IPv4 info. */
	assert(retrace_sockaddr_match(&info, "[10.0.0.1]:443") == 0);
}

int main(void)
{
	/* Minimal real_impls setup. */
	retrace_real_impls.strcmp = strcmp;
	retrace_real_impls.strlen = strlen;
	retrace_real_impls.strcpy = strcpy;
	retrace_real_impls.memset = memset;
	retrace_real_impls.memcpy = memcpy;
	retrace_real_impls.malloc = malloc;
	retrace_real_impls.free = free;
	retrace_real_impls.real_snprintf = snprintf;

	printf("sockaddr_inspect tests:\n");

	printf("  -- inspect: AF_INET --\n");
	TEST(inspect_ipv4_loopback);
	TEST(inspect_ipv4_arbitrary);

	printf("  -- inspect: AF_INET6 --\n");
	TEST(inspect_ipv6_loopback);
	TEST(inspect_ipv6_link_local);

	printf("  -- inspect: AF_UNIX --\n");
	TEST(inspect_unix_path);
	TEST(inspect_unix_anonymous);

	printf("  -- inspect: edge cases --\n");
	TEST(inspect_null);
	TEST(inspect_null_out);
	TEST(inspect_unknown_family);
	TEST(inspect_short_buffer);

	printf("  -- match: IPv4 forms --\n");
	TEST(match_ipv4_exact);
	TEST(match_ipv4_wild_port);
	TEST(match_ipv4_wild_host);
	TEST(match_ipv4_deny_all);

	printf("  -- match: IPv6 forms --\n");
	TEST(match_ipv6_bracketed);
	TEST(match_ipv6_no_port);

	printf("  -- match: AF_UNIX --\n");
	TEST(match_unix_path);

	printf("  -- match: malformed --\n");
	TEST(match_malformed);
	TEST(match_null);
	TEST(match_family_mismatch);

	printf("\nPass: %d, Fail: %d (of %d)\n",
		tests_pass, tests_fail, tests_run);
	return tests_fail == 0 ? 0 : 1;
}
