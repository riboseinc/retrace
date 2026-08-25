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
 * Policy acceptance rules (TODO.supervisor/05) against the real
 * validator: the fail-closed gates (malformed, missing header,
 * epoch regression, expiry, missing scripts) and the applied
 * swap (name cache rebuilt from the new tree).
 */

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "agent.h"
#include "config_cache.h"

static int failures;

#define CHECK(cond) do { \
	if (!(cond)) { \
		printf("FAIL %s:%d: %s\n", __func__, __LINE__, #cond); \
		failures++; \
	} \
} while (0)

static const char *GOOD =
	"{\"policy\":{\"epoch\":9},"
	"\"intercept_scripts\":["
	"{\"func_name\":\"open\",\"actions\":["
	"{\"action_name\":\"sandbox\",\"action_params\":"
	"{\"deny_paths\":[\"/etc/hosts\"]}},"
	"{\"action_name\":\"call_real\"}]}]}";

static void expect_refused(const char *payload, const char *needle)
{
	char reason[96];
	int rc;

	reason[0] = '\0';
	rc = retrace_agent_policy_apply(payload, reason, sizeof(reason));
	CHECK(rc == -1);
	CHECK(reason[0] != '\0');
	if (needle != NULL)
		CHECK(strstr(reason, needle) != NULL);
}

int main(void)
{
	char reason[96];

	/* the library constructor has already booted the core
	 * (constructor(101) in main.c); the config cache is live
	 */

	/* nothing applied yet: every gate refuses */
	expect_refused("not json", "malformed");
	expect_refused("{}", "policy header");
	expect_refused("{\"policy\":{}}", "epoch");
	expect_refused(
		"{\"policy\":{\"epoch\":1}}", "intercept_scripts");
	expect_refused(
		"{\"policy\":{\"epoch\":1,\"expires\":1000},"
		"\"intercept_scripts\":[]}", "expired");

	/* apply GOOD (epoch 9): accepted, cache rebuilt */
	reason[0] = '\0';
	CHECK(retrace_agent_policy_apply(GOOD, reason,
		sizeof(reason)) == 0);
	CHECK(reason[0] == '\0');
	CHECK(retrace_config_cache_lookup("open") != NULL);
	CHECK(retrace_config_cache_lookup("openat") == NULL);

	/* regression: an older epoch is refused even though the
	 * payload is otherwise valid (replay protection)
	 */
	expect_refused(
		"{\"policy\":{\"epoch\":3},"
		"\"intercept_scripts\":[]}", "regression");

	/* the held policy is still the applied one */
	CHECK(retrace_config_cache_lookup("open") != NULL);

	if (failures > 0) {
		printf("%d failure(s)\n", failures);
		return 1;
	}
	printf("PASS: agent policy gates\n");
	return 0;
}
