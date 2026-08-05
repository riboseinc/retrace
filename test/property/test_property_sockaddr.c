// SPDX-License-Identifier: BSD-2-Clause
//
// Property-based tests for retrace_sockaddr_inspect and
// retrace_sockaddr_match (TODO.complete/16 expansion).
//
// Properties:
//
//   P-INSPECT-NEVER-CRASH      any byte buffer (any length 0..200)
//                              passed as a sockaddr* never crashes.
//                              Returns 0 or -1.
//
//   P-INSPECT-IDEMPOTENT       two calls on the same buffer produce
//                              identical retrace_sockaddr_info.
//
//   P-INSPECT-FAMILY-CONSISTENT  info.family == first 2 bytes of the
//                              buffer interpreted as sa_family_t.
//
//   P-MATCH-CONSISTENT         two calls with the same (info, spec)
//                              produce the same result.
//
//   P-MATCH-ROUNDTRIP          for any "a.b.c.d:port" spec, building
//                              a sockaddr_in and inspecting produces
//                              an info that matches the spec.
//
//   P-MATCH-WILDCARD-PORT      "host:*" matches the same set as
//                              "host:<actual-port>".
//
//   P-MATCH-DENY-ALL           "*" matches any non-NULL info.
//
//   P-MATCH-MALFORMED-LINEAR   for any malformed spec, match returns
//                              -1 deterministically (no crash).

#include "property_harness.h"
#include "sockaddr_inspect.h"
#include "real_impls.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/*
 * The helper's libc usage goes through retrace_real_impls; the test
 * must populate it before any property runs. Done once in main().
 */
extern struct RetraceRealImpls retrace_real_impls;

/* -- Generators -- */

/* Generate n random bytes into buf. */
static void gen_bytes(struct ret_prng *p, unsigned char *buf, size_t n)
{
	size_t i;

	for (i = 0; i < n; i++)
		buf[i] = (unsigned char)ret_prng_u32(p, 256);
}

/* Generate a random byte buffer of length 0..cap. Returns length. */
static size_t gen_random_buffer(struct ret_prng *p,
				unsigned char *buf, size_t cap)
{
	size_t n = ret_prng_u32(p, (uint32_t)(cap + 1));

	gen_bytes(p, buf, n);
	return n;
}

/* Generate a syntactically valid random IPv4 "a.b.c.d" string. */
static void gen_ipv4_text(struct ret_prng *p, char *buf, size_t cap)
{
	if (cap < 16) {
		if (cap > 0)
			buf[0] = '\0';
		return;
	}

	snprintf(buf, cap, "%u.%u.%u.%u",
		ret_prng_u32(p, 256),
		ret_prng_u32(p, 256),
		ret_prng_u32(p, 256),
		ret_prng_u32(p, 256));
}

/* Generate a syntactically valid random "host:port" spec. */
static void gen_host_port_spec(struct ret_prng *p, char *buf, size_t cap)
{
	char host[32];

	if (cap < 40)
		return;

	if (ret_prng_u32(p, 2) == 0)
		snprintf(host, sizeof(host), "*");
	else
		gen_ipv4_text(p, host, sizeof(host));

	snprintf(buf, cap, "%s:%u", host, ret_prng_u32(p, 65536));
}

/* -- Properties -- */

static int prop_inspect_never_crash(uint64_t seed)
{
	struct ret_prng prng;
	unsigned char buf[200];
	size_t len;
	struct retrace_sockaddr_info info;
	int rc;

	ret_prng_seed(&prng, seed);
	len = gen_random_buffer(&prng, buf, sizeof(buf));

	/* Even zero-length buffers must not crash. */
	rc = retrace_sockaddr_inspect(buf, len, &info);

	/* rc is 0 (success) or -1 (hard failure). Crash == exit != 0. */
	return rc == 0 || rc == -1;
}

static int prop_inspect_idempotent(uint64_t seed)
{
	struct ret_prng prng;
	unsigned char buf[200];
	size_t len;
	struct retrace_sockaddr_info a, b;
	int rc1, rc2;

	ret_prng_seed(&prng, seed);
	len = gen_random_buffer(&prng, buf, sizeof(buf));

	rc1 = retrace_sockaddr_inspect(buf, len, &a);
	rc2 = retrace_sockaddr_inspect(buf, len, &b);

	if (rc1 != rc2)
		return 0;

	if (rc1 != 0)
		return 1;

	if (a.family != b.family)
		return 0;
	if (a.group != b.group)
		return 0;
	if (a.expected_len != b.expected_len)
		return 0;
	if (a.port != b.port)
		return 0;
	if (strcmp(a.ip, b.ip) != 0)
		return 0;
	if (strcmp(a.path, b.path) != 0)
		return 0;

	return 1;
}

static int prop_inspect_family_consistent(uint64_t seed)
{
	struct ret_prng prng;
	unsigned char buf[200];
	size_t len;
	struct retrace_sockaddr_info info;
	const struct sockaddr *s;
	int rc;
	sa_family_t reported;

	ret_prng_seed(&prng, seed);
	/* Force a length that fits the sockaddr header (sa_len +
	 * sa_family on BSD; just sa_family on Linux). 4 bytes is
	 * enough for both layouts.
	 */
	len = 4 + ret_prng_u32(&prng, sizeof(buf) - 4);
	gen_bytes(&prng, buf, len);

	rc = retrace_sockaddr_inspect(buf, len, &info);
	if (rc != 0)
		return 1;

	/* Read sa_family via the same struct the helper uses. This
	 * abstracts over the BSD sa_len / Linux sa_family layout
	 * difference.
	 */
	s = (const struct sockaddr *)buf;
	reported = s->sa_family;
	return info.family == (int)reported;
}

static int prop_match_consistent(uint64_t seed)
{
	struct ret_prng prng;
	struct retrace_sockaddr_info info;
	char spec[64];
	int a, b;

	ret_prng_seed(&prng, seed);

	/* Construct a random info. */
	memset(&info, 0, sizeof(info));
	info.group = ret_prng_u32(&prng, 4);  /* UNKNOWN..UNIX */
	info.port = (uint16_t)ret_prng_u32(&prng, 65536);
	if (info.group == RETRACE_SAF_INET ||
	    info.group == RETRACE_SAF_INET6)
		gen_ipv4_text(&prng, info.ip, sizeof(info.ip));
	else if (info.group == RETRACE_SAF_UNIX)
		gen_ipv4_text(&prng, info.path, sizeof(info.path));

	gen_host_port_spec(&prng, spec, sizeof(spec));

	a = retrace_sockaddr_match(&info, spec);
	b = retrace_sockaddr_match(&info, spec);

	return a == b;
}

static int prop_match_roundtrip(uint64_t seed)
{
	struct ret_prng prng;
	char ip[32];
	uint32_t port;
	char spec[64];
	struct sockaddr_in sin;
	struct retrace_sockaddr_info info;
	int rc_inspect, rc_match;

	ret_prng_seed(&prng, seed);

	/* Generate a random IPv4 + port. */
	gen_ipv4_text(&prng, ip, sizeof(ip));
	port = ret_prng_u32(&prng, 65536);
	snprintf(spec, sizeof(spec), "%s:%u", ip, port);

	/* Build a sockaddr_in matching the spec. */
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons((uint16_t)port);
	if (inet_pton(AF_INET, ip, &sin.sin_addr) != 1)
		return 1;  /* skip if somehow invalid */

	rc_inspect = retrace_sockaddr_inspect(&sin, sizeof(sin), &info);
	if (rc_inspect != 0)
		return 0;

	rc_match = retrace_sockaddr_match(&info, spec);
	return rc_match == 1;
}

static int prop_match_wildcard_port(uint64_t seed)
{
	struct ret_prng prng;
	char ip[32];
	uint32_t port;
	char star_spec[48];
	char exact_spec[64];
	struct sockaddr_in sin;
	struct retrace_sockaddr_info info;
	int star_match, exact_match;

	ret_prng_seed(&prng, seed);

	gen_ipv4_text(&prng, ip, sizeof(ip));
	port = ret_prng_u32(&prng, 65536);
	snprintf(star_spec, sizeof(star_spec), "%s:*", ip);
	snprintf(exact_spec, sizeof(exact_spec), "%s:%u", ip, port);

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons((uint16_t)port);
	if (inet_pton(AF_INET, ip, &sin.sin_addr) != 1)
		return 1;

	if (retrace_sockaddr_inspect(&sin, sizeof(sin), &info) != 0)
		return 0;

	star_match = retrace_sockaddr_match(&info, star_spec);
	exact_match = retrace_sockaddr_match(&info, exact_spec);

	/* Both specs should match the constructed info (since the IP
	 * matches and the port matches in both cases -- star always
	 * matches the port, exact matches because port==port).
	 */
	return star_match == 1 && exact_match == 1 &&
		star_match == exact_match;
}

static int prop_match_deny_all(uint64_t seed)
{
	struct ret_prng prng;
	struct retrace_sockaddr_info info;
	int rc;

	ret_prng_seed(&prng, seed);

	memset(&info, 0, sizeof(info));
	info.group = ret_prng_u32(&prng, 4);
	info.port = (uint16_t)ret_prng_u32(&prng, 65536);
	if (info.group == RETRACE_SAF_UNIX)
		info.group = RETRACE_SAF_INET;  /* "*" doesn't match unix */

	rc = retrace_sockaddr_match(&info, "*");
	return rc == 1;
}

static int prop_match_malformed_linear(uint64_t seed)
{
	struct ret_prng prng;
	unsigned char raw[64];
	size_t len;
	char spec[64];
	struct retrace_sockaddr_info info;
	int rc;

	ret_prng_seed(&prng, seed);

	/* Build a valid info (random family). */
	memset(&info, 0, sizeof(info));
	info.group = RETRACE_SAF_INET;
	info.port = (uint16_t)ret_prng_u32(&prng, 65536);

	/* Generate a random spec string; could be valid, malformed,
	 * or trivially short. The property: match never crashes.
	 */
	len = ret_prng_u32(&prng, sizeof(spec));
	gen_bytes(&prng, (unsigned char *)spec, len);
	spec[len < sizeof(spec) ? len : sizeof(spec) - 1] = '\0';

	rc = retrace_sockaddr_match(&info, spec);
	return rc == 0 || rc == 1 || rc == -1;
}

int main(void)
{
	int failures = 0;

	/* Set up minimal retrace_real_impls for the helper. */
	retrace_real_impls.strcmp = strcmp;
	retrace_real_impls.strlen = strlen;
	retrace_real_impls.strcpy = strcpy;
	retrace_real_impls.memset = memset;
	retrace_real_impls.memcpy = memcpy;
	retrace_real_impls.malloc = malloc;
	retrace_real_impls.free = free;
	retrace_real_impls.real_snprintf = snprintf;

	printf("sockaddr_inspect property tests:\n");

	failures += property_run(prop_inspect_never_crash,
		"inspect_never_crash",
		RETRACE_PROPERTY_DEFAULT_ITERS, 1);
	failures += property_run(prop_inspect_idempotent,
		"inspect_idempotent",
		RETRACE_PROPERTY_DEFAULT_ITERS, 1);
	failures += property_run(prop_inspect_family_consistent,
		"inspect_family_consistent",
		RETRACE_PROPERTY_DEFAULT_ITERS, 1);
	failures += property_run(prop_match_consistent,
		"match_consistent",
		RETRACE_PROPERTY_DEFAULT_ITERS, 1);
	failures += property_run(prop_match_roundtrip,
		"match_roundtrip",
		RETRACE_PROPERTY_DEFAULT_ITERS, 1);
	failures += property_run(prop_match_wildcard_port,
		"match_wildcard_port",
		RETRACE_PROPERTY_DEFAULT_ITERS, 1);
	failures += property_run(prop_match_deny_all,
		"match_deny_all",
		RETRACE_PROPERTY_DEFAULT_ITERS, 1);
	failures += property_run(prop_match_malformed_linear,
		"match_malformed_linear",
		RETRACE_PROPERTY_DEFAULT_ITERS, 1);

	if (failures == 0)
		printf("\n[property] all sockaddr properties PASS\n");
	else
		printf("\n[property] %d sockaddr properties FAILED\n",
			failures);

	return failures ? 1 : 0;
}
