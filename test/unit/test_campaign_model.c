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
 * Unit: the campaign model (TODO.impl/09) -- STANDALONE. The
 * manifest is validated whole (a farm does not run half a
 * plan) and the matrix expands in a deterministic sample-major
 * order: for each sample, for each policy, for each repeat.
 * The order IS the spec: run indices in the summary table and
 * the journal's launch sequence must both be reproducible.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "model.h"

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

static char manifest_path[256];

static void write_manifest(const char *text)
{
	snprintf(manifest_path, sizeof(manifest_path),
		"/tmp/campaign-%ld.json", (long) getpid());
	{
		FILE *f = fopen(manifest_path, "w");

		if (f == NULL)
			return;
		fputs(text, f);
		fclose(f);
	}
}

static void cleanup_manifest(void)
{
	remove(manifest_path);
}

static void test_matrix_expansion_order(void)
{
	struct campaign c;
	char err[128];

	write_manifest(
		"{\"repeats\":2,"
		"\"ctl_sock\":\"/tmp/s\",\"journal\":\"/tmp/j\","
		"\"samples\":["
		"{\"name\":\"a\",\"argv\":[\"./a\"]},"
		"{\"name\":\"b\",\"argv\":[\"./b\",\"--x\"]}],"
		"\"policies\":["
		"{\"name\":\"p1\",\"path\":\"p1.json\"},"
		"{\"name\":\"p2\",\"path\":\"p2.json\"}]}");

	CHECK(campaign_load(manifest_path, &c, err, sizeof(err)) == 0);
	CHECK(c.n_cells == 8);	/* 2 samples x 2 policies x 2 repeats */

	/* sample-major, then policy, then repeat */
	CHECK(c.cells[0].sample == 0 && c.cells[0].policy == 0 &&
		c.cells[0].repeat == 0);
	CHECK(c.cells[1].sample == 0 && c.cells[1].policy == 0 &&
		c.cells[1].repeat == 1);
	CHECK(c.cells[2].sample == 0 && c.cells[2].policy == 1 &&
		c.cells[2].repeat == 0);
	CHECK(c.cells[3].sample == 0 && c.cells[3].policy == 1 &&
		c.cells[3].repeat == 1);
	CHECK(c.cells[4].sample == 1 && c.cells[4].policy == 0 &&
		c.cells[4].repeat == 0);
	CHECK(c.cells[7].sample == 1 && c.cells[7].policy == 1 &&
		c.cells[7].repeat == 1);

	/* argv carried */
	CHECK(c.samples[1].argc == 2);
	CHECK(strcmp(c.samples[1].argv[1], "--x") == 0);

	campaign_free(&c);
}

static void test_defaults(void)
{
	struct campaign c;
	char err[128];

	write_manifest(
		"{\"ctl_sock\":\"/tmp/s\",\"journal\":\"/tmp/j\","
		"\"samples\":[{\"name\":\"a\",\"argv\":[\"./a\"]}],"
		"\"policies\":[{\"name\":\"p\",\"path\":\"p.json\"}]}");

	CHECK(campaign_load(manifest_path, &c, err, sizeof(err)) == 0);
	CHECK(c.repeats == 1);
	CHECK(c.concurrency == 1);
	CHECK(c.timeout_sec == 30);
	CHECK(c.n_cells == 1);
	campaign_free(&c);
}

static void test_rejection_is_whole(void)
{
	struct campaign c;
	char err[128];

	/* missing samples */
	write_manifest(
		"{\"ctl_sock\":\"s\",\"journal\":\"j\","
		"\"policies\":[{\"name\":\"p\",\"path\":\"p\"}]}");
	CHECK(campaign_load(manifest_path, &c, err, sizeof(err)) != 0);
	CHECK(strstr(err, "samples") != NULL);

	/* sample without argv */
	write_manifest(
		"{\"ctl_sock\":\"s\",\"journal\":\"j\","
		"\"samples\":[{\"name\":\"a\"}],"
		"\"policies\":[{\"name\":\"p\",\"path\":\"p\"}]}");
	CHECK(campaign_load(manifest_path, &c, err, sizeof(err)) != 0);
	CHECK(strstr(err, "argv") != NULL);

	/* policy without path */
	write_manifest(
		"{\"ctl_sock\":\"s\",\"journal\":\"j\","
		"\"samples\":[{\"name\":\"a\",\"argv\":[\"./a\"]}],"
		"\"policies\":[{\"name\":\"p\"}]}");
	CHECK(campaign_load(manifest_path, &c, err, sizeof(err)) != 0);
	CHECK(strstr(err, "path") != NULL);

	/* unparseable file */
	write_manifest("{not json");
	CHECK(campaign_load(manifest_path, &c, err, sizeof(err)) != 0);
}

int
main(void)
{
	printf("campaign model (TODO.impl/09)\n");

	TEST(matrix_expansion_order);
	TEST(defaults);
	TEST(rejection_is_whole);

	cleanup_manifest();
	printf("%d run, %d pass, %d fail\n",
		tests_run, tests_pass, tests_fail);
	return tests_fail == 0 ? 0 : 1;
}
