/*
 * Copyright (c) 2017, [Ribose Inc](https://www.ribose.com).
 *
 * BSD-2-Clause license -- see LICENSE for details.
 */

/*
 * Unit tests for the tolerant log scanner (TODO.next-level/02).
 *
 * retrace writes ONE JSON array document with leading-comma
 * emission; a crashed trace leaves the tail truncated. The
 * scanner must yield every complete entry from either shape --
 * and from JSONL, the streaming direction.
 *
 * Covers:
 *   - a well-formed array document
 *   - retrace's actual emission shape ("[\n{...},\n{...}\n]\n")
 *   - a truncated final object (crash mid-write)
 *   - JSONL input
 *   - braces, quotes, and escapes inside string values
 *   - BOM and CRLF noise between entries
 *   - complete-but-corrupt objects counted as skipped
 *   - NULL safety
 */

#include "parson.h"
#include "stream.h"

#include <stdio.h>
#include <string.h>

static int tests_run;
static int tests_pass;
static int tests_fail;

#define TEST(name)                               \
	do {                                     \
		tests_run++;                     \
		printf("  TEST %s ... ", #name); \
		test_##name();                   \
		tests_pass++;                    \
		printf("OK\n");                  \
	} while (0)

#define CHECK(cond)                                                             \
	do {                                                                    \
		if (!(cond)) {                                                  \
			printf("FAIL [%s:%d] %s\n", __FILE__, __LINE__, #cond); \
			tests_fail++;                                           \
			return;                                                 \
		}                                                               \
	} while (0)

struct Capture {
	size_t count;
	char   funcs[8][32];
};

static void
capture(JSON_Object *entry, void *ctx)
{
	struct Capture *cap = (struct Capture *) ctx;
	const char *func;

	if (cap->count < 8) {
		func =
		  json_object_get_string(json_object_get_object(entry, "message"), "func");
		if (func != NULL)
			snprintf(cap->funcs[cap->count], sizeof(cap->funcs[0]), "%s", func);
	}
	cap->count++;
}

static size_t
run(const char *text, struct Capture *cap, size_t *skipped)
{
	memset(cap, 0, sizeof(*cap));
	if (skipped != NULL)
		*skipped = 0;
	return corr_stream_scan(text, strlen(text), capture, cap, skipped);
}

/*
 * Slot ring: several e() results are used in one snprintf, so a
 * single static buffer would alias them all to the last call.
 */
static const char *
e(const char *func, double tid)
{
	static char bufs[4][256];
	static int  slot;

	slot = (slot + 1) % 4;
	snprintf(bufs[slot],
		 sizeof(bufs[slot]),
		 "{ \"time\": 1755580000, \"pid\": 1, \"tid\": %ld, "
		 "\"module\": \"retrace\", \"severity\": \"INFO\", "
		 "\"message\": { \"func\": \"%s\" } }",
		 (long) tid,
		 func);
	return bufs[slot];
}

static void
test_array_document(void)
{
	struct Capture cap;
	char	       doc[2048];

	snprintf(
	  doc, sizeof(doc), "[\n%s,\n%s,\n%s\n]\n", e("open", 3), e("close", 4), e("read", 5));
	CHECK(run(doc, &cap, NULL) == 3);
	CHECK(strcmp(cap.funcs[0], "open") == 0);
	CHECK(strcmp(cap.funcs[1], "close") == 0);
	CHECK(strcmp(cap.funcs[2], "read") == 0);
}

static void
test_retrace_emission_shape(void)
{
	/*
	 * The exact leading-comma shape retrace_logger_log_json
	 * writes: first entry bare, later entries with "," prefix.
	 */
	struct Capture cap;
	char	       doc[2048];

	snprintf(doc,
		 sizeof(doc),
		 "[\n%s\n,\n%s\n,\n%s\n]\n",
		 e("first", 1),
		 e("second", 2),
		 e("third", 3));
	CHECK(run(doc, &cap, NULL) == 3);
	CHECK(strcmp(cap.funcs[0], "first") == 0);
	CHECK(strcmp(cap.funcs[2], "third") == 0);
}

static void
test_truncated_tail(void)
{
	struct Capture cap;
	char	       doc[2048];
	size_t	       cut;

	/*
	 * Crash mid-write: the last object never closes, and the
	 * array bracket never arrives.
	 */
	snprintf(doc,
		 sizeof(doc),
		 "[\n%s,\n%s,\n{ \"time\": 1755580000",
		 e("kept1", 1),
		 e("kept2", 2));
	cut = strlen(doc);
	doc[cut] = '\0';
	CHECK(run(doc, &cap, NULL) == 2);
	CHECK(strcmp(cap.funcs[0], "kept1") == 0);
	CHECK(strcmp(cap.funcs[1], "kept2") == 0);
}

static void
test_jsonl(void)
{
	struct Capture cap;
	char	       doc[2048];

	snprintf(doc, sizeof(doc), "%s\n%s\n%s\n", e("l1", 1), e("l2", 2), e("l3", 3));
	CHECK(run(doc, &cap, NULL) == 3);
	CHECK(strcmp(cap.funcs[2], "l3") == 0);
}

static void
test_braces_inside_strings(void)
{
	struct Capture cap;
	char	       doc[1024];

	snprintf(doc,
		 sizeof(doc),
		 "{ \"message\": { \"func\": \"a{b\\\"c,d}e\" } }\n"
		 "{ \"message\": { \"func\": \"ok\" } }\n");
	CHECK(run(doc, &cap, NULL) == 2);
	CHECK(strcmp(cap.funcs[0], "a{b\"c,d}e") == 0);
	CHECK(strcmp(cap.funcs[1], "ok") == 0);
}

static void
test_noise_tolerated(void)
{
	struct Capture cap;
	char	       doc[1024];

	snprintf(doc, sizeof(doc), "\xEF\xBB\xBF \r\n %s \r\n , %s", e("one", 1), e("two", 2));
	CHECK(run(doc, &cap, NULL) == 2);
	CHECK(strcmp(cap.funcs[1], "two") == 0);
}

static void
test_corrupt_object_skipped(void)
{
	struct Capture cap;
	size_t	       skipped = 99;

	CHECK(run("{oops} {\"message\": {}}", &cap, &skipped) == 1);
	CHECK(skipped == 1);
	CHECK(run("not json at all", &cap, NULL) == 0);
	CHECK(run("", &cap, NULL) == 0);
}

static void
test_null_safety(void)
{
	struct Capture cap;

	CHECK(corr_stream_scan(NULL, 10, capture, &cap, NULL) == 0);
	CHECK(corr_stream_scan("{}", 2, NULL, &cap, NULL) == 0);
}

int
main(void)
{
	printf("correlate stream tests\n");
	TEST(array_document);
	TEST(retrace_emission_shape);
	TEST(truncated_tail);
	TEST(jsonl);
	TEST(braces_inside_strings);
	TEST(noise_tolerated);
	TEST(corrupt_object_skipped);
	TEST(null_safety);
	printf("%d run, %d pass, %d fail\n", tests_run, tests_pass, tests_fail);
	return (tests_fail == 0) ? 0 : 1;
}
